#pragma once

#include <IJaeGame.hpp>
#include <Camera.hpp>
#include <openvr.h>
#include <Mesh.hpp>
#include <Material.hpp>
#include <MeshRenderer.hpp>
#include <Scene.hpp>
#include <Font.hpp>
#include <Texture.hpp>

#include "VRInteractable.hpp"

class VolumeRenderer;
class VRCamera;
class VRDevice;
class VRInteraction;

class cdvis : public IJaeGame {
public:
	cdvis();
	~cdvis();

	void Initialize() override;
	void WindowEvent(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) override;
	void OnResize() override;
	void DoFrame() override;

	bool InitializeVR();
	void VRCreateDevice(unsigned int index);
	void VRGetRenderModel(unsigned int index, MeshRenderer* renderer);

	void Update(double totalTime, double deltaTime);
	void PreRender(const std::shared_ptr<CommandList>& commandList);
	void Render(const std::shared_ptr<Camera>& camera, const std::shared_ptr<CommandList>& commandList);

	void BrowseImage();
	void BrowseVolume();
	void BrowseMask();

private:
	double mfps;

	class VRLight : public MeshRenderer, public VRInteractable {
	public:
		VRLight() : MeshRenderer(), mActivated(false) {};
		VRLight(jwstring name) : MeshRenderer(name), mActivated(false) {};

		bool mActivated;

		bool Draggable() override { return true; }
		void ActivatePress(const std::shared_ptr<VRDevice>& controller) override {
			mActivated = !mActivated;
		}
	};

	std::shared_ptr<Font> mArial;

	std::shared_ptr<Scene> mScene;
	std::shared_ptr<Camera> mWindowCamera;
	std::shared_ptr<VRCamera> mVRCamera;
	std::shared_ptr<MeshRenderer> mFloor;

	std::shared_ptr<VolumeRenderer> mVolume;
	std::shared_ptr<VRInteraction> mVRInteraction;
	std::shared_ptr<VRLight> mLight;

	// OpenVR
	vr::IVRSystem* mHmd;
	vr::TrackedDevicePose_t mVRTrackedDevices[vr::k_unMaxTrackedDeviceCount];
	vr::IVRRenderModels* mVRRenderModelInterface;

	// All tracked devices
	jvector<std::shared_ptr<VRDevice>> mVRDevices;
	// Meshes for all devices
	std::unordered_map<jstring, std::shared_ptr<Mesh>> mVRMeshes;
	// Jae3d textures for all device textures
	std::unordered_map<vr::TextureID_t, std::shared_ptr<Texture>> mVRTextures;

	bool mVREnable = false;
	bool m3DTVEnable = false;
	float mTVEyeSeparation = 0.f;

	wchar_t mPerfBuffer[1024]; // performance overlay text
	float mFrameTimes[128];
	unsigned int mFrameTimeIndex;
	bool mDebugDraw = false;
	bool mWireframe = false;
	bool mPerfOverlay = false;
	float mCameraZ = -2.0f;
};

