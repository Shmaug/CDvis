#pragma once

#include <MeshRenderer.hpp>

#include "VRInteractable.hpp"

class VRDial : public MeshRenderer, public VRInteractable {
public:
	VRDial(jwstring name);
	~VRDial();

	virtual void DragStart(const std::shared_ptr<VRDevice>& device) override;
	virtual void DragStop (const std::shared_ptr<VRDevice>& device) override;
};

