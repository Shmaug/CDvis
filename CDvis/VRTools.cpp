#include "VRTools.hpp"

#include <AssetDatabase.hpp>
#include <CommandList.hpp>
#include <Mesh.hpp>
#include <Scene.hpp>
#include <Shader.hpp>
#include <Material.hpp>

#include "VRDevice.hpp"
#include "VRButton.hpp"
#include "VolumeRenderer.hpp"

#define F2D(x) (int)x, (int)((x - floor(x)) * 10.0f)

using namespace std;
using namespace DirectX;

VRTools::VRTools(jwstring name) : Object(name), mMenuShown(false) {}
VRTools::~VRTools() {}

void VRTools::InitTools() {
	auto textured = AssetDatabase::GetAsset<Shader>(L"textured");
	auto colored = AssetDatabase::GetAsset<Shader>(L"colored");

	auto dialMesh = AssetDatabase::GetAsset<Mesh>(L"dial");
	dialMesh->UploadStatic();
	auto penMesh = AssetDatabase::GetAsset<Mesh>(L"pen");
	penMesh->UploadStatic();
	auto lightMesh = AssetDatabase::GetAsset<Mesh>(L"light");
	lightMesh->UploadStatic();
	auto arrowMesh = AssetDatabase::GetAsset<Mesh>(L"rotatearrow");
	arrowMesh->UploadStatic();

	auto penTex = AssetDatabase::GetAsset<Texture>(L"pen_diffuse");
	penTex->Upload();
	auto dialTex = AssetDatabase::GetAsset<Texture>(L"dial_diffuse");
	dialTex->Upload();
	auto icons = AssetDatabase::GetAsset<Texture>(L"icons");
	icons->Upload();
	
	auto penMat = shared_ptr<Material>(new Material(L"Pen", textured));
	penMat->SetTexture("Texture", penTex, -1);
	auto lightBodyMat = shared_ptr<Material>(new Material(L"Light Body", colored));
	lightBodyMat->SetFloat4("Color", { .2f, .2f, .2f, 1 }, -1);
	auto lightMat = shared_ptr<Material>(new Material(L"Light", colored));
	lightMat->SetFloat4("Color", { 1, 1, 1, 1 }, -1);
	lightMat->EnableKeyword("NOLIGHTING");

	mLight = GetScene()->AddObject<MeshRenderer>(L"Light");
	mLight->LocalPosition(0, 1, 0);
	mLight->SetMesh(lightMesh);
	mLight->SetMaterial(lightBodyMat, 0);
	mLight->SetMaterial(lightMat, 1);

	mPen = GetScene()->AddObject<MeshRenderer>(L"Pen");
	mPen->SetMesh(penMesh);
	mPen->SetMaterial(penMat);
	mPen->mVisible = false;

	for (unsigned int i = 0; i < DirectX::BoundingOrientedBox::CORNER_COUNT; i++) {
		auto mat = shared_ptr<Material>(new Material(L"Arrow material", colored));
		mat->SetFloat4("Color", { .8f, .3f, .15f, 1.0f }, -1);
		mat->RenderQueue(100);
		mRotateArrows[i] = GetScene()->AddObject<MeshRenderer>(L"Rotate Arrow");
		mRotateArrows[i]->SetMesh(arrowMesh);
		mRotateArrows[i]->SetMaterial(mat);
		mRotateArrows[i]->mVisible = false;
	}
}

