#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "implot.h"
#include "ImGuiFileDialog.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm> // For std::find, std::sort
#include <atomic>
#include <chrono>
#include <cmath> // For FFT functions
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

// Network Includes
#include "SignalConfigLoader.h"
#include "DataSinks/UDPDataSink.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// DPI Awareness
#include <windows.h>
#include <ShellScalingApi.h>

#include "types.hpp"
#include "LuaScriptManager.hpp"
#include "plot_types.hpp"
#include "signal_processing.hpp"
#include "ui_state.hpp"

#define UDP_PORT 5000

// Note: plot_rendering.hpp is included later, after global state definitions

// -------------------------------------------------------------------------
// GLOBAL STATE
// -------------------------------------------------------------------------
std::mutex stateMutex;
std::atomic<bool> appRunning(true);

// Playback mode state
PlaybackMode currentPlaybackMode = PlaybackMode::ONLINE;

// Network connection state
std::atomic<bool> networkConnected(false);
std::atomic<bool> networkShouldConnect(false);
std::mutex networkConfigMutex;
std::string networkIP = "localhost";
int networkPort = UDP_PORT;

// Data logging state
std::ofstream logFile;
std::mutex logFileMutex;

// Parser selection state
std::vector<std::string> availableParsers;
std::string selectedParser = "legacy_binary";
std::mutex parserSelectionMutex;

// Offline playback state
struct OfflinePlaybackState {
  double minTime = 0.0;
  double maxTime = 0.0;
  double currentWindowStart = 0.0;
  double windowWidth = 10.0;  // Will be set to full duration on file load
  bool fileLoaded = false;
  std::string loadedFilePath;
} offlineState;

// The Registry: Maps a string ID to the actual data buffer
std::map<std::string, Signal> signalRegistry;

// Lua Script Manager
LuaScriptManager luaScriptManager;

// UI plot state (consolidates all plot window vectors and ID counters)
UIPlotState uiPlotState;

// Tier 3: Frame tracking for Lua callbacks
static uint64_t frameNumber = 0;
static auto lastFrameTime = std::chrono::high_resolution_clock::now();

// Include layout persistence functions (must come after OfflinePlaybackState definition)
#include "layout_persistence.hpp"

// Include plot rendering functions (must come after all global state definitions)
#include "plot_rendering.hpp"

// Include control element rendering functions (Tier 4)
#include "control_rendering.hpp"

// -------------------------------------------------------------------------
// NETWORK THREAD
// -------------------------------------------------------------------------

void NetworkReceiverThread() {
  // Load packet definitions
  std::vector<PacketDefinition> packets;
  if (!SignalConfigLoader::Load("signals.yaml", packets)) {
    printf(
        "Failed to load signals.yaml! Network thread will not process data.\n");
    return;
  }

  // Initialize signal registry with all signals from all packets
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    for (const auto &pkt : packets) {
      for (const auto &sig : pkt.signals) {
        signalRegistry[sig.key] = Signal(sig.key);
      }
    }
  }

  // Create UDPDataSink (initially with placeholder connection params)
  UDPDataSink* udpSink = nullptr;

  while (appRunning) {
    // Check if we should connect
    if (networkShouldConnect && !networkConnected) {
      // Get connection parameters
      std::string ip;
      int port;
      {
        std::lock_guard<std::mutex> lock(networkConfigMutex);
        ip = networkIP;
        port = networkPort;
      }

      // Create new UDP data sink with current connection params
      // Pass LuaScriptManager for Tier 2 packet parsing support
      udpSink = new UDPDataSink(signalRegistry, packets, ip, port, &logFile, &logFileMutex, &luaScriptManager);

      // Set Lua packet callback (Tier 1 feature - will be deprecated when fully migrated to Tier 2)
      udpSink->setPacketCallback([](const std::string& packetType) {
        luaScriptManager.executePacketCallbacks(packetType, signalRegistry);
      });

      if (udpSink->open()) {
        networkConnected = true;

        // Open log file
        {
          std::lock_guard<std::mutex> lock(logFileMutex);
          std::string filename = GenerateLogFilename();
          logFile.open(filename, std::ios::binary);
          if (logFile.is_open()) {
            printf("Logging to file: %s\n", filename.c_str());
          } else {
            printf("Failed to open log file: %s\n", filename.c_str());
          }
        }
      } else {
        // Failed to connect
        delete udpSink;
        udpSink = nullptr;
        networkShouldConnect = false;
      }
    }

    // Check if we should disconnect
    if (!networkShouldConnect && networkConnected) {
      if (udpSink) {
        udpSink->close();
        delete udpSink;
        udpSink = nullptr;
      }
      networkConnected = false;

      // Close log file
      {
        std::lock_guard<std::mutex> lock(logFileMutex);
        if (logFile.is_open()) {
          logFile.close();
          printf("Log file closed\n");
        }
      }
    }

    // Receive data if connected
    if (networkConnected && udpSink) {
      bool dataAvailable = true;

      // Process all available packets before sleeping
      // Lua callbacks are invoked automatically by UDPDataSink after each packet
      while (dataAvailable && networkConnected) {
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          dataAvailable = udpSink->step();
        }
        // Release the mutex between packets to allow UI thread to access data
      }

      // Sleep briefly after processing all available packets
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } else {
      // Not connected, sleep to avoid busy-waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  // Cleanup on exit
  if (udpSink) {
    udpSink->close();
    delete udpSink;
  }
}

