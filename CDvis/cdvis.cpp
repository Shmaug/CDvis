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

#include <Light.hpp>
#include <Shader.hpp>
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

	delete game;

	JaeDestroy();

	return 0;
}

bool cdvis::InitializeVR() {
	vr::EVRInitError eError = vr::VRInitError_None;
	mHmd = vr::VR_Init(&eError, vr::VRApplication_Scene);
	if (eError != vr::VRInitError_None) {
		OutputDebugStringA("Failed to initialize VR: ");
		OutputDebugStringA(vr::VR_GetVRInitErrorAsEnglishDescription(eError));
		OutputDebugStringA("\n");
		return false;
	}

	mVRRenderModelInterface = (vr::IVRRenderModels*)vr::VR_GetGenericInterface(vr::IVRRenderModels_Version, &eError);
	if (!mVRRenderModelInterface) {
		vr::VR_Shutdown();
		OutputDebugf(L"Failed to get render model interface: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
		return false;
	}

	if (!vr::VRCompositor()) {
		OutputDebugString(L"Failed to initialize compositor.\n");
		return false;
	}

	unsigned int w, h;
	mHmd->GetRecommendedRenderTargetSize(&w, &h);

	mVRCamera = mScene->AddObject<VRCamera>(L"VR Camera");
	mVRCamera->LocalPosition(0, 0, mCameraZ);
	mVRCamera->CreateCameras(w, h);
	mVRCamera->UpdateCameras(mHmd);

	mVRMaterial = shared_ptr<Material>(new Material(L"Device", AssetDatabase::GetAsset<Shader>(L"device")));

	mVRDevices.reserve(vr::k_unMaxTrackedDeviceCount);
	for (unsigned int i = 0; i < vr::k_unMaxTrackedDeviceCount; i++)
		mVRDevices.push_back(nullptr);

	mVREnable = true;

	return true;
}
void cdvis::Initialize() {
	AssetDatabase::LoadAssets(L"core.asset");
	AssetDatabase::LoadAssets(L"assets.asset");

	mScene = make_shared<Scene>();

	mArial = AssetDatabase::GetAsset<Font>(L"arial");
	mArial->GetTexture()->Upload();

	mWindowCamera = mScene->AddObject<Camera>(L"Window Camera");

	mDepthMaterial = shared_ptr<Material>(new Material(L"Depth", AssetDatabase::GetAsset<Shader>(L"DepthPass")));

	mVolume = mScene->AddObject<VolumeRenderer>(L"Volume");
	mVolume->LocalPosition(0, 1, 0);

	XMFLOAT3 vp = mVolume->LocalPosition();
	mWindowCamera->LocalPosition(vp.x, vp.y, vp.z + mCameraZ);

	if (!InitializeVR()) {
		OutputDebugString(L"Failed to initialize VR\n");
		mHmd = nullptr;
		mVRCamera = nullptr;
		mVREnable = false;
	}
}
cdvis::~cdvis() {
	if (mHmd) vr::VR_Shutdown();
}

void cdvis::OnResize() {
	auto window = Graphics::GetWindow();
	mWindowCamera->PixelWidth(window->GetWidth());
	mWindowCamera->PixelHeight(window->GetHeight());
	mWindowCamera->CreateRenderBuffers();
}

void cdvis::BrowseImage() {
	jwstring file = BrowseFile();
	if (file.empty()) return;

	XMFLOAT3 size;
	mVolume->SetTexture(Dicom::LoadImage(file, size));
	mVolume->LocalScale(size);
}
void cdvis::BrowseVolume() {
	jwstring folder = BrowseFolder();
	if (folder.empty()) return;

	XMFLOAT3 size;
	mVolume->SetTexture(Dicom::LoadVolume(folder, size));
	mVolume->LocalScale(size);
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

void cdvis::VRGetRenderModel(unsigned int index, MeshRenderer* renderer) {
	unsigned int len = mHmd->GetStringTrackedDeviceProperty(index, vr::TrackedDeviceProperty::Prop_RenderModelName_String, NULL, 0);
	char* name = new char[len];
	mHmd->GetStringTrackedDeviceProperty(index, vr::TrackedDeviceProperty::Prop_RenderModelName_String, name, len);

	vr::RenderModel_t* renderModel;
	vr::RenderModel_TextureMap_t* renderModelDiffuse;
	
	vr::EVRRenderModelError error;
	while ((error = mVRRenderModelInterface->LoadRenderModel_Async(name, &renderModel)) == vr::VRRenderModelError_Loading)
		Sleep(1);
	if (error != vr::VRRenderModelError_None) return;

	while ((error = mVRRenderModelInterface->LoadTexture_Async(renderModel->diffuseTextureId, &renderModelDiffuse)) == vr::VRRenderModelError_Loading)
		Sleep(1);

	if (error != vr::VRRenderModelError_None) {
		mVRRenderModelInterface->FreeRenderModel(renderModel);
		return;
	}

	if (mVRMeshes.count(name)) {
		renderer->SetMesh(mVRMeshes.at(name));
		auto mat = shared_ptr<Material>(new Material(L"VR Render Model", AssetDatabase::GetAsset<Shader>(L"device")));
		mat->SetTexture("Texture", mVRTextures.at(renderModel->diffuseTextureId), -1);
		renderer->SetMaterial(mat);
	}

	shared_ptr<Mesh> mesh = shared_ptr<Mesh>(new Mesh(L"VR Render Model"));
	mesh->HasSemantic(MESH_SEMANTIC_NORMAL, true);
	mesh->HasSemantic(MESH_SEMANTIC_TEXCOORD0, true);
	mesh->VertexCount(renderModel->unVertexCount);
	
	for (unsigned int i = 0; i < renderModel->unVertexCount; i++) {
		mesh->GetVertices()[i] = *((XMFLOAT3*)renderModel->rVertexData[i].vPosition.v);
		mesh->GetVertices()[i].z = -mesh->GetVertices()[i].z;
		mesh->GetNormals()[i] = *((XMFLOAT3*)renderModel->rVertexData[i].vNormal.v);
		mesh->GetNormals()[i].z = -mesh->GetNormals()[i].z;
		mesh->GetTexcoords(0)[i].x = renderModel->rVertexData[i].rfTextureCoord[0];
		mesh->GetTexcoords(0)[i].y = renderModel->rVertexData[i].rfTextureCoord[1];
	}
	
	for (unsigned int i = 0; i < renderModel->unTriangleCount * 3; i+=3)
		mesh->AddTriangle(renderModel->rIndexData[i], renderModel->rIndexData[i + 2], renderModel->rIndexData[i + 1]);

	mesh->UploadStatic();

	shared_ptr<Texture> texture = shared_ptr<Texture>(new Texture(L"VR Render Model Diffuse",
		renderModelDiffuse->unWidth, renderModelDiffuse->unHeight, 1,
		D3D12_RESOURCE_DIMENSION_TEXTURE2D, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1,
		renderModelDiffuse->rubTextureMapData, renderModelDiffuse->unWidth * renderModelDiffuse->unHeight * 4, false));
	texture->Upload();

	mVRMeshes.emplace(name, mesh);
	mVRTextures.emplace(renderModel->diffuseTextureId, texture);

	renderer->SetMesh(mesh);
	auto mat = shared_ptr<Material>(new Material(L"VR Render Model", AssetDatabase::GetAsset<Shader>(L"device")));
	mat->SetTexture("Texture", texture, -1);
	renderer->SetMaterial(mat);

	mVRRenderModelInterface->FreeRenderModel(renderModel);
	mVRRenderModelInterface->FreeTexture(renderModelDiffuse);

	delete[] name;
}

void cdvis::VRCreateDevice(unsigned int index) {
	mVRDevices[index] = mScene->AddObject<MeshRenderer>(L"VR Controller");
	VRGetRenderModel(index, mVRDevices[index].get());
}

void cdvis::Update(double total, double delta) {
	auto window = Graphics::GetWindow();
	if (Input::OnKeyDown(KeyCode::Enter) && Input::KeyDown(KeyCode::AltKey))
		window->SetFullscreen(!window->IsFullscreen());
	if (Input::OnKeyDown(KeyCode::F4) && Input::KeyDown(KeyCode::AltKey))
		window->Close();

	if (Input::OnKeyDown(KeyCode::F1))
		mWireframe = !mWireframe;
	if (Input::OnKeyDown(KeyCode::F2))
		mDebugDraw = !mDebugDraw;

	if (Input::OnKeyDown(KeyCode::D))
		mVolume->mDepthColor = !mVolume->mDepthColor;
	if (Input::OnKeyDown(KeyCode::G))
		mVolume->mLightingEnable = !mVolume->mLightingEnable;

	if (Input::OnKeyDown(KeyCode::V))
		mVREnable = !mVREnable;
	
	mVolume->mSlicePlaneEnable = false;
	if (mHmd) {
		// Process SteamVR events
		vr::VREvent_t event;
		while (mHmd->PollNextEvent(&event, sizeof(event)))
			VREvent(event);

		if (mVRTrackedDevices[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
			VR2DX(mVRTrackedDevices[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking, mVRCamera.get());

		bool drawControllers = !mHmd->IsSteamVRDrawingControllers();

		for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
			// update tracking
			if (mVRTrackedDevices[i].bPoseIsValid) {
				if (!mVRDevices[i]) VRCreateDevice(i);
				VR2DX(mVRTrackedDevices[i].mDeviceToAbsoluteTracking, mVRDevices[i].get());
				mVRDevices[i]->mVisible = drawControllers;
			} else
				if (mVRDevices[i]) mVRDevices[i]->mVisible = false;

			// Process SteamVR controller state
			switch (mHmd->GetTrackedDeviceClass(i)) {
			case vr::TrackedDeviceClass_Controller:
			{
				vr::VRControllerState_t state;
				if (mHmd->GetControllerState(i, &state, sizeof(state))) {
					if (state.ulButtonPressed) {
						mVolume->mSlicePlaneEnable = true;
						XMVECTOR n = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), XMLoadFloat4(&mVRDevices[i]->WorldRotation()));
						XMStoreFloat3((XMFLOAT3*)&mVolume->mSlicePlane, n);
						mVolume->mSlicePlane.w = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&mVRDevices[i]->WorldPosition()), n));
					}
				}
				break;
			}
			}
		}

		if (mVRDevices[vr::k_unTrackedDeviceIndex_Hmd] && mVRDevices[vr::k_unTrackedDeviceIndex_Hmd]->mVisible)
			mVRDevices[vr::k_unTrackedDeviceIndex_Hmd]->mVisible = !mVREnable;
	}

	#pragma region PC controls
	if (Input::MouseWheelDelta() != 0) {
		mCameraZ += Input::MouseWheelDelta() * .1f;
		XMFLOAT3 vp = mVolume->LocalPosition();
		mWindowCamera->LocalPosition(vp.x, vp.y, vp.z + mCameraZ);
	}

	POINT cursor;
	GetCursorPos(&cursor);
	ScreenToClient(window->GetHandle(), &cursor);

	if (Input::ButtonDown(0)) {
		XMFLOAT2 md((float)-Input::MouseDelta().x, (float)-Input::MouseDelta().y);

		XMVECTOR camRot = XMLoadFloat4(&mWindowCamera->WorldRotation());
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
			mVolume->LocalRotation(XMQuaternionMultiply(XMLoadFloat4(&mVolume->LocalRotation()), delta));
		}
	}

	if (Input::KeyDown(KeyCode::Up))
		mVolume->mLightDensity += 1.5f * (float)delta;
	if (Input::KeyDown(KeyCode::Down))
		mVolume->mLightDensity -= 1.5f * (float)delta;

	if (Input::KeyDown(KeyCode::Right))
		mVolume->mDensity += 1.5f * (float)delta;
	if (Input::KeyDown(KeyCode::Left))
		mVolume->mDensity -= 1.5f * (float)delta;
	#pragma endregion
}

