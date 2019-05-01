#pragma once

#include <MeshRenderer.hpp>
#include <TextRenderer.hpp>
#include <Texture.hpp>
#include <Scene.hpp>
#include <Font.hpp>
#include <Camera.hpp>

#include <openvr.h>

#include "VRInteractable.hpp"

class VRDevice;
class VolumeRenderer;
class VRPieMenu;
class VRController;
class VRToolTips;
class VRDial;

class VRInteraction {
public:
	static const vr::EVRButtonId GrabButton = vr::EVRButtonId::k_EButton_Grip;
	static const vr::EVRButtonId PieMenuButton = vr::EVRButtonId::k_EButton_SteamVR_Touchpad;
	static const vr::EVRButtonId ActivateButton = vr::EVRButtonId::k_EButton_SteamVR_Trigger;
	static const vr::EVRButtonId HelpButton = vr::EVRButtonId::k_EButton_ApplicationMenu;

	enum VRTOOL {
		VRTOOL_PLANE,
		VRTOOL_PAINT,
		VRTOOL_ERASE,
		NUM_VRTOOLS
	};

	VRInteraction();
	~VRInteraction();

	void InitTools(const std::shared_ptr<Scene>& scene);
	void ProcessInput(const std::shared_ptr<Scene>& scene, const jvector<std::shared_ptr<VRDevice>>& controllers, const std::shared_ptr<VolumeRenderer>& volume, double deltaTime);

	unsigned int mPieMenuAxis;
	VRTOOL mCurTool;

private:
	class HudText : public TextRenderer, public VRInteractable {
	public:
		HudText() : TextRenderer() {};
		HudText(jwstring name) : TextRenderer(name) {};

		bool Draggable() override { return true; }
	};

	DirectX::XMFLOAT3 mLastPenPos;
	std::shared_ptr<MeshRenderer> mPen;
	std::shared_ptr<MeshRenderer> mPlane;
	std::shared_ptr<VRPieMenu> mPieMenu;
	std::shared_ptr<VRToolTips> mToolTips;
	std::shared_ptr<VRDevice> mToolController;

	std::shared_ptr<VRDial> mThresholdDial;
	std::shared_ptr<VRDial> mDensityDial;
	std::shared_ptr<VRDial> mExposureDial;
};

