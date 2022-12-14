#include "gui.h"
#include "Settings.h"

#include <Windows.h>
#include <WinUser.h>

#include "Utils.h"
#include "Global.h"
#include "../resource.h"

#include <thread> // for cpu threads counter

#include "../Lib/imgui/imgui.h"
#include "../Lib/imgui/imgui_stdlib.h"
#include "../Lib/imgui/imgui_impl_dx9.h"
#include "../Lib/imgui/imgui_impl_win32.h"
#include <fstream>

#define sameLine ImGui::SameLine();
#define push(val) ImGui::PushID(val);
#define pop() ImGui::PopID();

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (GUI::device && wideParameter != SIZE_MINIMIZED)
		{
			GUI::presentParameters.BackBufferWidth = LOWORD(longParameter);
			GUI::presentParameters.BackBufferHeight = HIWORD(longParameter);
			GUI::ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
	}break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: {
		GUI::position = MAKEPOINTS(longParameter); // set click points
	}return 0;

	case WM_MOUSEMOVE: {
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{ };

			GetWindowRect(GUI::window, &rect);

			rect.left += points.x - GUI::position.x;
			rect.top += points.y - GUI::position.y;

			if (GUI::position.x >= 0 &&
				GUI::position.x <= GUI::width &&
				GUI::position.y >= 0 && GUI::position.y <= 37)
				SetWindowPos(
					GUI::window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}

	}return 0;

	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}

void GUI::windowVisibility(int mode) noexcept {
	ShowWindow(window, mode);
}

void updateStyle() {
	if (cfg->guiStyle)
		ImGui::StyleColorsSourceVGUI();
	else
		ImGui::StyleColorsGoldSourceVGUI();
}

void GUI::CreateHWindow(const char* windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = "xs9";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		"xs9",
		windowName,
		WS_POPUP,
		100,
		100,
		width,
		height,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void GUI::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool GUI::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}

void GUI::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void GUI::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

void GUI::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;

	updateStyle();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void GUI::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void GUI::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void GUI::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}

void pushRemoveButtonStyle() {
	if (cfg->guiStyle) {
		ImGui::PushStyleColor(ImGuiCol_CustomButtonOne, ImVec4{ 0.f, 0.f, 0.f, 0.f });
		ImGui::PushStyleColor(ImGuiCol_CustomButtonTwo, ImVec4{ 0.f, 0.f, 0.f, 0.f });
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.f, 0.f, 0.f, 0.f });
	}
}

void popRemoveButtonStyle() {
	if (cfg->guiStyle) {
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
	}
}

