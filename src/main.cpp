#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include "implot.h"
#include "ImGuiFileDialog.h"

// Standard Library Includes
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <stdio.h>
#include <string>
#include <thread>
#include <vector>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Windows Specific
#include <windows.h>
#include <ShellScalingApi.h>
#include <timeapi.h>

// Application Includes
#include "types.hpp"
#include "LuaScriptManager.hpp"
#include "plot_types.hpp"
#include "signal_processing.hpp"
#include "ui_state.hpp"

// -------------------------------------------------------------------------
// GLOBAL STATE
// -------------------------------------------------------------------------
std::mutex stateMutex;
std::atomic<bool> appRunning(true);

// Playback mode state
PlaybackMode currentPlaybackMode = PlaybackMode::ONLINE;

// Offline playback state
struct OfflinePlaybackState {
  double minTime = 0.0;
  double maxTime = 0.0;
  double currentWindowStart = 0.0;
  double windowWidth = 10.0;
  bool fileLoaded = false;
  std::string loadedFilePath;
} offlineState;

// The Registry: Maps a string ID to the actual data buffer
std::map<std::string, Signal> signalRegistry;

// Lua Script Manager
LuaScriptManager luaScriptManager;

// UI plot state
UIPlotState uiPlotState;

// Tier 3: Frame tracking for Lua callbacks
static uint64_t frameNumber = 0;
static auto lastFrameTime = std::chrono::high_resolution_clock::now();

// Include application logic (order matters due to dependencies)
#include "layout_persistence.hpp"
#include "plot_rendering.hpp"
#include "control_rendering.hpp"
#include "image_rendering.hpp"

// DirectX 11 Globals
ID3D11Device*            g_pd3dDevice = nullptr;
ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Forward Declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// -------------------------------------------------------------------------
// STYLING
// -------------------------------------------------------------------------
void SetupImGuiStyle() {
  // 1. Theme and Colors
  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();

  // Rounding
  style.WindowRounding = 5.0f;
  style.FrameRounding = 5.0f;
  style.PopupRounding = 5.0f;
  style.ScrollbarRounding = 5.0f;
  style.GrabRounding = 5.0f;
  style.TabRounding = 5.0f;

  // Padding
  style.WindowPadding = ImVec2(10, 10);
  style.FramePadding = ImVec2(5, 5);
  style.ItemSpacing = ImVec2(10, 8);

  // Color Tweaks
  ImVec4 *colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.30f, 0.35f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.32f, 0.38f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.30f, 0.35f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.32f, 0.38f, 1.00f);

  // 2. ImPlot Styling
  ImPlotStyle &plotStyle = ImPlot::GetStyle();
  plotStyle.LineWeight = 2.0f;
  plotStyle.MarkerSize = 4.0f;
}

