#include "cdvis.hpp"

#include <jae.hpp>
#include <Profiler.hpp>
#include <Input.hpp>
#include <Util.hpp>
#include <Window.hpp>
#include <Graphics.hpp>
#include <CommandQueue.hpp>
#include <CommandList.hpp>
#include <AssetDatabase.hpp>
#include <Scene.hpp>

#include <MeshRenderer.hpp>
#include <Mesh.hpp>
#include <Camera.hpp>
#include <Light.hpp>
#include <Shader.hpp>
#include <Material.hpp>
#include <Texture.hpp>
#include <ConstantBuffer.hpp>
#include <SpriteBatch.hpp>
#include <Font.hpp>

#include "VRUtil.hpp"
#include "Dicom.hpp"
#include "VolumeRenderer.hpp"
#include "VRCamera.hpp"

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "runtimeobject.lib")
#pragma comment(lib, "propsys.lib")
#include "CDialogEventHandler.hpp"

using namespace DirectX;
using namespace Microsoft::WRL;
using namespace std;

shared_ptr<Camera> windowCamera;
shared_ptr<VRCamera> vrCamera;
shared_ptr<Scene> scene;
shared_ptr<Font> arial;
shared_ptr<VolumeRenderer> volume;

// VR
vr::IVRSystem *hmd;
vr::IVRRenderModels *vrRenderModels;
vr::TrackedDevicePose_t vrTrackedDevices[vr::k_unMaxTrackedDeviceCount];

wchar_t pbuf[1024]; // performance overlay text
float frameTimes[128];
unsigned int frameTimeIndex;
bool debugDraw = false;
bool wireframe = false;
float z = -2.0f;

#define F2D(x) (int)x, (int)((x - floor(x)) * 10.0f)

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
	Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_SINGLETHREADED);
	if (FAILED(initialize)) {
		OutputDebugString(L"Failed to initialize COM!\n");
		return -1;
	}

	HWND hWnd = JaeCreateWindow(L"CDvis", 1600, 900, 3);
	Graphics::GetWindow()->SetVSync(false);

	cdvis* game = new cdvis();
	game->Initialize();

	JaeMsgLoop(game);

	if (hmd) {
		vr::VR_Shutdown();
		hmd = NULL;
	}
	JaeDestroy();

	delete game;
	return 0;
}

