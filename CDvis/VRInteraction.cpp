#include "VRInteraction.hpp"

#include <AssetDatabase.hpp>
#include <CommandList.hpp>
#include <Mesh.hpp>
#include <Shader.hpp>
#include <Material.hpp>
#include <Graphics.hpp>
#include <Texture.hpp>
#include <SpriteBatch.hpp>
#include <Window.hpp>

#include "VRDevice.hpp"
#include "VRInteractable.hpp"
#include "VolumeRenderer.hpp"
#include "VRPieMenu.hpp"
#include "VRToolTips.hpp"

using namespace std;
using namespace DirectX;

VRInteraction::VRInteraction() : mPieMenuAxis(0) {}
VRInteraction::~VRInteraction() {}

void VRInteraction::InitTools(const shared_ptr<Scene>& scene) {
	auto textured = AssetDatabase::GetAsset<Shader>(L"textured");
	auto colored = AssetDatabase::GetAsset<Shader>(L"colored");

	auto penMesh = AssetDatabase::GetAsset<Mesh>(L"pen");
	penMesh->UploadStatic();
	auto arrowMesh = AssetDatabase::GetAsset<Mesh>(L"rotatearrow");
	arrowMesh->UploadStatic();

	auto planeMesh = shared_ptr<Mesh>(new Mesh(L"plane"));
	planeMesh->VertexCount(4);
	planeMesh->GetVertices()[0] = { -.1f, -.1f, 0 };
	planeMesh->GetVertices()[1] = { -.1f,  .1f, 0 };
	planeMesh->GetVertices()[2] = {  .1f,  .1f, 0 };
	planeMesh->GetVertices()[3] = {  .1f, -.1f, 0 };
	planeMesh->Topology(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP);
	unsigned int inds[5]{ 0, 1, 2, 3, 0 };
	planeMesh->AddIndices(5, inds);
	planeMesh->UploadStatic();

	auto penTex = AssetDatabase::GetAsset<Texture>(L"pen_diffuse");
	penTex->Upload();
	auto pieicons = AssetDatabase::GetAsset<Texture>(L"pie_icons");
	pieicons->Upload();
	auto icons = AssetDatabase::GetAsset<Texture>(L"icons");
	icons->Upload();
	
	auto penMat = shared_ptr<Material>(new Material(L"Pen", textured));
	penMat->SetTexture("Texture", penTex, -1);
	penMat->SetFloat("Metallic", 0.f, -1);
	penMat->SetFloat("Smoothness", .5f, -1);

	auto planeMat = shared_ptr<Material>(new Material(L"Plane", colored));
	planeMat->EnableKeyword("NOLIGHTING");

	auto textMat = shared_ptr<Material>(new Material(L"Text", textured));
	textMat->EnableKeyword("NOLIGHTING");

	auto tooltipMat = shared_ptr<Material>(new Material(L"Text", textured));
	tooltipMat->EnableKeyword("NOLIGHTING");
	tooltipMat->CullMode(D3D12_CULL_MODE_NONE);
	tooltipMat->SetTexture("Texture", icons, -1);

	mPen = scene->AddObject<MeshRenderer>(L"Pen");
	mPen->SetMesh(penMesh);
	mPen->SetMaterial(penMat);
	mPen->mVisible = false;
	mPen->LocalPosition(0, 0, .01f);

	mPlane = scene->AddObject<MeshRenderer>(L"Plane");
	mPlane->SetMesh(planeMesh);
	mPlane->SetMaterial(planeMat);
	mPlane->mVisible = false;

	mPieMenu = scene->AddObject<VRPieMenu>(L"Pie Menu");
	mPieMenu->Initialize(.1f, NUM_VRTOOLS, pieicons);
	mPieMenu->LocalPosition(0, .02f, -.05f);
	mPieMenu->mVisible = false;

	mHudText = scene->AddObject<HudText>(L"HUD Text");
	mHudText->Font(AssetDatabase::GetAsset<Font>(L"arial"));
	mHudText->Material(textMat);
	mHudText->LocalRotation(XMQuaternionRotationRollPitchYaw(XM_PIDIV2, 0, 0));
	mHudText->LocalPosition(.5f, 1.f, -.1f);
	mHudText->LocalScale(.1f, .1f, .1f);

	mToolTips = scene->AddObject<VRToolTips>(L"ToolTips");
	mToolTips->SetMaterial(tooltipMat);
	mToolTips->mVisible = false;
}