void VRTools::ProcessInput(jvector<shared_ptr<VRDevice>>& controllers, shared_ptr<VolumeRenderer> volume) {
	XMVECTOR vp = XMLoadFloat3(&volume->LocalPosition());
	XMVECTOR vr = XMLoadFloat4(&volume->LocalRotation());

	static XMFLOAT3 corners[DirectX::BoundingOrientedBox::CORNER_COUNT];
	volume->Bounds().GetCorners(corners);

	static jvector<unsigned int> cornerControllers; // controllers hovering over a corner
	cornerControllers.clear();
	bool showCorners = false;

	for (unsigned int i = 0; i < controllers.size(); i++) {
		auto controller = controllers[i];

		// cutting plane
		if (controller->ButtonPressed(vr::EVRButtonId::k_EButton_Axis0)) {
			volume->mSlicePlaneEnable = true;
			XMFLOAT3 n;
			XMStoreFloat3(&n, XMVector3Rotate(XMVectorSet(0, 0, 1, 1), XMLoadFloat4(&controller->DeviceRotation())));
			volume->SetSlicePlane(controller->DevicePosition(), n);
		}

		// corner arrows
		XMVECTOR cp = XMLoadFloat3(&controller->LocalPosition());
		unsigned int ci = 0;
		float cd = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(XMLoadFloat3(&corners[0]), cp)));
		for (unsigned int j = 1; j < DirectX::BoundingOrientedBox::CORNER_COUNT; j++) {
			float d = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(XMLoadFloat3(&corners[j]), cp)));
			if (d < cd) {
				cd = d;
				ci = j;
			}
		}
		if (cd < .002) showCorners = true;

		// dragging
		if (controller->ButtonPressedFirst(vr::EVRButtonId::k_EButton_Grip)) {
			if (volume->Bounds().Contains(cp)) {
				mDragging = controller;
				// dragPos = (volPos - controllerPos) * controllerRotation^-1
				XMStoreFloat3(&mDragPos, XMVector3Rotate(XMVectorSubtract(vp, XMLoadFloat3(&controller->DevicePosition())), XMQuaternionInverse(XMLoadFloat4(&controller->LocalRotation()))));
				XMStoreFloat4(&mDragRotation, XMQuaternionMultiply(vr, XMQuaternionInverse(XMLoadFloat4(&controller->DeviceRotation()))));
			}
		}

		// haptics
		if (controller->ButtonPressed(vr::EVRButtonId::k_EButton_SteamVR_Trigger)) {
			XMVECTOR lpv = XMVector3Transform(XMLoadFloat3(&controller->DevicePosition()), XMLoadFloat4x4(&volume->WorldToObject()));
			lpv = XMVectorAdd(lpv, XMVectorSet(.5f, .5f, .5f, 0.f)); // [-.5, .5] to [0, 1]
			XMFLOAT3 lp;
			XMStoreFloat3(&lp, lpv);

			float x = volume->GetDensityTrilinear(lp, true);
			x += .5f;
			x = x * x - .4f; // (x+.5)^2 - .4

			// scale by distance moved
			x *= fmin(fmax(1000.0f * (XMVectorGetX(XMVector3Length(XMLoadFloat3(&controller->DeltaDevicePosition()))) - .001f), 0.0f), 1.0f);

			controller->TriggerHapticPulse((unsigned short)(fmin(fmax(x, 0.0f), 1.0f) * 5000));
		}

		// open pie menu
		if (controller->ButtonPressedFirst(vr::EVRButtonId::k_EButton_ApplicationMenu)) {
			if (mMenuShown)
				HideMenu();
			else
				ShowMenu(controller);
		}
	}

	if (mDragging) {
		if (mDragging->ButtonPressed(vr::EVRButtonId::k_EButton_Grip)) {
			volume->LocalPosition(XMVector3Transform(XMLoadFloat3(&mDragPos), XMLoadFloat4x4(&mDragging->ObjectToWorld())));
			volume->LocalRotation(XMQuaternionMultiply(XMLoadFloat4(&mDragRotation), XMLoadFloat4(&mDragging->LocalRotation())));
		} else
			mDragging = nullptr;
	}

	for (unsigned int i = 0; i < DirectX::BoundingOrientedBox::CORNER_COUNT; i++) {
		mRotateArrows[i]->mVisible = showCorners;
		mRotateArrows[i]->LocalPosition(corners[i]);
		mRotateArrows[i]->LocalRotation(vr);
	}

	if (cornerControllers.size() == 1) {
		// Rotate volume from corner
	}
}

void VRTools::ShowMenu(shared_ptr<VRDevice> attach) {
	mMenuShown = true;
	Parent(attach);
	// TODO
}
void VRTools::HideMenu() {
	mMenuShown = false;
}