bool cdvis::InitializeVR() {
	vr::EVRInitError eError = vr::VRInitError_None;
	hmd = vr::VR_Init(&eError, vr::VRApplication_Scene);
	if (eError != vr::VRInitError_None) {
		OutputDebugStringA("Failed to initialize VR: ");
		OutputDebugStringA(vr::VR_GetVRInitErrorAsEnglishDescription(eError));
		OutputDebugStringA("\n");
		return false;
	}

	vrRenderModels = (vr::IVRRenderModels*)vr::VR_GetGenericInterface(vr::IVRRenderModels_Version, &eError);
	if (!vrRenderModels) {
		vr::VR_Shutdown();
		OutputDebugf(L"Failed to get render model interface: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
		return false;
	}

	if (!vr::VRCompositor()) {
		OutputDebugString(L"Failed to initialize compositor.\n");
		return false;
	}

	unsigned int w, h;
	hmd->GetRecommendedRenderTargetSize(&w, &h);

	vrCamera = scene->AddObject<VRCamera>(L"VR Camera");
	vrCamera->LocalPosition(0, 0, z);
	vrCamera->CreateCameras(w, h);
	vrCamera->UpdateCameras(hmd);

	return true;
}

void cdvis::Initialize() {
	AssetDatabase::LoadAssets(L"core.asset");
	AssetDatabase::LoadAssets(L"assets.asset");

	scene = make_shared<Scene>();

	arial = AssetDatabase::GetAsset<Font>(L"arial");
	arial->GetTexture()->Upload();

	windowCamera = scene->AddObject<Camera>(L"Window Camera");
	windowCamera->LocalPosition(0, 0, z);

	shared_ptr<Mesh> cubeMesh = shared_ptr<Mesh>(new Mesh(L"Cube"));
	cubeMesh->LoadCube(.5f);
	cubeMesh->UploadStatic();

	shared_ptr<Mesh> vrCubeMesh = shared_ptr<Mesh>(new Mesh(L"Cube Mesh"));
	vrCubeMesh->LoadCube(.1f);
	vrCubeMesh->UploadStatic();

	volume = scene->AddObject<VolumeRenderer>(L"Volume");
	volume->mCubeMesh = cubeMesh;
	volume->LocalPosition(0, 1, 0);
	volume->mShader = AssetDatabase::GetAsset<Shader>(L"Volume");

	if (!InitializeVR()) {
		hmd = nullptr;
		OutputDebugString(L"Failed to initialize VR\n");
		vrCamera = nullptr;
	}
}
cdvis::~cdvis() {}

void cdvis::OnResize() {
	auto window = Graphics::GetWindow();
	windowCamera->PixelWidth(window->GetWidth());
	windowCamera->PixelHeight(window->GetHeight());
	windowCamera->CreateRenderBuffers();
}

void cdvis::BrowseImage() {
	jwstring file = BrowseFile();
	if (file.empty()) return;

	XMFLOAT3 size;
	volume->mTexture = Dicom::LoadImage(file, size);
	// multiply by 2 since the cube has a radius of .5
	size.x *= 2.0f;
	size.y *= 2.0f;
	size.z *= 2.0f;
	volume->LocalScale(size);
}
void cdvis::BrowseVolume() {
	jwstring folder = BrowseFolder();
	if (folder.empty()) return;

	XMFLOAT3 size;
	volume->mTexture = Dicom::LoadVolume(folder, size);
	// multiply by 2 since the cube has a radius of .5
	size.x *= 2.0f;
	size.y *= 2.0f;
	size.z *= 2.0f;
	volume->LocalScale(size);
}

void cdvis::WindowEvent(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_KEYDOWN) {
		if (wParam == 0x49 && GetKeyState(VK_CONTROL)) // ctrl-i
			BrowseImage();
		if (wParam == 0x4F && GetKeyState(VK_CONTROL)) // ctrl-o
			BrowseVolume();
	}
}
void cdvis::VREvent(const vr::VREvent_t & event) {
	switch (event.eventType) {
	case vr::VREvent_TrackedDeviceActivated:
	{
		//SetupRenderModelForTrackedDevice(event.trackedDeviceIndex);
	}
	break;
	case vr::VREvent_TrackedDeviceDeactivated:
	case vr::VREvent_TrackedDeviceUpdated:
	break;
	}
}