//
void SetupImGuiStyle() {
  // 1. Theme and Colors
  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();

  // Rounding - makes it look less like Windows 95
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

  // Color Tweaks (optional: deepens the background, makes headers pop)
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
  plotStyle.LineWeight = 2.0f; // Make lines thicker and easier to see
  plotStyle.MarkerSize = 4.0f;
}

// -------------------------------------------------------------------------
// MAIN LOOP CONTEXT
// -------------------------------------------------------------------------
struct GlobalContext {
  SDL_Window *window;
  SDL_GLContext gl_context;
};

void MainLoopStep(void *arg) {
  GlobalContext *ctx = (GlobalContext *)arg;
  ImGuiIO &io = ImGui::GetIO();

  if (!appRunning) {
    return;
  }

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  // Lock data while we render to prevent iterator invalidation
  std::lock_guard<std::mutex> lock(stateMutex);

  // Tier 3: Execute frame callbacks
  frameNumber++;
  auto currentFrameTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = currentFrameTime - lastFrameTime;
  double deltaTime = elapsed.count();
  lastFrameTime = currentFrameTime;

  // Count total active plots
  int totalPlots = (int)(uiPlotState.activePlots.size() +
                        uiPlotState.activeReadoutBoxes.size() +
                        uiPlotState.activeXYPlots.size() +
                        uiPlotState.activeHistograms.size() +
                        uiPlotState.activeFFTs.size() +
                        uiPlotState.activeSpectrograms.size());

  luaScriptManager.executeFrameCallbacks(signalRegistry, frameNumber, deltaTime, totalPlots, &uiPlotState);

  // Get menu bar height
  float menuBarHeight = ImGui::GetFrameHeight();

  // ---------------------------------------------------------
  // UI: MENU BAR
  // ---------------------------------------------------------
  RenderMenuBar(uiPlotState);

  // ---------------------------------------------------------
  // UI: LEFT PANEL (Signal Browser)
  // ---------------------------------------------------------
  RenderSignalBrowser(uiPlotState, menuBarHeight);

  // ---------------------------------------------------------
  // UI: TIME-BASED PLOTS
  // ---------------------------------------------------------
  RenderTimePlots(uiPlotState, menuBarHeight);

  // ---------------------------------------------------------
  // UI: READOUT BOXES
  // ---------------------------------------------------------
  RenderReadoutBoxes(uiPlotState, menuBarHeight);

  // ---------------------------------------------------------
  // UI: X/Y PLOTS
  // ---------------------------------------------------------
  RenderXYPlots(uiPlotState, menuBarHeight);

  // ---------------------------------------------------------
  // UI: HISTOGRAMS
  // ---------------------------------------------------------
  RenderHistograms(uiPlotState, menuBarHeight);

  // ---------------------------------------------------------
  // UI: FFT PLOTS
  // ---------------------------------------------------------
  RenderFFTPlots(uiPlotState, menuBarHeight);

  // ---------------------------------------------------------
  // UI: SPECTROGRAMS
  // ---------------------------------------------------------
  RenderSpectrograms(uiPlotState, menuBarHeight);

  // ---------------------------------------------------------
  // UI: CONTROL ELEMENTS (Tier 4)
  // ---------------------------------------------------------
  RenderButtonControls(uiPlotState, menuBarHeight);
  RenderToggleControls(uiPlotState, menuBarHeight);
  RenderTextInputControls(uiPlotState, menuBarHeight);

  // ---------------------------------------------------------
  // UI: TIME SLIDER (Offline Mode Only)
  // ---------------------------------------------------------
  RenderTimeSlider();

  // ---------------------------------------------------------
  // UI: FILE DIALOGS
  // ---------------------------------------------------------
  RenderFileDialogs(uiPlotState);

  // ---------------------------------------------------------
  // CLEANUP PHASE (The "Erase-Remove Idiom")
  // ---------------------------------------------------------
  uiPlotState.activePlots.erase(
      std::remove_if(uiPlotState.activePlots.begin(), uiPlotState.activePlots.end(),
                     [](const PlotWindow &p) { return !p.isOpen; }),
      uiPlotState.activePlots.end());

  uiPlotState.activeReadoutBoxes.erase(
      std::remove_if(uiPlotState.activeReadoutBoxes.begin(), uiPlotState.activeReadoutBoxes.end(),
                     [](const ReadoutBox &r) { return !r.isOpen; }),
      uiPlotState.activeReadoutBoxes.end());

  uiPlotState.activeXYPlots.erase(
      std::remove_if(uiPlotState.activeXYPlots.begin(), uiPlotState.activeXYPlots.end(),
                     [](const XYPlotWindow &xy) { return !xy.isOpen; }),
      uiPlotState.activeXYPlots.end());

  // Render
  ImGui::Render();
  glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(ctx->window);
}