void GUI::Render() noexcept
{
	constexpr int spacing = 112;
	int processorCount = std::thread::hardware_concurrency();
	
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize({ width, height });
	constexpr int flags = (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::Begin(title, nullptr, flags);
	ImGui::Text(title); sameLine ImGui::SetCursorPosX(width  - 98);
	pushRemoveButtonStyle();
	if (ImGui::Button("S", { 16.f, 16.f })) {
		cfg->guiStyle = !cfg->guiStyle;
		if (cfg->guiStyle)
			ImGui::StyleColorsSourceVGUI();
		else
			ImGui::StyleColorsGoldSourceVGUI();
	}
	popRemoveButtonStyle();
	sameLine
	pushRemoveButtonStyle();
	if (ImGui::Button("C", { 16.f, 16.f })) {
		ImGui::SetNextWindowPos({ 404.f, 32.f });
		ImGui::OpenPopup("configWindow");
	}
	popRemoveButtonStyle();
	sameLine
	pushRemoveButtonStyle();
	if (ImGui::Button("_", { 16.f, 16.f })) windowVisibility(VISIBLITY::MINIMIZE);  sameLine
	popRemoveButtonStyle();
	pushRemoveButtonStyle();
	if (ImGui::Button("X", { 16.f, 16.f })) isRunning = false;
	popRemoveButtonStyle();
	if (ImGui::BeginPopup("configWindow")) {
		static std::string configID = "";
		static std::string notExist = "";
		static bool exist = true;
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_ChildBg));
		if(cfg->guiStyle)
			ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.463f, 0.463f, 0.463f, 1.f));
		ImGui::SetNextItemWidth(223.f);
		ImGui::InputText("", &configID);
		ImGui::PopStyleColor(cfg->guiStyle ? 2 : 1);
		if (!exist) {
			ImGui::TextWrapped("%s config doesn't exist", notExist.c_str());
		}
		if (ImGui::Button("Save")) {
			cfg->Save(configID);
			ImGui::CloseCurrentPopup();
		}
		sameLine
		if (ImGui::Button("Load")) {
			if (std::ifstream file(configID); file.good()) {
				cfg->Load(configID);
				updateStyle();
				exist = true;
				ImGui::CloseCurrentPopup();
			} else {
				notExist = configID.size() > 0b1000 ? std::string(configID.substr(0, 5)).append("...") : configID;
				exist = false;
			}
		}
		sameLine
		if (ImGui::Button("Open Config Folder")) {
			cfg->openConfigDir();
		}
		ImGui::EndPopup();
	}
	ImGui::SetCursorPosY(32.f);
	ImGui::BeginChild("Main", { width - 16.f, height - 64.f }, true);
	ImGui::Columns(2, nullptr, false);
	ImGui::PushItemWidth(48);
	ImGui::Text("Display");
	ImGui::BeginChild("Display", { 208.f, 80.f }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::Text("Width"); ImGui::SameLine(spacing - 4);
	ImGui::SetNextItemWidth(48.f);
	ImGui::InputInt("##width", &cfg->res.width, 0, 0);
	ImGui::Text("Height"); ImGui::SameLine(spacing - 4);
	ImGui::SetNextItemWidth(48.f);
	ImGui::InputInt("##height", &cfg->res.height, 0, 0);
	ImGui::Text("Mode"); ImGui::SameLine(spacing - 4);
	ImGui::SetNextItemWidth(96.f);
	ImGui::Combo("##display", &cfg->res.displayMode, "Default\0Windowed\0Fullscreen\0Windowed Fullscreen\0");
	ImGui::EndChild();

	ImGui::Text("Debug");
	ImGui::BeginChild("Debug", { 208.f, 216.f }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	push("Insecure")
	ImGui::Checkbox("Insecure", &cfg->insecure);

	sameLine
	ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Your game files signatures will not be validated\nand you will be not allowed to join this VAC secure server.");
	pop()

	push("Allow Debuging")
	ImGui::Checkbox("Allow Debuging", &cfg->allowDebug);
	sameLine
	ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Allows to debug the game.");
	pop()

	push("Log Console")
	ImGui::Checkbox("Log Console", &cfg->logConsole);
	sameLine
	ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Game will log console in ../csgo/console.log.");
	pop()

	push("Show Console")
	ImGui::Checkbox("Console on Startup", &cfg->consoleOnStartup);
	sameLine
	ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Makes the game start with the console opened.");
	pop()

	push("To Console")
		ImGui::Checkbox("To Console", &cfg->toConsole);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Starts the engine directly in the console,\nunless a map is specified with +map.");
	pop()

	push("Crashdialog")
		ImGui::Checkbox("No Crash Dialog", &cfg->noCrashDialog);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Suppresses some \"memory could not be read\" unhandled errors.");
	pop()

	push("Hijack")
	ImGui::Checkbox("Hijack", &cfg->hijack);
	sameLine
	ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Hijacks the game.");
	pop()

	push("Dev")
	ImGui::Checkbox("Dev", &cfg->dev);

	sameLine
	ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Enables developer mode. Also disables the automatic\nloading of menu background maps and stops the quit dialog from appearing on exit.");
	pop()

	push("Game")
	ImGui::Text("Game"); ImGui::SameLine(spacing - 8);
	ImGui::SetNextItemWidth(64.f);
	ImGui::InputText("##game", cfg->game, sizeof(cfg->game));
	sameLine
	ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Folder of the game.");
	pop()

	ImGui::EndChild();

	push("Threads")
	ImGui::Text("Threads"); ImGui::SameLine(spacing);
	ImGui::InputInt("##threads", &cfg->threads, 0, 0);
	sameLine
	ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Sets the amount of processor threads that CS:GO will use.");
	cfg->threads = std::clamp(cfg->threads, 0, processorCount);
	pop()

	push("heapsize")
	ImGui::Text("Heapsize"); ImGui::SameLine(spacing);
	ImGui::InputInt("##heap", &cfg->heapsize, 0, 0);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Specifies the heapsize (in kilobytes) which the game will use.");
	pop()

	push("Tickrate")
	ImGui::Text("Tickrate"); ImGui::SameLine(spacing);
	ImGui::InputInt("##tickrate", &cfg->tickrate, 0, 0);
	sameLine
	ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Sets the tick rate of any \"Offline With Bots\" games,\nor any servers that you host via your client.");
	pop()

	push("Refresh")
	ImGui::Text("Refresh"); ImGui::SameLine(spacing);
	ImGui::InputInt("##refresh", &cfg->refresh, 0, 0);
	sameLine
	ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Sets the refresh rate of your client. The refresh rate (in hz) is\nhow many times per second your monitor will update - at 60hz,\nyour monitor is effectively showing 60 frames per second. For 144hz monitors,\nset this to 144 so that your game refreshes 144 times a second.");
	pop()

	ImGui::NextColumn();				/*					NEXT COLUMN					*/

	ImGui::Dummy({ 0.f, 16.f });
	push("Config")
	ImGui::Text("Execute Config"); ImGui::SameLine(spacing);
	ImGui::SetNextItemWidth(128.f);
	ImGui::InputText("##exec", cfg->execConfig, sizeof(cfg->execConfig));
	sameLine
	ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("This launch option will execute all commands a specified file.");
	pop()

	push("Language")
	ImGui::Text("Language"); ImGui::SameLine(spacing);
	ImGui::SetNextItemWidth(128.f);
	ImGui::InputText("##lang", cfg->language, sizeof(cfg->language));
	sameLine
	ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Makes CS:GO start in the language that you specified.");
	pop()

	push("Server")
	ImGui::Text("Server"); ImGui::SameLine(spacing);
	ImGui::SetNextItemWidth(128.f);
	ImGui::InputText("##server", cfg->serverConnect, sizeof(cfg->serverConnect));
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Connects with the server automatically.");
	pop()

	push("thirdparty")
	ImGui::Checkbox("Allow Third Party Software", &cfg->thirdParty);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Allows some applications interact with CS:GO by injecting directly into the game process. May lower your Trust Factor!");
	pop()
	push("high")
	ImGui::Checkbox("High Priority", &cfg->highPriority);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("It will make the CS:GO process higher priority, and therefore, your computer will give\nresources (RAM, CPU, etc)to CS:GO instead of any other processes that you are running.");
	pop()
	push("vertex")
	ImGui::Checkbox("Limit Vertex Shaders", &cfg->limitVSConst);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("It will limit the game's number of vertex shaders to a maximum of 256,\nwhich may offer an FPS boost.");
	pop()
	push("vsync")
	ImGui::Checkbox("Force NoVSync", &cfg->forceNoVSync);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("It disables VSync which often causes more problems than it solves");
	pop()
	push("emulategl")
	ImGui::Checkbox("Emulate GL", &cfg->emulateGL);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("It will make CS:GO emulate OpenGL, which can cause an FPS boost for some players.");
	pop()
	push("DX9Ex")
	ImGui::Checkbox("Disable DX9Ex", &cfg->disableDX9Ex);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("It will disable DirectX 9Ex, but will decrease FPS for most people.");
	pop()
	push("softparticles")
	ImGui::Checkbox("Disable Soft Particles on Default", &cfg->softParticlesDefaultOFF);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("It will make particles be rendered without feathering (scene depth blending),\nwhich can cause an FPS boost but decrease the graphics quality of your game.");
	pop()
	push("defaultcfg")
	ImGui::Checkbox("Default Config on Startup", &cfg->defaultCfg);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("This launch option will force your game to start up with\nthe default configuration settings for the game");
	pop()
	push("AAFonts")
	ImGui::Checkbox("Disable Anti-Aliasing Fonts", &cfg->noAAFonts);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("It disables Anti-Aliasing for Fonts");
	pop()
	push("dHLTV")
	ImGui::Checkbox("Disable HLTV", &cfg->noHLTV);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("This launch option turns off all Source TV features.");
	pop()
	push("dPreload")
	ImGui::Checkbox("Disable Preload", &cfg->noPreload);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("This launch option forces models not to be preloaded. Whilst this will have a different\neffect for every computer, chances are, this command will make games/maps load faster,\nbut may cause worse performance during actual gameplay.");
	pop()
	push("dBrowser")
	ImGui::Checkbox("Disable Browser", &cfg->noBrowser);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("It should disable the in-game browser that shows\nin servers as welcome messages (not the Steam overlay).");
	pop()
	push("dIntro")
	ImGui::Checkbox("Disable Intro", &cfg->noVideo);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("This launch option makes the game skip the Valve/CS:GO intro video\nwhen you load it up - i.e. you'll go straight to the game.");
	pop()
	push("dJoystick")
	ImGui::Checkbox("Disable Joystick Support", &cfg->noJoystick);
	sameLine
		ImGui::TextDisabled("?");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("This launch option makes the game drop all joystick support,\nwhich can decrease the amount of RAM it uses.");
	pop()

	ImGui::PopItemWidth();
	ImGui::Columns(1);
	ImGui::EndChild();
	prepareConfig();

	std::string buttonText = "Start";

	if (isSteamRunning())
		buttonText = "Steam not detected";
	else if (FindWindowW(L"Valve001", nullptr))
		buttonText = "CS:GO detected";

	ImGui::SetNextItemWidth(width - static_cast<int>(ImGui::CalcTextSize(buttonText.c_str()).x) - 34.f);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_ChildBg));
	if (cfg->guiStyle)
		ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.463f, 0.463f, 0.463f, 1.f));

	ImGui::InputText("##output", &global->gameArgs, ImGuiInputTextFlags_ReadOnly);
	ImGui::PopStyleColor(cfg->guiStyle ? 2 : 1);
	sameLine
	if (isSteamRunning() || FindWindowW(L"Valve001", nullptr)) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button(buttonText.c_str())) {
		startTheGame();
	}

	if (isSteamRunning() || FindWindowW(L"Valve001", nullptr)) {
		ImGui::EndDisabled();
	}


	ImGui::End();
}