void cdvis::Update(double total, double delta) {
	auto window = Graphics::GetWindow();
	if (Input::OnKeyDown(KeyCode::Enter) && Input::KeyDown(KeyCode::AltKey))
		window->SetFullscreen(!window->IsFullscreen());
	if (Input::OnKeyDown(KeyCode::F4) && Input::KeyDown(KeyCode::AltKey))
		window->Close();

	if (Input::OnKeyDown(KeyCode::F1))
		wireframe = !wireframe;
	if (Input::OnKeyDown(KeyCode::F2))
		debugDraw = !debugDraw;

	if (Input::OnKeyDown(KeyCode::G))
		volume->mLighting = !volume->mLighting;
	
	if (hmd) {
		// Process SteamVR events
		vr::VREvent_t event;
		while (hmd->PollNextEvent(&event, sizeof(event)))
			VREvent(event);

		if (vrTrackedDevices[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
			VR2DX(vrTrackedDevices[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking, vrCamera.get());
	}

	#pragma region PC controls
	if (Input::MouseWheelDelta() != 0) {
		z += Input::MouseWheelDelta() * .1f;
		windowCamera->LocalPosition(0, 0, z);
	}

	POINT cursor;
	GetCursorPos(&cursor);
	ScreenToClient(window->GetHandle(), &cursor);

	if (Input::ButtonDown(0)) {
		XMFLOAT2 md((float)-Input::MouseDelta().x, (float)-Input::MouseDelta().y);

		XMVECTOR camRot = XMLoadFloat4(&windowCamera->WorldRotation());
		XMVECTOR axis;

		if (Input::KeyDown(KeyCode::ShiftKey)) {
			XMINT2 v((int)cursor.x - window->GetWidth() / 2, (int)cursor.y - window->GetHeight() / 2);
			v = XMINT2(-v.y, v.x);
			axis = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), camRot) * (md.x * v.x + md.y * v.y);
		} else {
			XMVECTOR up = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), camRot) * md.x;
			XMVECTOR right = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), camRot) * md.y;
			axis = up + right;
		}

		if (XMVectorGetX(XMVector3LengthSq(axis)) > .01f) {
			XMVECTOR delta = XMQuaternionRotationAxis(XMVector3Normalize(axis), sqrtf(md.x * md.x + md.y * md.y) * .003f);
			volume->LocalRotation(XMQuaternionMultiply(XMLoadFloat4(&volume->LocalRotation()), delta));
		}
	}

	if (Input::KeyDown(KeyCode::Up))
		volume->mLightDensity += 1.5f * (float)delta;
	if (Input::KeyDown(KeyCode::Down))
		volume->mLightDensity -= 1.5f * (float)delta;

	if (Input::KeyDown(KeyCode::Right))
		volume->mDensity += 1.5f * (float)delta;
	if (Input::KeyDown(KeyCode::Left))
		volume->mDensity -= 1.5f * (float)delta;
	#pragma endregion
}

void cdvis::Render(shared_ptr<Camera> cam, shared_ptr<CommandList> commandList) {
	commandList->SetCamera(cam);
	cam->Clear(commandList);
	scene->Draw(commandList, cam->Frustum());
	if (debugDraw) scene->DebugDraw(commandList, cam->Frustum());
}

void cdvis::DoFrame() {
	static std::chrono::high_resolution_clock clock;
	static auto start = clock.now();
	static auto t0 = clock.now();
	static int frameCounter;
	static double elapsedSeconds;
	static double elapsedSeconds2;

	Profiler::FrameStart();

	#pragma region update
	Profiler::BeginSample(L"Update");
	auto t1 = clock.now();
	double delta = (t1 - t0).count() * 1e-9;
	t0 = t1;
	Update((t1 - start).count() * 1e-9, delta);
	Profiler::EndSample();
	#pragma endregion

	#pragma region render
	Profiler::BeginSample(L"Render");
	auto commandQueue = Graphics::GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList(Graphics::CurrentFrameIndex());
	auto d3dCommandList = commandList->D3DCommandList();
	auto window = Graphics::GetWindow();

	window->PrepareRender(commandList);

	if (wireframe) commandList->SetFillMode(D3D12_FILL_MODE_WIREFRAME);
	if (hmd) {
		Render(vrCamera->LeftEye(), commandList);
		Render(vrCamera->RightEye(), commandList);
		vrCamera->ResolveEyeTextures(commandList);
	} else {
		Render(windowCamera, commandList);
	}

	commandList->SetCamera(windowCamera);
	commandList->SetFillMode(D3D12_FILL_MODE_SOLID);

	shared_ptr<SpriteBatch> sb = Graphics::GetSpriteBatch();

	// eye textures
	windowCamera->Clear(commandList);
	float w = (float)window->GetWidth();
	float h = (float)window->GetHeight();
	sb->DrawTexture(vrCamera->SRVHeap(), vrCamera->LeftEyeSRV(), XMFLOAT4(0, 0, w*.5f, w*1.1111f), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0, 0, 1, 1));
	sb->DrawTexture(vrCamera->SRVHeap(), vrCamera->RightEyeSRV(), XMFLOAT4(w*.5f, 0, w, w*1.111f), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0, 0, 1, 1));

