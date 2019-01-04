#pragma once

#include <IJaeGame.hpp>
#include <Camera.hpp>
#include <openvr.h>

class cdvis : public IJaeGame {
public:
	~cdvis();

	void Initialize() override;
	void WindowEvent(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) override;
	void OnResize() override;
	void DoFrame() override;

	bool InitializeVR();
	void cdvis::VREvent(const vr::VREvent_t &event);

	void Update(double totalTime, double deltaTime);
	void Render(std::shared_ptr<Camera> camera, std::shared_ptr<CommandList> commandList);

	void BrowseImage();
	void BrowseVolume();

private:
	double mfps;
};

