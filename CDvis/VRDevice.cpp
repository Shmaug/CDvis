#include "VRDevice.hpp"

#include "VRUtil.hpp"
#include "VRInteractable.hpp"
#include "VRInteraction.hpp"

#include <Scene.hpp>

using namespace std;
using namespace DirectX;

VRDevice::VRDevice(jwstring name, unsigned int index)
	: MeshRenderer(name), mDeviceIndex(index), mHmd(nullptr),
	mDeltaDevicePosition(XMFLOAT3()), mDeltaDeviceRotation(XMFLOAT4(0,0,0,1)), mDevicePosition(XMFLOAT3()), mDeviceRotation(XMFLOAT4(0,0,0,1)),
	mLastDevicePosition(XMFLOAT3()), mLastDeviceRotation(XMFLOAT4(0,0,0,1)), mState(), mLastState(), mTracking(false) {}
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
	GetScene()->Intersect(BoundingSphere(mDevicePosition, .025f), boundsIntersect);

	for (auto it = mHovered.begin(); it != mHovered.end();) {
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

		// track activating this object
		if (activateStart) {
			g->ActivatePress(this_controller);
			mActivated.insert(g);
		}

		// Drag start
		if (grabStart && g->Draggable()) {
			DragOperation* d = nullptr;
			for (unsigned int k = 0; k < mDragging.size(); k++) {
				auto o = mDragging[k].mObject.lock();
				if (!o) {
					mDragging.remove(k);
					k--;
					continue;
				}
				if (dynamic_pointer_cast<Object>(o) == boundsIntersect[i]) {
					d = &mDragging[k];
					break;
				}
			}
			if (!d) {
				d = &mDragging.push_back({});

				XMVECTOR op = XMLoadFloat3(&boundsIntersect[i]->WorldPosition());
				XMVECTOR or = XMLoadFloat4(&boundsIntersect[i]->WorldRotation());

				d->mObject = dynamic_pointer_cast<VRInteractable>(boundsIntersect[i]);
				XMStoreFloat3(&(d->mDragPos), XMVector3Transform(op, XMLoadFloat4x4(&WorldToObject())));
				XMStoreFloat4(&(d->mDragRotation), XMQuaternionMultiply(or , XMQuaternionInverse(XMLoadFloat4(&mDeviceRotation))));

				TriggerHapticPulse(600);
				g->DragStart(this_controller);
			}
		}
	}

	if (!activate) {
		for (const auto& it : mActivated)
			it->ActivateRelease(this_controller);
		mActivated.clear();
	}

	// apply drag
	for (unsigned int i = 0; i < mDragging.size(); i++) {
		auto obj = mDragging[i].mObject.lock();
		if (!obj) {
			mDragging.remove(i);
			i--;
			continue;
		}

		if (grab) {
			auto object = dynamic_pointer_cast<Object>(obj);
			XMVECTOR pos = XMVector3Transform(XMLoadFloat3(&mDragging[i].mDragPos), XMLoadFloat4x4(&ObjectToWorld()));
			XMVECTOR rot = XMQuaternionMultiply(XMLoadFloat4(&mDragging[i].mDragRotation), XMLoadFloat4(&mDeviceRotation));
			if (auto parent = object->Parent()) {
				pos = XMVector3Transform(pos, XMLoadFloat4x4(&parent->WorldToObject()));
				rot = XMQuaternionMultiply(rot, XMQuaternionInverse(XMLoadFloat4(&parent->WorldRotation())));
			}
			obj->Drag(object, this_controller, pos, rot);
		} else {
			obj->DragStop(this_controller);
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