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

#include "ImageLoader.hpp"
#include "VolumeRenderer.hpp"
#include "VRUtil.hpp"
#include "VRCamera.hpp"
#include "VRDevice.hpp"
#include "VRInteraction.hpp"

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

	// turn tearing off if targeting 3d tv, to allow for true fullscreen
	HWND hWnd = JaeCreateWindow(L"CDvis", 1600, 900, 3, Graphics::CheckTearingSupport());
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
	mVRCamera->CreateCameras(w, h);
	mVRCamera->UpdateCameras(mHmd);

	mVRDevices.reserve(vr::k_unMaxTrackedDeviceCount);
	for (unsigned int i = 0; i < vr::k_unMaxTrackedDeviceCount; i++)
		mVRDevices.push_back(nullptr);

	mVRInteraction = shared_ptr<VRInteraction>(new VRInteraction());
	mVRInteraction->InitTools(mScene);

	mVREnable = true;

	return true;
}
void cdvis::Initialize() {
	AssetDatabase::LoadAssets(L"core.asset");
	AssetDatabase::LoadAssets(L"assets.asset");
	AssetDatabase::LoadAssets(L"shaders.asset");

	mScene = make_shared<Scene>();

	mArial = AssetDatabase::GetAsset<Font>(L"arial");
	mArial->GetTexture()->Upload();

	mWindowCamera = mScene->AddObject<Camera>(L"Window Camera");

	mVolume = mScene->AddObject<VolumeRenderer>(L"Volume");
	mVolume->LocalPosition(0, 1, 0);

	XMFLOAT3 vp = mVolume->LocalPosition();
	mWindowCamera->LocalPosition(vp.x, vp.y, vp.z + mCameraZ);

	#pragma region scene objects
	auto colored = AssetDatabase::GetAsset<Shader>(L"colored");

	auto roomMesh = AssetDatabase::GetAsset<Mesh>(L"Room");
	roomMesh->UploadStatic();
	auto lightMesh = AssetDatabase::GetAsset<Mesh>(L"light");
	lightMesh->UploadStatic();

	auto roomMat = shared_ptr<Material>(new Material(L"Room", AssetDatabase::GetAsset<Shader>(L"colored")));
	roomMat->SetFloat3("Color", { .5f, .5f, .5f }, -1);
	roomMat->SetFloat("Metallic", 1.f, -1);
	roomMat->SetFloat("Smoothness", .3f, -1);

	auto lightBodyMat = shared_ptr<Material>(new Material(L"Light Body", colored));
	lightBodyMat->SetFloat4("Color", { .1f, .1f, .1f, 1 }, -1);
	lightBodyMat->SetFloat("Smoothness", .2f, -1);
	auto lightMat = shared_ptr<Material>(new Material(L"Light", colored));
	lightMat->SetFloat("Smoothness", 0.9f, -1);
	lightMat->SetFloat4("Color", { .2f, .2f, .2f, 1 }, -1);

	auto room = mScene->AddObject<MeshRenderer>(L"Room");
	room->LocalScale(1.2f, 1.2f, 1.2f);
	room->SetMesh(roomMesh);
	room->SetMaterial(roomMat, 0);

	mLight = mScene->AddObject<VRLight>(L"Light");
	mLight->LocalPosition(0, 1, 0);
	mLight->SetMesh(lightMesh);
	mLight->SetMaterial(lightBodyMat, 0);
	mLight->SetMaterial(lightMat, 1);
	mLight->LocalRotation(XMQuaternionRotationRollPitchYaw(XM_PIDIV4, XM_PIDIV4, 0));
	mLight->LocalPosition(XMVectorAdd(XMVectorSet(0, 1, 0, 0), XMVector3Rotate(XMVectorSet(0, 0, -.4f, 0), XMLoadFloat4(&mLight->LocalRotation()))));
	#pragma endregion

	if (!InitializeVR()) {
		OutputDebugString(L"Failed to initialize VR\n");
		mHmd = nullptr;
		mVRCamera = nullptr;
		mVREnable = false;
	}
}
cdvis::cdvis() {}
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
	mVolume->SetTexture(ImageLoader::LoadImage(file, size));
	mVolume->LocalScale(size);
}
void cdvis::BrowseVolume() {
	jwstring folder = BrowseFolder();
	if (folder.empty()) return;

	XMFLOAT3 size;
	mVolume->SetTexture(ImageLoader::LoadVolume(folder, size));
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
		// we've already created this mesh earlier
		renderer->SetMesh(mVRMeshes.at(name));
		auto mat = shared_ptr<Material>(new Material(L"VR Render Model", AssetDatabase::GetAsset<Shader>(L"textured")));
		mat->RenderQueue(1000);
		mat->SetTexture("Texture", mVRTextures.at(renderModel->diffuseTextureId), -1);
		mat->SetFloat("Smoothness", 0.f, -1);
		mat->SetFloat("Metallic", 0.f, -1);
		renderer->SetMaterial(mat);
		return;
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
	auto mat = shared_ptr<Material>(new Material(L"VR Render Model", AssetDatabase::GetAsset<Shader>(L"textured")));
	mat->RenderQueue(1000);
	mat->SetTexture("Texture", texture, -1);
	mat->SetFloat("Smoothness", 0.f, -1);
	mat->SetFloat("Metallic", 0.f, -1);
	renderer->SetMaterial(mat);

	mVRRenderModelInterface->FreeRenderModel(renderModel);
	mVRRenderModelInterface->FreeTexture(renderModelDiffuse);

	delete[] name;
}

