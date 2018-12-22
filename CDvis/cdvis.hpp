#pragma once

#include <IJaeGame.hpp>

class cdvis : public IJaeGame {
public:
	~cdvis();

	void Initialize();
	void OnResize();
	void WindowEvent(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	void Update(double totalTime, double deltaTime);
	void Render(std::shared_ptr<CommandList> commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv);
	void DoFrame();

private:
	double mfps;
};