void VRInteraction::ProcessInput(const shared_ptr<Scene>& scene, const jvector<shared_ptr<VRDevice>>& controllers, const shared_ptr<VolumeRenderer>& volume, double deltaTime) {
	auto volumeInteractable = dynamic_pointer_cast<VRInteractable>(volume);
	mPieMenu->mVisible = false;
	for (unsigned int i = 0; i < controllers.size(); i++) {
		auto controller = controllers[i];

		// pie menu
		if (controller->ButtonTouched(PieMenuButton)) {
			mPieMenu->mVisible = true;
			mPieMenu->Parent(controller);

			mPieMenu->Pressed(mCurTool);

			unsigned short x = 0;
			if (mPieMenu->UpdateTouch(*(XMFLOAT2*)&controller->GetState().rAxis[mPieMenuAxis]))
				x = 600;

			if (controller->ButtonPressed(PieMenuButton) && mCurTool != mPieMenu->Hovered()) {
				if (mToolController) mToolController->mVisible = true;
				mToolController = controller;
				mPen->Parent(controller);
				mPlane->Parent(controller);
				mLastPenPos = mPen->WorldPosition();

				mCurTool = (VRTOOL)mPieMenu->Hovered();
				x = 1000;

				switch (mCurTool) {
				case VRTOOL_ERASE:
				case VRTOOL_PAINT:
					mPen->mVisible = true;
					mPlane->mVisible = false;
					mToolController->mVisible = false;
					break;
				case VRTOOL_PLANE:
					mPen->mVisible = false;
					mPlane->mVisible = true;
					break;
				}
			}

			if (x) controller->TriggerHapticPulse(x);
		}

		if (controller->ButtonPressedFirst(HelpButton)) {
			mToolTips->mVisible = !mToolTips->mVisible;
			if (mToolTips->mVisible)
				mToolTips->Parent(controller);
		}

		// volume haptics
		if (mCurTool != VRTOOL_PLANE) {
			// measure density travelled through since last frame
			XMMATRIX w2o = XMLoadFloat4x4(&volume->WorldToObject());

			XMVECTOR p = XMVector3Transform(XMLoadFloat3(&mPen->WorldPosition()), w2o);
			p = XMVectorAdd(p, XMVectorSet(.5f, .5f, .5f, 0.f)); // [-.5, .5] to [0, 1]

			XMVECTOR lp = XMVector3Transform(XMLoadFloat3(&mLastPenPos), w2o);
			lp = XMVectorAdd(lp, XMVectorSet(.5f, .5f, .5f, 0.f)); // [-.5, .5] to [0, 1]

			XMVECTOR rd = XMVectorSubtract(lp, p);
			float d = XMVectorGetX(XMVector3Length(rd));

			float x = 0;
			float dt = fminf(.01f, d / 5.f); // 5 tiny steps, or a step size of .01f

			rd = XMVectorScale(rd, dt * 1.f / d); // make rd have a length of dt
			XMFLOAT3 uvw;
			for (float t = dt; t < d; t += dt) {
				p = XMVectorAdd(p, rd);
				XMStoreFloat3(&uvw, p);
				x += volume->GetDensity(uvw, true) * dt;
			}
			x *= 100.f;
			x = sqrtf(fmin(fmax(x * x, 0.0f), 1.0f)) * 5000.f;
			unsigned short xs = (unsigned short)x;

			if (xs > 10) controller->TriggerHapticPulse(xs);
		}

		// tools
		if ((controller->mHovered.size() == 1) && (*controller->mHovered.begin() == volumeInteractable)) {
			// use tools
			if (controller->ButtonPressed(ActivateButton)) {
				switch (mCurTool) {
				case VRTOOL_PLANE:
				{
					volume->mSlicePlaneEnable = true;
					XMFLOAT3 n;
					XMStoreFloat3(&n, XMVector3Rotate(XMVectorSet(0, 0, 1, 1), XMLoadFloat4(&controller->DeviceRotation())));
					volume->SetSlicePlane(controller->DevicePosition(), n);
					break;
				}
				case VRTOOL_PAINT:
				{
					volume->PaintMask(mLastPenPos, mPen->WorldPosition(), 1.f, .03f);
					break;
				}
				case VRTOOL_ERASE:
				{
					volume->PaintMask(mLastPenPos, mPen->WorldPosition(), 0.f, .03f);
					break;
				}
				}
			}
		}
	}
	mLastPenPos = mPen->WorldPosition();

	if (mHudText->mVisible) {
		static wchar_t hudtext[1024];
		swprintf_s(hudtext, 1024,
			L"Density: %.1f\n"
			L"Exposure: %.1f\n"
			L"Thresh: %.1f%%\n"
			L"Light: %.1f",
			volume->mDensity, volume->mLightIntensity, volume->mExposure, volume->mISOValue * 100.f);
		mHudText->Text(hudtext);
	}
}