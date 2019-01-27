#pragma once

#include <MeshRenderer.hpp>
#include <Texture.hpp>

#include <openvr.h>

class VRDevice;
class VRButton;
class VolumeRenderer;

class VRTools : public Object {
public:
	VRTools(jwstring name);
	~VRTools();

	void InitTools();
	void ProcessInput(jvector<std::shared_ptr<VRDevice>> &controllers, std::shared_ptr<VolumeRenderer> volume);

	std::shared_ptr<MeshRenderer> mLight;

private:
	bool mMenuShown;
	void ShowMenu(std::shared_ptr<VRDevice> attachController);
	void HideMenu();

	std::shared_ptr<MeshRenderer> mPen;
	std::shared_ptr<MeshRenderer> mRotateArrows[8];
	std::shared_ptr<VRDevice> mDragging;
	DirectX::XMFLOAT3 mDragPos;
	DirectX::XMFLOAT4 mDragRotation;
};