// -------------------------------------------------------------------------
// MAIN
// -------------------------------------------------------------------------
int main(int, char **) {
  // Enable DPI awareness for high-DPI displays (4K, etc.)
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

  // Setup SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
    printf("Error: SDL_Init failed: %s\n", SDL_GetError());
    return -1;
  }

  // Initialize Winsock
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    printf("WSAStartup failed\n");
    return -1;
  }

  const char *glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED);
  SDL_Window *window =
      SDL_CreateWindow("Telemetry Analyzer", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1600, 900, window_flags);
  if (window == NULL) {
    printf("Error: SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return -1;
  }

  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  if (gl_context == NULL) {
    printf("Error: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return -1;
  }

  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1);

  // Windows-specific hints for smoother vsync and timing
  SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
  SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "0");

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();

  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // --- NEW STYLING CODE START ---

  // 1. Load a bigger, smoother font
  if (!io.Fonts->AddFontFromFileTTF("Roboto-Medium.ttf", 20.0f)) {
    printf("Warning: Could not load font Roboto-Medium.ttf, using default font\n");
  }

  // 2. Apply our custom visual style
  SetupImGuiStyle();

  // --- NEW STYLING CODE END ---
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // No initial plots - user can create them via "Add New Plot Window" button

  GlobalContext ctx;
  ctx.window = window;
  ctx.gl_context = gl_context;

  // Load Lua scripts from scripts/ directory
  printf("Loading Lua scripts...\n");
  luaScriptManager.setAppRunningPtr(&appRunning);  // Tier 5: Allow Lua threads to check app status
  luaScriptManager.loadScriptsFromDirectory("scripts");

  // Scan available parsers for dropdown menu
  scanAvailableParsers(availableParsers);

  std::thread receiver(NetworkReceiverThread);
  SDL_Event event;
  while (appRunning) {
    // Use SDL_WaitEventTimeout to reduce CPU usage when idle
    // 16ms timeout targets ~60 FPS, allowing the OS to idle the thread
    if (SDL_WaitEventTimeout(&event, 16)) {
      // Process the event that woke us up
      do {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT)
          appRunning = false;
      } while (SDL_PollEvent(&event)); // Drain any remaining events
    }

    MainLoopStep(&ctx);
  }
  if (receiver.joinable())
    receiver.join();

  // Tier 5: Stop all Lua I/O threads before cleanup
  printf("Stopping Lua threads...\n");
  luaScriptManager.stopAllLuaThreads();

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  WSACleanup();

  return 0;
}