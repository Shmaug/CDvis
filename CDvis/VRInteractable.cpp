#include "VRInteractable.hpp"
#include "VRDevice.hpp"

using namespace std;
using namespace DirectX;

void VRInteractable::Drag(const shared_ptr<Object>& this_obj, const shared_ptr<VRDevice>& device, const XMVECTOR& newPos, const XMVECTOR& newRot) {
	this_obj->LocalPosition(newPos);
	this_obj->LocalRotation(newRot);
};