// -------------------------------------------------------------------------
// MAIN
// -------------------------------------------------------------------------
int main(int, char**) {
    // Enable DPI awareness
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    // Create application window
    // ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"SignaKit", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"SignaKit Analyzer", WS_OVERLAPPEDWINDOW, 100, 100, 1600, 900, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Optional, might complicate things for now, leave off to be safe

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Fonts and Style
    if (!io.Fonts->AddFontFromFileTTF("Roboto-Medium.ttf", 20.0f)) {
        printf("Warning: Could not load font Roboto-Medium.ttf, using default font\n");
    }
    SetupImGuiStyle();

    // Initialize Winsock (needed for sockpp/lua sockets)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return -1;
    }

    // Load Lua Scripts
    printf("Loading Lua scripts...\n");
    luaScriptManager.setAppRunningPtr(&appRunning);
    luaScriptManager.setSignalRegistry(&signalRegistry);
    luaScriptManager.loadScriptsFromDirectory("scripts");

    // Enable high-res timer
    timeBeginPeriod(1);

    // Main loop
    while (appRunning) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                appRunning = false;
        }
        if (!appRunning) break;

        // Handle Window Resize (Deferred)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Handle Layout Loading
        if (!uiPlotState.pendingLoadFilename.empty()) {
            LayoutData data;
            if (LoadLayout(uiPlotState.pendingLoadFilename, data)) {
                 if (!data.imguiSettings.empty()) {
                       ImGui::LoadIniSettingsFromMemory(data.imguiSettings.c_str(), data.imguiSettings.size());
                 }
                 uiPlotState.activePlots = data.plots;
                 uiPlotState.nextPlotId = data.nextPlotId;
                 uiPlotState.activeReadoutBoxes = data.readouts;
                 uiPlotState.nextReadoutBoxId = data.nextReadoutId;
                 uiPlotState.activeXYPlots = data.xyPlots;
                 uiPlotState.nextXYPlotId = data.nextXYPlotId;
                 uiPlotState.activeHistograms = data.histograms;
                 uiPlotState.nextHistogramId = data.nextHistogramId;
                 uiPlotState.activeFFTs = data.ffts;
                 uiPlotState.nextFFTId = data.nextFFTId;
                 uiPlotState.activeSpectrograms = data.spectrograms;
                 uiPlotState.nextSpectrogramId = data.nextSpectrogramId;
                 uiPlotState.activeButtons = data.buttons;
                 uiPlotState.nextButtonId = data.nextButtonId;
                 uiPlotState.activeToggles = data.toggles;
                 uiPlotState.nextToggleId = data.nextToggleId;
                 uiPlotState.activeTextInputs = data.textInputs;
                 uiPlotState.nextTextInputId = data.nextTextInputId;
                 uiPlotState.editMode = data.editMode;
                 uiPlotState.managedByImGui = !data.imguiSettings.empty();
            }
            uiPlotState.pendingLoadFilename.clear();
        }

        // Start Frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Application Logic
        {
            std::lock_guard<std::mutex> lock(stateMutex);

            frameNumber++;
            auto currentFrameTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = currentFrameTime - lastFrameTime;
            double deltaTime = elapsed.count();
            lastFrameTime = currentFrameTime;

            int totalPlots = (int)(uiPlotState.activePlots.size() +
                                uiPlotState.activeReadoutBoxes.size() +
                                uiPlotState.activeXYPlots.size() +
                                uiPlotState.activeHistograms.size() +
                                uiPlotState.activeFFTs.size() +
                                uiPlotState.activeSpectrograms.size());

            luaScriptManager.executeFrameCallbacks(signalRegistry, frameNumber, deltaTime, totalPlots, &uiPlotState);

            float menuBarHeight = ImGui::GetFrameHeight();
            RenderMenuBar(uiPlotState);
            RenderSignalBrowser(uiPlotState, menuBarHeight);
            RenderTimePlots(uiPlotState, menuBarHeight);
            RenderReadoutBoxes(uiPlotState, menuBarHeight);
            RenderXYPlots(uiPlotState, menuBarHeight);
            RenderHistograms(uiPlotState, menuBarHeight);
            RenderFFTPlots(uiPlotState, menuBarHeight);
            RenderSpectrograms(uiPlotState, menuBarHeight);
            RenderMemoryProfiler(uiPlotState);
            RenderButtonControls(uiPlotState, menuBarHeight);
            RenderToggleControls(uiPlotState, menuBarHeight);
            RenderTextInputControls(uiPlotState, menuBarHeight);
            RenderImageWindows(uiPlotState, menuBarHeight);
            RenderTimeSlider();
            RenderFileDialogs(uiPlotState);

            // Cleanup closed windows
            uiPlotState.activePlots.erase(std::remove_if(uiPlotState.activePlots.begin(), uiPlotState.activePlots.end(), [](const PlotWindow &p) { return !p.isOpen; }), uiPlotState.activePlots.end());
            uiPlotState.activeReadoutBoxes.erase(std::remove_if(uiPlotState.activeReadoutBoxes.begin(), uiPlotState.activeReadoutBoxes.end(), [](const ReadoutBox &r) { return !r.isOpen; }), uiPlotState.activeReadoutBoxes.end());
            uiPlotState.activeXYPlots.erase(std::remove_if(uiPlotState.activeXYPlots.begin(), uiPlotState.activeXYPlots.end(), [](const XYPlotWindow &xy) { return !xy.isOpen; }), uiPlotState.activeXYPlots.end());
            uiPlotState.activeHistograms.erase(std::remove_if(uiPlotState.activeHistograms.begin(), uiPlotState.activeHistograms.end(), [](const HistogramWindow &h) { return !h.isOpen; }), uiPlotState.activeHistograms.end());
            uiPlotState.activeFFTs.erase(std::remove_if(uiPlotState.activeFFTs.begin(), uiPlotState.activeFFTs.end(), [](const FFTWindow &f) { return !f.isOpen; }), uiPlotState.activeFFTs.end());
            uiPlotState.activeSpectrograms.erase(std::remove_if(uiPlotState.activeSpectrograms.begin(), uiPlotState.activeSpectrograms.end(), [](const SpectrogramWindow &s) { return !s.isOpen; }), uiPlotState.activeSpectrograms.end());
            uiPlotState.activeImageWindows.erase(std::remove_if(uiPlotState.activeImageWindows.begin(), uiPlotState.activeImageWindows.end(), [](const ImageWindow &img) { return !img.isOpen; }), uiPlotState.activeImageWindows.end());
            uiPlotState.activeButtons.erase(std::remove_if(uiPlotState.activeButtons.begin(), uiPlotState.activeButtons.end(), [](const ButtonControl &b) { return !b.isOpen; }), uiPlotState.activeButtons.end());
            uiPlotState.activeToggles.erase(std::remove_if(uiPlotState.activeToggles.begin(), uiPlotState.activeToggles.end(), [](const ToggleControl &t) { return !t.isOpen; }), uiPlotState.activeToggles.end());
            uiPlotState.activeTextInputs.erase(std::remove_if(uiPlotState.activeTextInputs.begin(), uiPlotState.activeTextInputs.end(), [](const TextInputControl &ti) { return !ti.isOpen; }), uiPlotState.activeTextInputs.end());

            uiPlotState.refreshActiveSignals();
        }

        // Render
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.1f, 0.1f, 0.1f, 1.0f }; // Background color
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // VSync on
    }

    // Cleanup
    luaScriptManager.stopAllLuaThreads();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    WSACleanup();
    timeEndPeriod(1);

    return 0;
}

// Helper functions (copied from ImGui DX11 example)
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high performance WARP software driver if hardware fails.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            g_ResizeWidth = (UINT)LOWORD(lParam);
            g_ResizeHeight = (UINT)HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}