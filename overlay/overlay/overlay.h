#pragma once
#include "../includes.h"
#include "../features/menu/gui.h"
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

enum Mode {
	CreateWin,
	Banding
};

inline class c_Overlay {
private:
	bool CreateDeviceD3D(HWND hWnd);
	void CleanupDeviceD3D();
	void CreateRenderTarget();
	void CleanupRenderTarget();

	bool showmenu = false;
	RECT GameRect{};
	POINT GamePoint{};
public:
	void Render(int mode, std::wstring windowname, std::wstring windowclass);

}overlay;