void cdvis::VRCreateDevice(unsigned int index) {
	mVRDevices[index] = shared_ptr<VRDevice>(new VRDevice(L"VR Device", index));
	mScene->AddObject(mVRDevices[index]);
	VRGetRenderModel(index, mVRDevices[index].get());
}

void cdvis::Update(double total, double delta) {
	auto window = Graphics::GetWindow();

	if (Input::OnKeyDown(KeyCode::Enter) && Input::KeyDown(KeyCode::AltKey))
		window->SetWindowState(window->GetWindowState() == Window::WINDOW_STATE_WINDOWED ? Window::WINDOW_STATE_BORDERLESS : Window::WINDOW_STATE_WINDOWED);
	if (Input::OnKeyDown(KeyCode::F4) && Input::KeyDown(KeyCode::AltKey))
		window->Close();

	if (Input::OnKeyDown(KeyCode::F1))
		mPerfOverlay = !mPerfOverlay;
	if (Input::OnKeyDown(KeyCode::F2))
		mDebugDraw = !mDebugDraw;
	
	#pragma region VR Controls
	static jvector<shared_ptr<VRDevice>> trackedControllers;
	if (mHmd) {
		// Process SteamVR events (controller added/removed/etc)
		vr::VREvent_t event;
		while (mHmd->PollNextEvent(&event, sizeof(event))) {}

		// Gather input from tracked controllers and convert to VRController class

		bool drawControllers = !mHmd->IsSteamVRDrawingControllers();
		trackedControllers.clear();

		for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
			if (!mVRTrackedDevices[i].bPoseIsValid) {
				// don't draw or update if the device's pose isn't valid
				if (mVRDevices[i]) mVRDevices[i]->mTracking = false;
				continue;
			}

			if (!mVRDevices[i]) VRCreateDevice(i);

			// don't draw devices if SteamVR is drawing them already (in SteamVR menu, etc)
			mVRDevices[i]->mTracking = drawControllers;
			mVRDevices[i]->UpdateDevice(mHmd, mVRTrackedDevices[i]);

			// Process device input
			switch (mHmd->GetTrackedDeviceClass(i)) {
			case vr::TrackedDeviceClass_Controller:
				trackedControllers.push_back(mVRDevices[i]);
				break;

			case vr::TrackedDeviceClass_HMD:
				mVRDevices[i]->mVisible = !mVREnable; // don't draw the headset mesh when in VR
				mVRCamera->LocalPosition(mVRDevices[i]->DevicePosition());
				mVRCamera->LocalRotation(mVRDevices[i]->DeviceRotation());
				break;

			default:
				break;
			}
		}

		mVRInteraction->ProcessInput(mScene, trackedControllers, mVolume, delta);
	}
	#pragma endregion

	#pragma region PC controls

	if (Input::OnKeyDown(KeyCode::G))
		mLight->mActivated = !mLight->mActivated;
	if (Input::OnKeyDown(KeyCode::H))
		mVolume->mISOEnable = !mVolume->mISOEnable;

	if (Input::OnKeyDown(KeyCode::V))
		mVREnable = !mVREnable;
	if (Input::OnKeyDown(KeyCode::T))
		m3DTVEnable = !m3DTVEnable;

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

	if (Input::KeyDown(KeyCode::W))
		mVolume->mDensity -= 1.5f * (float)delta;
	if (Input::KeyDown(KeyCode::E))
		mVolume->mDensity += 1.5f * (float)delta;

	if (Input::KeyDown(KeyCode::S))
		mVolume->mLightDensity -= 5.f * (float)delta;
	if (Input::KeyDown(KeyCode::D))
		mVolume->mLightDensity += 5.f * (float)delta;

	if (Input::KeyDown(KeyCode::X))
		mVolume->mExposure -= 1.5f * (float)delta;
	if (Input::KeyDown(KeyCode::C))
		mVolume->mExposure += 1.5f * (float)delta;

	if (Input::OnKeyDown(KeyCode::A))
		mVolume->mMaskMode = (VolumeRenderer::MASK_MODE)((mVolume->mMaskMode + 1) % 3);

	if (Input::KeyDown(KeyCode::N))
		mVolume->mISOValue -= .2f * (float)delta;
	if (Input::KeyDown(KeyCode::M))
		mVolume->mISOValue += .2f * (float)delta;
	mVolume->mISOValue = fmaxf(fminf(mVolume->mISOValue, 1.f), 0.f);

	if (Input::KeyDown(KeyCode::Left))
		mTVEyeSeparation -= 10.f * (float)delta;
	else if (Input::KeyDown(KeyCode::Right))
		mTVEyeSeparation += 10.f * (float)delta;
	#pragma endregion
}

