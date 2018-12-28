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

#include "Dicom.hpp"
#include "VolumeRenderer.hpp"

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "runtimeobject.lib")
#pragma comment(lib, "propsys.lib")
#include "CDialogEventHandler.hpp"

using namespace DirectX;
using namespace Microsoft::WRL;
using namespace std;

shared_ptr<Camera> camera;
shared_ptr<Scene> scene;
shared_ptr<Font> arial;
shared_ptr<VolumeRenderer> volume;

wchar_t pbuf[1024]; // performance overlay text
float frameTimes[128];
unsigned int frameTimeIndex;
bool debugDraw = false;
bool wireframe = false;
float z = -2.0f;

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

	JaeDestroy();

	delete game;
	return 0;
}

void cdvis::Initialize() {
	AssetDatabase::LoadAssets(L"core.asset");
	AssetDatabase::LoadAssets(L"assets.asset");

	scene = make_shared<Scene>();

	arial = AssetDatabase::GetAsset<Font>(L"arial");
	arial->GetTexture()->Upload();

	camera = scene->AddObject<Camera>(L"Camera");
	camera->LocalPosition(0, 0, z);
	camera->FieldOfView(60);
	camera->PixelWidth(Graphics::GetWindow()->GetWidth());
	camera->PixelHeight(Graphics::GetWindow()->GetHeight());

	shared_ptr<Mesh> cubeMesh = shared_ptr<Mesh>(new Mesh(L"Cube"));
	cubeMesh->LoadCube(.5f);
	cubeMesh->UploadStatic();

	volume = scene->AddObject<VolumeRenderer>(L"Volume");
	volume->mCubeMesh = cubeMesh;
	volume->mShader = AssetDatabase::GetAsset<Shader>(L"Volume");
}
cdvis::~cdvis() {}

void cdvis::OnResize() {
	auto window = Graphics::GetWindow();
	camera->PixelWidth(window->GetWidth());
	camera->PixelHeight(window->GetHeight());
}

void BrowseVolume() {
	jwstring folder = BrowseFolder();
	if (folder.empty()) return;
	volume->mTexture = Dicom::LoadVolume(folder);
}

void cdvis::WindowEvent(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_KEYDOWN) {
		if (wParam == 0x4F && GetKeyState(VK_CONTROL)) // ctrl-o
			BrowseVolume();
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

	POINT cursor;
	GetCursorPos(&cursor);
	ScreenToClient(window->GetHandle(), &cursor);

	if (Input::ButtonDown(0)) {
		XMFLOAT2 md((float)-Input::MouseDelta().x, (float)-Input::MouseDelta().y);

		XMVECTOR camRot = XMLoadFloat4(&camera->WorldRotation());
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

	if (Input::KeyDown(KeyCode::Up)) {
		z += (float)delta;
		camera->LocalPosition(0, 0, z);
	}
	if (Input::KeyDown(KeyCode::Down)) {
		z -= (float)delta;
		camera->LocalPosition(0, 0, z);
	}
}

void cdvis::Render(shared_ptr<CommandList> commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv) {
	auto window = Graphics::GetWindow();
	float w = (float)window->GetWidth();
	float h = (float)window->GetHeight();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.f, 0.f, w, h);
	D3D12_RECT sr = { 0, 0, window->GetWidth(), window->GetHeight() };
	commandList->D3DCommandList()->RSSetViewports(1, &vp);
	commandList->D3DCommandList()->RSSetScissorRects(1, &sr);
	commandList->D3DCommandList()->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

	DirectX::XMFLOAT4 clearColor = { 0.2f, 0.2f, 0.2f, 1.f };
	commandList->D3DCommandList()->ClearRenderTargetView(rtv, (float*)&clearColor, 0, nullptr);
	commandList->D3DCommandList()->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	if (wireframe) commandList->SetFillMode(D3D12_FILL_MODE_WIREFRAME);

	commandList->SetCamera(camera);
	scene->Draw(commandList, camera->Frustum());

	if (debugDraw) scene->DebugDraw(commandList, camera->Frustum());

#pragma region performance overlay
	commandList->SetFillMode(D3D12_FILL_MODE_SOLID);
	shared_ptr<SpriteBatch> sb = Graphics::GetSpriteBatch();
	sb->DrawTextf(arial, XMFLOAT2(10.0f, (float)arial->GetAscender() * .5f), .5f, { 1,1,1,1 }, L"FPS: %d.%d\n", (int)mfps, (int)((mfps - floor(mfps)) * 10.0f + .5f));
	sb->DrawTextf(arial, XMFLOAT2(10.0f, (float)arial->GetAscender()), .5f, { 1,1,1,1 }, pbuf);

	jvector<XMFLOAT3> verts;
	jvector<XMFLOAT4> colors;
	for (int i = 1; i < 128; i++) {
		int d = frameTimeIndex - i; if (d < 0) d += 128;
		verts.push_back({ 512.0f - i * 4.0f, h - frameTimes[d] * 5000, 0 });
		float r = fmin(frameTimes[d] / .025f, 1.0f); // full red < 40fps
		r *= r;
		colors.push_back({ r, 1.0f - r, 0, 1 });
	}
	sb->DrawLines(verts, colors);

	sb->Flush(commandList);
#pragma endregion
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

	window->PrepareRenderTargets(commandList);

	auto rtv = window->GetCurrentRenderTargetView();
	auto dsv = window->GetDepthStencilView();

	Render(commandList, rtv, dsv);
	Profiler::EndSample();

	Profiler::BeginSample(L"Present");
	window->Present(commandList, commandQueue);

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
}