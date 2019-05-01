#pragma once

#include <MeshRenderer.hpp>
#include <TextRenderer.hpp>

#include "VRInteractable.hpp"

class VRDevice;

class VRDial : public MeshRenderer, public VRInteractable {
public:
	VRDial(const jwstring& name, float val = .2f, float min = 0.f, float max = 1.f, unsigned int steps = 20);
	~VRDial();

	void Create();

	inline float Value() const { return mValue; }
	void Value(float v);

	bool Draggable() { return true; }

	void Drag(const std::shared_ptr<Object>& this_obj, const std::shared_ptr<VRDevice>& device, const DirectX::XMVECTOR& newPos, const DirectX::XMVECTOR& newRot) override;

	float mMin;
	float mMax;
	unsigned int mSteps;

private:
	std::shared_ptr<TextRenderer> mLabel;

	float mValue;
};