void cdvis::Render(shared_ptr<Camera> cam, shared_ptr<CommandList> commandList) {
	commandList->SetCamera(cam);
	cam->Clear(commandList);
	mScene->Draw(commandList, cam);
	if (mDebugDraw) mScene->DebugDraw(commandList, cam);
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

	shared_ptr<SpriteBatch> sb = Graphics::GetSpriteBatch();

	if (mWireframe) commandList->SetFillMode(D3D12_FILL_MODE_WIREFRAME);
	if (mVREnable) {
		Render(mVRCamera->LeftEye(), commandList);
		Render(mVRCamera->RightEye(), commandList);
		// eye textures
		mVRCamera->ResolveEyeTextures(commandList);
		commandList->SetCamera(mWindowCamera);
		commandList->SetFillMode(D3D12_FILL_MODE_SOLID);
		mWindowCamera->Clear(commandList);
		float w = (float)window->GetWidth();
		float h = (float)window->GetHeight();
		sb->DrawTexture(mVRCamera->SRVHeap(), mVRCamera->LeftEyeSRV(), XMFLOAT4(0, 0, w*.5f, w*1.1111f), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0, 0, 1, 1));
		sb->DrawTexture(mVRCamera->SRVHeap(), mVRCamera->RightEyeSRV(), XMFLOAT4(w*.5f, 0, w, w*1.111f), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0, 0, 1, 1));
	} else {
		Render(mWindowCamera, commandList);
	}

