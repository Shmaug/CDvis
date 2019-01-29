#pragma once

#include <MeshRenderer.hpp>
#include <openvr.h>

class VRDevice;

class VRInteractable {
public:
	virtual void ButtonPress(std::shared_ptr<VRDevice> device, vr::EVRButtonId button) {};
	virtual void ButtonRelease(std::shared_ptr<VRDevice> device, vr::EVRButtonId button) {};

	virtual void DragStart(std::shared_ptr<VRDevice> device) {};
	virtual void DragStop(std::shared_ptr<VRDevice> device) {};

	virtual bool Draggable() { return false; }
};

class VRDraggableMeshRenderer : public MeshRenderer, public VRInteractable {
public:
	VRDraggableMeshRenderer(jwstring name) : MeshRenderer(name) {};

	virtual bool Draggable() override { return true; }
};