void cdvis::PreRender(const shared_ptr<CommandList>& commandList) {
	XMFLOAT4 c { 8.f, 8.f, 8.f, 1.0f };
	XMFLOAT4 p;
	XMFLOAT4 d;
	*((XMFLOAT3*)&p) = mLight->WorldPosition();
	c.w = 1.f; // spot light
	p.w = 10.f;
	XMStoreFloat3((XMFLOAT3*)&d, XMVector3Rotate(XMVectorSet(0, 0, 1, 0), XMLoadFloat4(&mLight->WorldRotation())));
	d.w = cosf(XMConvertToRadians(60.f));
	if (!mLight->mActivated) c.x = c.y = c.z = 0.f;

	commandList->SetGlobalFloat3("LightAmbientSpec", { .5f, .5f, .5f });
	commandList->SetGlobalFloat3("LightAmbientDiff", { .5f, .5f, .5f });
	commandList->SetGlobalFloat4("LightColor", c);
	commandList->SetGlobalFloat4("LightPosition", p);
	commandList->SetGlobalFloat4("LightDirection", d);
	mVolume->mLightPos = *((XMFLOAT3*)&p);
	mVolume->mLightDir = *((XMFLOAT3*)&d);
	mVolume->mLightMode = 1;
	mVolume->mLightAngle = d.w;
	mVolume->mLightRange = p.w;
	mVolume->mLightAmbient = .3f;
	mVolume->mLightingEnable = mLight->mActivated;

	mLight->GetMaterial(1)->SetFloat3("Emission", mLight->mActivated ? XMFLOAT3(1.f, 1.f, 1.f) : XMFLOAT3(0.f, 0.f, 0.f), commandList->GetFrameIndex());
}

