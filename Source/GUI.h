#pragma once
#include <d3d9.h>
#define title "Counter-Strike: Global Offensive Loader"

enum VISIBLITY{
	HIDE,
	NORMAL,
	SHOWMINIMIZED,
	MAXIMIZE,
	SHOWNOACTIVATE,
	SHOW,
	MINIMIZE,
	SHOWMINNOACTIVE,
	SHOWNA,
	RESTORE,
	SHOWDEFAULT,
	MAX
};

namespace GUI
{
	// constant window size
	constexpr int width = 640;
	constexpr int height = 512;

	// when this changes, exit threads
	// and close menu :)
	inline bool isRunning = true;

	// winapi window vars
	inline HWND window = nullptr;
	inline WNDCLASSEX windowClass = { };

	// points for window movement
	inline POINTS position = { };

	// direct x state vars
	inline PDIRECT3D9 d3d = nullptr;
	inline LPDIRECT3DDEVICE9 device = nullptr;
	inline D3DPRESENT_PARAMETERS presentParameters = { };

	// handle window visibility
	void windowVisibility(int mode) noexcept;

	// handle window creation & destruction
	void CreateHWindow(const char* windowName) noexcept;
	void DestroyHWindow() noexcept;

	// handle device creation & destruction
	bool CreateDevice() noexcept;
	void ResetDevice() noexcept;
	void DestroyDevice() noexcept;

	// handle ImGui creation & destruction
	void CreateImGui() noexcept;
	void DestroyImGui() noexcept;

	void BeginRender() noexcept;
	void EndRender() noexcept;
	void Render() noexcept;
}