#pragma region window overlay
	sb->DrawTextf(arial, XMFLOAT2(windowCamera->PixelWidth() * .5f, (float)arial->GetAscender() * .5f), .5f, { 1,1,1,1 }, L"Density: %d.%d\nLight: %d.%d", F2D(volume->mDensity), F2D(volume->mLightDensity));

	sb->DrawTextf(arial, XMFLOAT2(10.0f, (float)arial->GetAscender() * .5f), .5f, { 1,1,1,1 }, L"FPS: %d.%d\n", F2D(mfps));
	sb->DrawTextf(arial, XMFLOAT2(10.0f, (float)arial->GetAscender()), .5f, { 1,1,1,1 }, pbuf);

	jvector<XMFLOAT3> verts;
	jvector<XMFLOAT4> colors;
	for (int i = 1; i < 128; i++) {
		int d = frameTimeIndex - i; if (d < 0) d += 128;
		verts.push_back({ 512.0f - i * 4.0f, windowCamera->PixelHeight() - frameTimes[d] * 5000, 0 });
		float r = fmin(frameTimes[d] / .025f, 1.0f); // full red < 40fps
		r *= r;
		colors.push_back({ r, 1.0f - r, 0, 1 });
	}
	sb->DrawLines(verts, colors);

	sb->Flush(commandList);
#pragma endregion

	Profiler::EndSample();

	Profiler::BeginSample(L"Present");
	ID3D12Resource* camrt = windowCamera->RenderBuffer().Get();
	ID3D12Resource* winrt = window->RenderBuffer().Get();
	commandList->TransitionResource(camrt, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
	commandList->TransitionResource(winrt, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_DEST);
	d3dCommandList->ResolveSubresource(winrt, 0, camrt, 0, windowCamera->RenderFormat());
	commandList->TransitionResource(camrt, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList->TransitionResource(winrt, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
	window->Present(commandList, commandQueue);

	// Submit VR textures
	if (hmd) {
		vr::VRTextureBounds_t bounds;
		bounds.uMin = 0.0f;
		bounds.uMax = 1.0f;
		bounds.vMin = 0.0f;
		bounds.vMax = 1.0f;

		vr::D3D12TextureData_t d3d12LeftEyeTexture = { vrCamera->LeftEyeTexture().Get(), commandQueue->GetCommandQueue().Get(), 0 };
		vr::Texture_t leftEyeTexture = { (void *)&d3d12LeftEyeTexture, vr::TextureType_DirectX12, vr::ColorSpace_Gamma };
		vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture, &bounds, vr::Submit_Default);

		vr::D3D12TextureData_t d3d12RightEyeTexture = { vrCamera->RightEyeTexture().Get(), commandQueue->GetCommandQueue().Get(), 0 };
		vr::Texture_t rightEyeTexture = { (void *)&d3d12RightEyeTexture, vr::TextureType_DirectX12, vr::ColorSpace_Gamma };
		vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture, &bounds, vr::Submit_Default);
	}

	Profiler::EndSample();
	#pragma endregion

	Input::FrameEnd();
	Profiler::FrameEnd();

	// measure fps
	frameCounter++;
	elapsedSeconds += delta;
	if (elapsedSeconds > 1.0) {
		mfps = frameCounter / elapsedSeconds;
		frameCounter = 0;
		elapsedSeconds = 0.0;
		ZeroMemory(pbuf, sizeof(wchar_t) * 1024);
		Profiler::PrintLastFrame(pbuf, 1024);
	}

	elapsedSeconds2 += delta;
	if (elapsedSeconds2 > 0.05) {
		elapsedSeconds2 = 0;
		frameTimes[frameTimeIndex] = (float)Profiler::LastFrameTime();
		frameTimeIndex = (frameTimeIndex + 1) % 128;
	}

	if (hmd)
		vr::VRCompositor()->WaitGetPoses(vrTrackedDevices, vr::k_unMaxTrackedDeviceCount, NULL, 0);
}