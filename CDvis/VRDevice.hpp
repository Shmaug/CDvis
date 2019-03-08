#pragma once

#include <MeshRenderer.hpp>
#include <openvr.h>
#include <unordered_set>

#include "VRInteractable.hpp"

class VRInteraction;

class VRDevice : public MeshRenderer {
public:
	VRDevice(jwstring name, unsigned int deviceIndex);
	~VRDevice();

	// Update tracking and button state
	void UpdateDevice(vr::IVRSystem* hmd, const vr::TrackedDevicePose_t& pose);

	void TriggerHapticPulse(unsigned short duration) const;

	unsigned int DeviceIndex() const { return mDeviceIndex; }
	const vr::VRControllerState_t& GetState() { return mState; }
	const vr::VRControllerState_t& GetLastState() { return mLastState; }

	bool ButtonPressed(vr::EVRButtonId button) const {
		return mState.ulButtonPressed & vr::ButtonMaskFromId(button);
	}
	bool ButtonPressedFirst(vr::EVRButtonId button) const {
		return (mState.ulButtonPressed & vr::ButtonMaskFromId(button)) && !(mLastState.ulButtonPressed & vr::ButtonMaskFromId(button));
	}
	bool ButtonReleased(vr::EVRButtonId button) const {
		return !(mState.ulButtonPressed & vr::ButtonMaskFromId(button));
	}
	bool ButtonReleasedFirst(vr::EVRButtonId button) const {
		return !(mState.ulButtonPressed & vr::ButtonMaskFromId(button)) && (mLastState.ulButtonPressed & vr::ButtonMaskFromId(button));
	}

	bool ButtonTouched(vr::EVRButtonId button) const {
		return mState.ulButtonTouched & vr::ButtonMaskFromId(button);
	}
	bool ButtonTouchedFirst(vr::EVRButtonId button) const {
		return (mState.ulButtonTouched & vr::ButtonMaskFromId(button)) && !(mLastState.ulButtonTouched & vr::ButtonMaskFromId(button));
	}
	bool ButtonTouchReleased(vr::EVRButtonId button) const {
		return !(mState.ulButtonTouched & vr::ButtonMaskFromId(button));
	}
	bool ButtonTouchReleasedFirst(vr::EVRButtonId button) const {
		return !(mState.ulButtonTouched & vr::ButtonMaskFromId(button)) && (mLastState.ulButtonTouched & vr::ButtonMaskFromId(button));
	}

	DirectX::XMFLOAT3 DevicePosition() const { return mDevicePosition; }
	DirectX::XMFLOAT4 DeviceRotation() const { return mDeviceRotation; }
	DirectX::XMFLOAT3 LastDevicePosition() const { return mLastDevicePosition; }
	DirectX::XMFLOAT4 LastDeviceRotation() const { return mLastDeviceRotation; }
	DirectX::XMFLOAT3 DeltaDevicePosition() const { return mDeltaDevicePosition; }
	DirectX::XMFLOAT4 DeltaDeviceRotation() const { return mDeltaDeviceRotation; }

	bool Visible() override { return mVisible && mTracking; }

	bool mTracking;

private:
	friend class VRInteraction;
	const unsigned int mDeviceIndex;

	vr::IVRSystem* mHmd;

	vr::VRControllerState_t mState;
	vr::VRControllerState_t mLastState;

	DirectX::XMFLOAT3 mDevicePosition;
	DirectX::XMFLOAT4 mDeviceRotation;

	DirectX::XMFLOAT3 mLastDevicePosition;
	DirectX::XMFLOAT4 mLastDeviceRotation;

	DirectX::XMFLOAT3 mDeltaDevicePosition;
	DirectX::XMFLOAT4 mDeltaDeviceRotation;

	struct DragOperation {
		DirectX::XMFLOAT3 mDragPos;
		DirectX::XMFLOAT4 mDragRotation;
		std::shared_ptr<Object> mObject;
	};
	jvector<DragOperation> mDragging;
	std::unordered_set<std::shared_ptr<VRInteractable>> mHovered;
	std::unordered_set<std::shared_ptr<VRInteractable>> mActivated;
};

