#pragma once

#include <MeshRenderer.hpp>
#include <openvr.h>

class VRDevice;

class VRInteractable {
public:
	virtual void ActivatePress  (const std::shared_ptr<VRDevice>& device) {};
	virtual void ActivateRelease(const std::shared_ptr<VRDevice>& device) {};

	virtual void HoverEnter(const std::shared_ptr<VRDevice>& device) {};
	virtual void HoverExit (const std::shared_ptr<VRDevice>& device) {};

	virtual void DragStart(const std::shared_ptr<VRDevice>& device) {};
	virtual void DragStop (const std::shared_ptr<VRDevice>& device) {};

	virtual bool Draggable() { return false; }
};