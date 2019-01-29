#pragma once

#include <MeshRenderer.hpp>
#include <Texture.hpp>

#include <openvr.h>

class VRDevice;
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

	struct DragOperation {
		DirectX::XMFLOAT3 mDragPos;
		DirectX::XMFLOAT4 mDragRotation;
		std::shared_ptr<VRDevice> mController;
		std::shared_ptr<VRDevice> mSecondController;
		std::shared_ptr<Object> mObject;
	};

	std::shared_ptr<MeshRenderer> mPen;
	std::shared_ptr<MeshRenderer> mRotateArrows[8];
	jvector<DragOperation> mDragging;
};

