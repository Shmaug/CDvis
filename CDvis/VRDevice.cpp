#include "VRDevice.hpp"

#include "VRUtil.hpp"
#include "VRInteractable.hpp"
#include "VRInteraction.hpp"

#include <Scene.hpp>

using namespace std;
using namespace DirectX;

VRDevice::VRDevice(jwstring name, unsigned int index) : MeshRenderer(name), mDeviceIndex(index) {}
VRDevice::~VRDevice() {}

void VRDevice::UpdateDevice(vr::IVRSystem* hmd, const vr::TrackedDevicePose_t& pose) {
	mHmd = hmd;
	mLastDevicePosition = mDevicePosition;
	mLastDeviceRotation = mDeviceRotation;

	VR2DX(pose.mDeviceToAbsoluteTracking, mDevicePosition, mDeviceRotation);

	LocalPosition(mDevicePosition);
	LocalRotation(mDeviceRotation);

	XMStoreFloat3(&mDeltaDevicePosition, XMLoadFloat3(&mDevicePosition) - XMLoadFloat3(&mLastDevicePosition));
	// qDelta = qTo * inverse(qFrom)
	XMStoreFloat4(&mDeltaDeviceRotation,
		XMQuaternionMultiply(XMLoadFloat4(&mDeviceRotation), XMQuaternionInverse(XMLoadFloat4(&mLastDeviceRotation))) );

	if (hmd->GetTrackedDeviceClass(mDeviceIndex) == vr::TrackedDeviceClass_Controller) {
		mLastState = mState;
		hmd->GetControllerState(mDeviceIndex, &mState, sizeof(vr::VRControllerState_t));
	}

	#pragma region Interaction
	bool grab = ButtonPressed(VRInteraction::GrabButton);
	bool grabStart = ButtonPressedFirst(VRInteraction::GrabButton);
	bool activate = ButtonPressed(VRInteraction::ActivateButton);
	bool activateStart = ButtonPressedFirst(VRInteraction::ActivateButton);

	auto this_controller = dynamic_pointer_cast<VRDevice>(shared_from_this());

	// interact with objects
	static jvector<shared_ptr<Object>> boundsIntersect;
	boundsIntersect.clear();
	GetScene()->Intersect(BoundingSphere(mDevicePosition, .05f), boundsIntersect);

	for (auto &it = mHovered.begin(); it != mHovered.end();) {
		if (boundsIntersect.find(dynamic_pointer_cast<Object>(*it)) == -1) {
			(*it)->HoverExit(this_controller);
			it = mHovered.erase(it);
		} else
			it++;
	}

	for (unsigned int i = 0; i < boundsIntersect.size(); i++) {
		auto g = dynamic_pointer_cast<VRInteractable>(boundsIntersect[i]);
		if (!g) continue;

		if (!mHovered.count(g)) {
			g->HoverEnter(this_controller);
			mHovered.insert(g);
		}

		if (activateStart) {
			g->ActivatePress(this_controller);
			mActivated.insert(g);
		}

		// Drag start
		if (g->Draggable() && grabStart) {
			DragOperation* d = nullptr;
			for (unsigned int k = 0; k < mDragging.size(); k++) {
				if (mDragging[k].mObject == boundsIntersect[i]) {
					d = &mDragging[k];
					break;
				}
			}
			if (!d) {
				d = &mDragging.push_back({});

				XMVECTOR op = XMLoadFloat3(&boundsIntersect[i]->WorldPosition());
				XMVECTOR or = XMLoadFloat4(&boundsIntersect[i]->WorldRotation());

				d->mObject = boundsIntersect[i];
				XMStoreFloat3(&(d->mDragPos), XMVector3Transform(op, XMLoadFloat4x4(&WorldToObject())));
				XMStoreFloat4(&(d->mDragRotation), XMQuaternionMultiply(or , XMQuaternionInverse(XMLoadFloat4(&mDeviceRotation))));

				g->DragStart(this_controller);
				TriggerHapticPulse(600);
			}
		}
	}

	if (!activate) {
		for (const auto &it : mActivated)
			it->ActivateRelease(this_controller);
		mActivated.clear();
	}

	// apply drag
	for (unsigned int i = 0; i < mDragging.size(); i++) {
		if (grab) {
			auto o = mDragging[i].mObject.get();
			o->LocalPosition(XMVector3Transform(XMLoadFloat3(&mDragging[i].mDragPos), XMLoadFloat4x4(&ObjectToWorld())));
			o->LocalRotation(XMQuaternionMultiply(XMLoadFloat4(&mDragging[i].mDragRotation), XMLoadFloat4(&mDeviceRotation)));
		} else {
			dynamic_pointer_cast<VRInteractable>(mDragging[i].mObject)->DragStop(this_controller);
			TriggerHapticPulse(500);
			mDragging.remove(i);
			i--;
		}
	}
	
	#pragma endregion
}

void VRDevice::TriggerHapticPulse(unsigned short duration) const {
	if (mHmd) mHmd->TriggerHapticPulse(mDeviceIndex, 0, duration);
}