#pragma region window overlay
	sb->DrawTextf(mArial, XMFLOAT2(mWindowCamera->PixelWidth() * .5f, (float)mArial->GetAscender() * .5f), .5f, { 1,1,1,1 }, L"Density: %d.%d\nLight: %d.%d", F2D(mVolume->mDensity), F2D(mVolume->mLightDensity));

	sb->DrawTextf(mArial, XMFLOAT2(10.0f, (float)mArial->GetAscender() * .5f), .5f, { 1,1,1,1 }, L"FPS: %d.%d\n", F2D(mfps));
	sb->DrawTextf(mArial, XMFLOAT2(10.0f, (float)mArial->GetAscender()), .5f, { 1,1,1,1 }, mPerfBuffer);

	jvector<XMFLOAT3> verts;
	jvector<XMFLOAT4> colors;
	for (int i = 1; i < 128; i++) {
		int d = mFrameTimeIndex - i; if (d < 0) d += 128;
		verts.push_back({ 512.0f - i * 4.0f, mWindowCamera->PixelHeight() - mFrameTimes[d] * 5000, 0 });
		float r = fmin(mFrameTimes[d] / .025f, 1.0f); // full red < 40fps
		r *= r;
		colors.push_back({ r, 1.0f - r, 0, 1 });
	}
	sb->DrawLines(verts, colors);

	sb->Flush(commandList);
