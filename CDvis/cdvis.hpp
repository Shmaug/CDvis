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

class VRCamera;
class VolumeRenderer;

class cdvis : public IJaeGame {
public:
	~cdvis();

	void Initialize() override;
	void WindowEvent(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) override;
	void OnResize() override;
	void DoFrame() override;

	bool InitializeVR();
	void VREvent(const vr::VREvent_t &event);
	void VRCreateDevice(unsigned int index);
	void VRGetRenderModel(unsigned int index, MeshRenderer* renderer);

	void Update(double totalTime, double deltaTime);
	void Render(std::shared_ptr<Camera> camera, std::shared_ptr<CommandList> commandList);

	void BrowseImage();
	void BrowseVolume();

private:
	double mfps;

	std::shared_ptr<Scene> mScene;
	std::shared_ptr<Camera> mWindowCamera;
	std::shared_ptr<VRCamera> mVRCamera;
	std::shared_ptr<Font> mArial;
	std::shared_ptr<VolumeRenderer> mVolume;
	std::shared_ptr<Material> mDepthMaterial;

	// VR
	vr::IVRSystem* mHmd;
	vr::TrackedDevicePose_t mVRTrackedDevices[vr::k_unMaxTrackedDeviceCount];
	vr::IVRRenderModels* mVRRenderModelInterface;

	std::shared_ptr<Material> mVRMaterial;
	jvector<std::shared_ptr<MeshRenderer>> mVRDevices;
	std::unordered_map<jstring, std::shared_ptr<Mesh>> mVRMeshes;
	std::unordered_map<vr::TextureID_t, std::shared_ptr<Texture>> mVRTextures;

	bool mVREnable = false;

	wchar_t mPerfBuffer[1024]; // performance overlay text
	float mFrameTimes[128];
	unsigned int mFrameTimeIndex;
	bool mDebugDraw = false;
	bool mWireframe = false;
	float mCameraZ = -2.0f;
};

