#include "VRDevice.hpp"

#include "VRUtil.hpp"

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
}

void VRDevice::TriggerHapticPulse(unsigned short duration) const {
	if (mHmd) mHmd->TriggerHapticPulse(mDeviceIndex, 0, duration);
}