#pragma endregion

	Profiler::EndSample();

	Profiler::BeginSample(L"Present");
	ID3D12Resource* camrt = mWindowCamera->RenderBuffer().Get();
	ID3D12Resource* winrt = window->RenderBuffer().Get();
	commandList->TransitionResource(camrt, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
	commandList->TransitionResource(winrt, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_DEST);
	d3dCommandList->ResolveSubresource(winrt, 0, camrt, 0, mWindowCamera->RenderFormat());
	commandList->TransitionResource(camrt, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList->TransitionResource(winrt, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
	window->Present(commandList, commandQueue);

	// Submit VR textures
	if (mVREnable && mHmd) {
		vr::VRTextureBounds_t bounds;
		bounds.uMin = 0.0f;
		bounds.uMax = 1.0f;
		bounds.vMin = 0.0f;
		bounds.vMax = 1.0f;

		vr::D3D12TextureData_t d3d12LeftEyeTexture = { mVRCamera->LeftEyeTexture().Get(), commandQueue->GetCommandQueue().Get(), 0 };
		vr::Texture_t leftEyeTexture = { (void *)&d3d12LeftEyeTexture, vr::TextureType_DirectX12, vr::ColorSpace_Gamma };
		vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture, &bounds, vr::Submit_Default);

		vr::D3D12TextureData_t d3d12RightEyeTexture = { mVRCamera->RightEyeTexture().Get(), commandQueue->GetCommandQueue().Get(), 0 };
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
		ZeroMemory(mPerfBuffer, sizeof(wchar_t) * 1024);
		Profiler::PrintLastFrame(mPerfBuffer, 1024);
	}

	elapsedSeconds2 += delta;
	if (elapsedSeconds2 > 0.05) {
		elapsedSeconds2 = 0;
		mFrameTimes[mFrameTimeIndex] = (float)Profiler::LastFrameTime();
		mFrameTimeIndex = (mFrameTimeIndex + 1) % 128;
	}

	if (mHmd) vr::VRCompositor()->WaitGetPoses(mVRTrackedDevices, vr::k_unMaxTrackedDeviceCount, NULL, 0);
}