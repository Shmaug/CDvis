#pragma once

#include <IJaeGame.hpp>
#include <Camera.hpp>

class cdvis : public IJaeGame {
public:
	~cdvis();

	void Initialize();
	void OnResize();
	void WindowEvent(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	void Update(double totalTime, double deltaTime);
	void Render(std::shared_ptr<Camera> camera, std::shared_ptr<CommandList> commandList);
	void DoFrame();

private:
	double mfps;
};

