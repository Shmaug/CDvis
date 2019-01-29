#include "VRTools.hpp"

#include <AssetDatabase.hpp>
#include <CommandList.hpp>
#include <Mesh.hpp>
#include <Scene.hpp>
#include <Shader.hpp>
#include <Material.hpp>

#include "VRDevice.hpp"
#include "VRInteractable.hpp"
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
	penMat->SetFloat("Metallic", 0.f, -1);
	penMat->SetFloat("Smoothness", .5f, -1);

	auto lightBodyMat = shared_ptr<Material>(new Material(L"Light Body", colored));
	lightBodyMat->SetFloat4("Color", { .1f, .1f, .1f, 1 }, -1);
	lightBodyMat->SetFloat("Smoothness", .2f, -1);
	auto lightMat = shared_ptr<Material>(new Material(L"Light", colored));
	lightMat->SetFloat4("Color", { 1, 1, 1, 1 }, -1);
	lightMat->EnableKeyword("NOLIGHTING");

	mLight = GetScene()->AddObject<VRDraggableMeshRenderer>(L"Light");
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
	static jvector<shared_ptr<Object>> boundsIntersect;

	for (unsigned int i = 0; i < controllers.size(); i++) {
		auto controller = controllers[i];

		// dragging objects
			boundsIntersect.clear();
			GetScene()->IntersectPoint(controller->LocalPosition(), boundsIntersect);
			for (unsigned int j = 0; j < boundsIntersect.size(); j++) {
				auto g = std::dynamic_pointer_cast<VRInteractable>(boundsIntersect[j]);
				if (!g) continue;

			if (g->Draggable() && controller->ButtonPressedFirst(vr::EVRButtonId::k_EButton_Grip)) {
				DragOperation* d = nullptr;
				for (unsigned int k = 0; k < mDragging.size(); k++) {
					if (mDragging[k].mObject == boundsIntersect[j]) {
						d = &mDragging[k];
						break;
					}
				}
				if (!d) {
					d = &mDragging.push_back({});

					XMVECTOR op = XMLoadFloat3(&boundsIntersect[j]->WorldPosition());
					XMVECTOR or = XMLoadFloat4(&boundsIntersect[j]->WorldRotation());

					d->mController = controller;
					d->mObject = boundsIntersect[j];
					XMStoreFloat3(&(d->mDragPos), XMVector3Transform(op, XMLoadFloat4x4(&controller->WorldToObject())));
					XMStoreFloat4(&(d->mDragRotation), XMQuaternionMultiply(or, XMQuaternionInverse(XMLoadFloat4(&controller->DeviceRotation())) ));
					
					g->DragStart(controller);
				}
			}
		}

		// cutting plane
		if (controller->ButtonPressed(vr::EVRButtonId::k_EButton_Axis0)) {
			volume->mSlicePlaneEnable = true;
			XMFLOAT3 n;
			XMStoreFloat3(&n, XMVector3Rotate(XMVectorSet(0, 0, 1, 1), XMLoadFloat4(&controller->DeviceRotation())));
			volume->SetSlicePlane(controller->DevicePosition(), n);
		}


		// haptics
		if (controller->ButtonPressed(vr::EVRButtonId::k_EButton_SteamVR_Trigger)) {
			XMVECTOR lpv = XMVector3Transform(XMLoadFloat3(&controller->DevicePosition()), XMLoadFloat4x4(&volume->WorldToObject()));
			lpv = XMVectorAdd(lpv, XMVectorSet(.5f, .5f, .5f, 0.f)); // [-.5, .5] to [0, 1]
			XMFLOAT3 lp;
			XMStoreFloat3(&lp, lpv);

			float x = volume->GetDensityTrilinear(lp, true);
			if (volume->mISOEnable) x = (x > volume->mISOValue) ? x : 0;
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

	for (unsigned int i = 0; i < mDragging.size(); i++) {
		if (mDragging[i].mController->ButtonPressed(vr::EVRButtonId::k_EButton_Grip)) {
			auto o = mDragging[i].mObject.get();
			o->LocalPosition(XMVector3Transform(XMLoadFloat3(&mDragging[i].mDragPos), XMLoadFloat4x4(&mDragging[i].mController->ObjectToWorld())));
			o->LocalRotation(XMQuaternionMultiply(XMLoadFloat4(&mDragging[i].mDragRotation), XMLoadFloat4(&mDragging[i].mController->DeviceRotation())));
		} else {
			std::dynamic_pointer_cast<VRInteractable>(mDragging[i].mObject)->DragStop(mDragging[i].mController);
			mDragging.remove(i);
			i--;
		}
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