void cdvis::Render(const shared_ptr<Camera>& cam, const shared_ptr<CommandList>& commandList) {
	commandList->SetCamera(cam);
	cam->Clear(commandList, { .2f, .2f, .2f, 1.0f });
	mScene->Draw(commandList, cam);
	Graphics::GetSpriteBatch()->Flush(commandList);
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

	// Get poses as late as possible to maintain realtimeness
	if (mHmd) {
		Profiler::BeginSample(L"Update Tracking");
		vr::VRCompositor()->WaitGetPoses(mVRTrackedDevices, vr::k_unMaxTrackedDeviceCount, NULL, 0);
		Profiler::EndSample();
	}

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
	sb->Reset(commandList);

	PreRender(commandList);

	if (mHmd && mVREnable) {
		Render(mVRCamera->LeftEye(), commandList);
		Render(mVRCamera->RightEye(), commandList);

		// draw eye textures to window
		Profiler::BeginSample(L"Draw Eyes");
		mVRCamera->ResolveEyeTextures(commandList);
		commandList->SetCamera(mWindowCamera);
		commandList->SetFillMode(D3D12_FILL_MODE_SOLID);
		mWindowCamera->Clear(commandList, { 0, 0, 0, 0 });

		float w = (float)window->GetWidth();
		float h = (float)window->GetHeight();

		float ew = (float)mVRCamera->LeftEye()->PixelWidth();
		float eh = (float)mVRCamera->LeftEye()->PixelHeight();

		float viveAspectRatio = 1.0f / .7f;// .956521739f;

		if (m3DTVEnable) {
			ew = w * viveAspectRatio * .25f;
			sb->DrawTexture(mVRCamera->SRVHeap(), mVRCamera->LeftEyeSRV(), XMFLOAT4(w*.5f - ew - mTVEyeSeparation, 0, w*.5f + ew - mTVEyeSeparation, h * .5f), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0, 0, 1, 1));
			sb->DrawTexture(mVRCamera->SRVHeap(), mVRCamera->RightEyeSRV(), XMFLOAT4(w*.5f - ew + mTVEyeSeparation, h * .5f, w*.5f + ew + mTVEyeSeparation, h), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0, 0, 1, 1));
		} else {
			if (vr::IVRExtendedDisplay *d = vr::VRExtendedDisplay()) {
				uint32_t wx, wy, ww, wh;
				d->GetEyeOutputViewport(vr::EVREye::Eye_Left, &wx, &wy, &ww, &wh);
				ew = eh = (float)max(ww, wh);
				ew *= viveAspectRatio;
			}

			// calculate size of texture
			XMFLOAT2 t = XMFLOAT2(ew * 2.0f, eh);
			float s = fminf(w / t.x, h / t.y);
			t.x *= s * .5f;
			t.y *= s;

			sb->DrawTexture(mVRCamera->SRVHeap(), mVRCamera->LeftEyeSRV(),  XMFLOAT4(w*.5f - t.x, h*.5f - t.y, w*.5f, h*.5f + t.y), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0, 0, 1, 1));
			sb->DrawTexture(mVRCamera->SRVHeap(), mVRCamera->RightEyeSRV(), XMFLOAT4(w*.5f, h*.5f - t.y, w*.5f + t.x, h*.5f + t.y), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0, 0, 1, 1));
		}

		Profiler::EndSample();
	} else {
		Render(mWindowCamera, commandList);
	}

#pragma region window overlay
	Profiler::BeginSample(L"Window Overlay");

	sb->DrawTextf(mArial, XMFLOAT2(mWindowCamera->PixelWidth() * .5f, (float)mArial->GetAscender() * .4f), .4f, { 1,1,1,1 }, 
		L"Density: %d.%d\n"
		L"Light: %d.%d\n"
		L"Exposure: %d.%d\n"
		L"Thresh: %d%",
		F2D(mVolume->mDensity), F2D(mVolume->mLightDensity), F2D(mVolume->mExposure), (int)(mVolume->mISOValue * 100.f + .5f));
	sb->DrawTextf(mArial, XMFLOAT2(5.0f, (float)mArial->GetAscender() * .4f), .4f, { 1,1,1,1 }, L"FPS: %d.%d\n", F2D(mfps));

	if (mPerfOverlay) {
	sb->DrawTextf(mArial, XMFLOAT2(5.0f, mWindowCamera->PixelHeight() - 300.0f), .4f, { 1,1,1,1 },
		L"ctrl o: open volume (dicom folder)\n"
		L"ctrl i: open single image slice\n"
		L"g: toggle volume lighting\n"
		L"h: toggle volume threshold\n"
		L"w/e: adjust density\n"
		L"s/d: adjust light penetration\n"
		L"x/c: adjust exposure\n"
		L"n/m: adjust threshold value\n"
		L"v: toggle vr\n"
		L"t: toggle 3d tv mode\n"
		L"y: toggle separate camera mode for 3d tv\n"
		L"left/right: 3d tv eye separation\n"
		L"F1: toggle window overlay\n"
		L"F2: toggle debug\n"
		L"F3: toggle wireframe\n"
		L"vive touchpad: plane slice\n"
		L"vive grip: move volume\n");

		sb->DrawTextf(mArial, XMFLOAT2(5.0f, (float)mArial->GetAscender()), .4f, { 1,1,1,1 }, mPerfBuffer);

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
	}
	Profiler::EndSample();
#pragma endregion

	Profiler::BeginSample(L"Flush SpriteBatch");
	sb->Flush(commandList);
	Profiler::EndSample();

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

	// Submit VR textures (after window Present/command list execution)
	if (mVREnable && mHmd) {
		Profiler::BeginSample(L"Submit VR Textures");
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
		Profiler::EndSample();
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

	// capture frame time every .05 seconds for frame time graph
	elapsedSeconds2 += delta;
	if (elapsedSeconds2 > 0.05) {
		elapsedSeconds2 = 0;
		mFrameTimes[mFrameTimeIndex] = (float)Profiler::LastFrameTime();
		mFrameTimeIndex = (mFrameTimeIndex + 1) % 128;
	}
}