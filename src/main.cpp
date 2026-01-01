#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "implot.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm> // For std::find
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <stdio.h>
#include <string>
#include <thread>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Network Includes
#include "SignalConfigLoader.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#endif

// telemetry_defs.h removed as we now use dynamic loading.

#define UDP_PORT 5000

// -------------------------------------------------------------------------
// DATA STRUCTURES
// -------------------------------------------------------------------------

// A single signal (e.g., "IMU.AccelX") holding its own history
struct Signal {
  std::string name;
  int offset;
  std::vector<double> dataX; // Time
  std::vector<double> dataY; // Value
  int maxSize;

  Signal(std::string n = "", int size = 2000)
      : name(n), maxSize(size), offset(0) {
    dataX.reserve(maxSize);
    dataY.reserve(maxSize);
  }

  void AddPoint(double x, double y) {
    if (dataX.size() < maxSize) {
      dataX.push_back(x);
      dataY.push_back(y);
    } else {
      dataX[offset] = x;
      dataY[offset] = y;
      offset = (offset + 1) % maxSize;
    }
  }
};

// Represents one Plot Panel on the right side
struct PlotWindow {
  int id;
  std::string title;
  std::vector<std::string> signalNames; // List of keys to look up in Registry
  bool paused = false;
  bool isOpen = true;
};

// -------------------------------------------------------------------------
// GLOBAL STATE
// -------------------------------------------------------------------------
std::mutex stateMutex;
std::atomic<bool> appRunning(true);

// Network connection state
std::atomic<bool> networkConnected(false);
std::atomic<bool> networkShouldConnect(false);
std::mutex networkConfigMutex;
std::string networkIP = "localhost";
int networkPort = UDP_PORT;

// The Registry: Maps a string ID to the actual data buffer
std::map<std::string, Signal> signalRegistry;

// The Layout: List of active plot windows
std::vector<PlotWindow> activePlots;
int nextPlotId = 1; // Auto-increment ID for window titles

// -------------------------------------------------------------------------
// NETWORK THREAD
// -------------------------------------------------------------------------
// Helper to read a value from the buffer based on type
// Supports all stdint.h types plus legacy C types for backwards compatibility
// All values are cast to double for storage in the signal buffers
double ReadValue(const char *buffer, const PacketDefinition::Field &field) {
  // Floating point types
  if (field.type == "double") {
    double val;
    memcpy(&val, buffer + field.offset, sizeof(double));
    return val;
  } else if (field.type == "float") {
    float val;
    memcpy(&val, buffer + field.offset, sizeof(float));
    return (double)val;
  }
  // Signed integer types
  else if (field.type == "int8_t" || field.type == "int8") {
    int8_t val;
    memcpy(&val, buffer + field.offset, sizeof(int8_t));
    return (double)val;
  } else if (field.type == "int16_t" || field.type == "int16") {
    int16_t val;
    memcpy(&val, buffer + field.offset, sizeof(int16_t));
    return (double)val;
  } else if (field.type == "int32_t" || field.type == "int32" || field.type == "int") {
    int32_t val;
    memcpy(&val, buffer + field.offset, sizeof(int32_t));
    return (double)val;
  } else if (field.type == "int64_t" || field.type == "int64") {
    int64_t val;
    memcpy(&val, buffer + field.offset, sizeof(int64_t));
    return (double)val;
  }
  // Unsigned integer types
  else if (field.type == "uint8_t" || field.type == "uint8") {
    uint8_t val;
    memcpy(&val, buffer + field.offset, sizeof(uint8_t));
    return (double)val;
  } else if (field.type == "uint16_t" || field.type == "uint16") {
    uint16_t val;
    memcpy(&val, buffer + field.offset, sizeof(uint16_t));
    return (double)val;
  } else if (field.type == "uint32_t" || field.type == "uint32") {
    uint32_t val;
    memcpy(&val, buffer + field.offset, sizeof(uint32_t));
    return (double)val;
  } else if (field.type == "uint64_t" || field.type == "uint64") {
    uint64_t val;
    memcpy(&val, buffer + field.offset, sizeof(uint64_t));
    return (double)val;
  }
  // Standard C types (for backwards compatibility)
  else if (field.type == "char") {
    char val;
    memcpy(&val, buffer + field.offset, sizeof(char));
    return (double)val;
  } else if (field.type == "short") {
    short val;
    memcpy(&val, buffer + field.offset, sizeof(short));
    return (double)val;
  } else if (field.type == "long") {
    long val;
    memcpy(&val, buffer + field.offset, sizeof(long));
    return (double)val;
  } else if (field.type == "unsigned char") {
    unsigned char val;
    memcpy(&val, buffer + field.offset, sizeof(unsigned char));
    return (double)val;
  } else if (field.type == "unsigned short") {
    unsigned short val;
    memcpy(&val, buffer + field.offset, sizeof(unsigned short));
    return (double)val;
  } else if (field.type == "unsigned int") {
    unsigned int val;
    memcpy(&val, buffer + field.offset, sizeof(unsigned int));
    return (double)val;
  } else if (field.type == "unsigned long") {
    unsigned long val;
    memcpy(&val, buffer + field.offset, sizeof(unsigned long));
    return (double)val;
  }

  // Unknown type - return 0 and warn
  fprintf(stderr, "Warning: Unknown field type '%s' for field '%s'\n",
          field.type.c_str(), field.name.c_str());
  return 0.0;
}

void NetworkReceiverThread() {
  // Load Signals
  std::vector<PacketDefinition> packets;
  std::vector<SignalDefinition> signals;
  if (!SignalConfigLoader::Load("signals.yaml", packets, signals)) {
    printf(
        "Failed to load signals.yaml! Network thread will not process data.\n");
    return;
  }

  // Initialize Registry
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    for (const auto &sig : signals) {
      signalRegistry[sig.key] = Signal(sig.key);
    }
  }

#ifdef _WIN32
  SOCKET sockfd = INVALID_SOCKET;
#else
  int sockfd = -1;
#endif

  char buffer[1024];

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

      // Create socket
#ifdef _WIN32
      sockfd = socket(AF_INET, SOCK_DGRAM, 0);
      if (sockfd == INVALID_SOCKET) {
        printf("Failed to create socket\n");
        networkShouldConnect = false;
        continue;
      }

      // Set socket to non-blocking mode
      u_long mode = 1;
      ioctlsocket(sockfd, FIONBIO, &mode);
#else
      sockfd = socket(AF_INET, SOCK_DGRAM, 0);
      if (sockfd < 0) {
        printf("Failed to create socket\n");
        networkShouldConnect = false;
        continue;
      }

      // Set socket to non-blocking mode
      int flags = fcntl(sockfd, F_GETFL, 0);
      fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
#endif

      // Setup address
      sockaddr_in servaddr;
      memset(&servaddr, 0, sizeof(servaddr));
      servaddr.sin_family = AF_INET;
      servaddr.sin_port = htons(port);

      // Convert IP address
      if (ip == "localhost" || ip == "127.0.0.1") {
        servaddr.sin_addr.s_addr = INADDR_ANY;
      } else {
        inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr);
      }

      // Bind socket
      if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        printf("Failed to bind socket to %s:%d\n", ip.c_str(), port);
#ifdef _WIN32
        closesocket(sockfd);
        sockfd = INVALID_SOCKET;
#else
        close(sockfd);
        sockfd = -1;
#endif
        networkShouldConnect = false;
        continue;
      }

      networkConnected = true;
      printf("Connected to %s:%d\n", ip.c_str(), port);
    }

    // Check if we should disconnect
    if (!networkShouldConnect && networkConnected) {
#ifdef _WIN32
      if (sockfd != INVALID_SOCKET) {
        closesocket(sockfd);
        sockfd = INVALID_SOCKET;
      }
#else
      if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
      }
#endif
      networkConnected = false;
      printf("Disconnected\n");
    }

    // Receive data if connected
    if (networkConnected) {
#ifdef _WIN32
      int len = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
      if (len == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
          // Real error occurred
          printf("Socket error: %d\n", error);
        }
        // Sleep briefly to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      }
#else
      ssize_t len = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
      if (len < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
          // Real error occurred
          printf("Socket error: %d\n", errno);
        }
        // Sleep briefly to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      }
#endif

      if (len > 0) {
        std::lock_guard<std::mutex> lock(stateMutex);

        // Dynamic Packet Parsing
        for (const auto &pkt : packets) {
          // Basic check: length and header string
          if (len == (int)pkt.sizeCheck) {
            if (strncmp(buffer, pkt.headerString.c_str(),
                        pkt.headerString.length()) == 0) {

              // Optimize: Map packet ID to relevant signals beforehand?
              // For now, iterating signals is fine for small count.
              for (const auto &sig : signals) {
                if (sig.packetId == pkt.id) {
                  // Find fields
                  const PacketDefinition::Field *timeField = nullptr;
                  const PacketDefinition::Field *valField = nullptr;

                  for (const auto &f : pkt.fields) {
                    if (f.name == sig.timeField)
                      timeField = &f;
                    if (f.name == sig.valueField)
                      valField = &f;
                  }

                  if (timeField && valField) {
                    double t = ReadValue(buffer, *timeField);
                    double v = ReadValue(buffer, *valField);
                    signalRegistry[sig.key].AddPoint(t, v);
                  }
                }
              }

              // Break after finding the matching packet type for this buffer
              break;
            }
          }
        }
      }
    } else {
      // Not connected, sleep to avoid busy-waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  // Cleanup on exit
#ifdef _WIN32
  if (sockfd != INVALID_SOCKET) {
    closesocket(sockfd);
  }
#else
  if (sockfd >= 0) {
    close(sockfd);
  }
#endif
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
#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();
#endif
    return;
  }

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  // Lock data while we render to prevent iterator invalidation
  std::lock_guard<std::mutex> lock(stateMutex);

  // ---------------------------------------------------------
  // UI: MENU BAR
  // ---------------------------------------------------------
  static char ipBuffer[64] = "localhost";
  static char portBuffer[16] = "5000";
  static bool buffersInitialized = false;

  // Initialize buffers once with current values
  if (!buffersInitialized) {
    std::lock_guard<std::mutex> lock(networkConfigMutex);
    strncpy(ipBuffer, networkIP.c_str(), sizeof(ipBuffer) - 1);
    snprintf(portBuffer, sizeof(portBuffer), "%d", networkPort);
    buffersInitialized = true;
  }

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Close")) {
        appRunning = false;
      }
      ImGui::EndMenu();
    }

    // Add spacing
    ImGui::Separator();

    // IP input
    ImGui::Text("IP:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::InputText("##IP", ipBuffer, sizeof(ipBuffer));

    // Port input
    ImGui::SameLine();
    ImGui::Text("Port:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::InputText("##Port", portBuffer, sizeof(portBuffer), ImGuiInputTextFlags_CharsDecimal);

    // Connect/Disconnect button
    ImGui::SameLine();
    bool isConnected = networkConnected.load();
    const char* buttonText = isConnected ? "Disconnect" : "Connect";
    if (ImGui::Button(buttonText)) {
      if (isConnected) {
        // Disconnect
        networkShouldConnect = false;
      } else {
        // Update connection parameters and connect
        {
          std::lock_guard<std::mutex> lock(networkConfigMutex);
          networkIP = std::string(ipBuffer);
          networkPort = atoi(portBuffer);
        }
        networkShouldConnect = true;
      }
    }

    ImGui::EndMainMenuBar();
  }

  // ---------------------------------------------------------
  // UI: LEFT PANEL (Signal List)
  // ---------------------------------------------------------
  float menuBarHeight = ImGui::GetFrameHeight();
  ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(300, io.DisplaySize.y - menuBarHeight), ImGuiCond_FirstUseEver);

  ImGui::Begin("Signal Browser");
  ImGui::Text("Available Signals");
  ImGui::Separator();

  // Button to create new plot
  if (ImGui::Button("Add New Plot Window", ImVec2(-1, 0))) {
    PlotWindow newPlot;
    newPlot.id = nextPlotId++;
    newPlot.title = "Plot " + std::to_string(newPlot.id);
    activePlots.push_back(newPlot);
  }
  ImGui::Separator();

  // Iterate over registry to show draggable items
  ImGui::BeginChild("SignalList");
  for (auto &[key, signal] : signalRegistry) {

    // Display the item
    ImGui::Selectable(key.c_str());

    // 1. START DRAG SOURCE
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
      // Set payload to carry the string key (e.g., "IMU.AccelX")
      // +1 includes the null terminator
      ImGui::SetDragDropPayload("SIGNAL_NAME", key.c_str(), key.length() + 1);

      // Tooltip while dragging
      ImGui::Text("Add %s to plot", key.c_str());
      ImGui::EndDragDropSource();
    }
  }
  ImGui::EndChild();
  ImGui::End();

  // ---------------------------------------------------------
  // UI: RIGHT AREA (Dynamic Plots)
  // ---------------------------------------------------------

  // Loop through all active plot windows
  for (auto &plot : activePlots) {
    if (!plot.isOpen)
      continue;

    // Set initial position below menu bar for new plot windows
    ImGui::SetNextWindowPos(ImVec2(350, menuBarHeight + 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    ImGui::Begin(plot.title.c_str(), &plot.isOpen);

    // Plot Header Controls
    if (ImGui::Button(plot.paused ? "Resume" : "Pause")) {
      plot.paused = !plot.paused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Signals")) {
      plot.signalNames.clear();
    }

    if (ImPlot::BeginPlot("##LinePlot", ImVec2(-1, -1))) {
      ImPlot::SetupAxes("Time (s)", "Value");

      // Axis Logic
      if (!plot.paused && !plot.signalNames.empty()) {
        double maxTime = 0;
        for (const auto &sigName : plot.signalNames) {
          if (signalRegistry.count(sigName)) {
            Signal &sig = signalRegistry[sigName];
            if (!sig.dataX.empty()) {
              int idx =
                  (sig.offset == 0) ? sig.dataX.size() - 1 : sig.offset - 1;
              if (sig.dataX[idx] > maxTime)
                maxTime = sig.dataX[idx];
            }
          }
        }
        if (maxTime > 0)
          ImPlot::SetupAxisLimits(ImAxis_X1, maxTime - 5.0, maxTime,
                                  ImGuiCond_Always);
      }

      // Drag & Drop Target
      if (ImPlot::BeginDragDropTargetPlot()) {
        if (const ImGuiPayload *payload =
                ImGui::AcceptDragDropPayload("SIGNAL_NAME")) {
          std::string droppedName = (const char *)payload->Data;
          if (std::find(plot.signalNames.begin(), plot.signalNames.end(),
                        droppedName) == plot.signalNames.end()) {
            plot.signalNames.push_back(droppedName);
          }
        }
        ImPlot::EndDragDropTarget();
      }

      // Render Lines
      for (const auto &sigName : plot.signalNames) {
        if (signalRegistry.count(sigName)) {
          Signal &sig = signalRegistry[sigName];
          if (!sig.dataX.empty()) {
            ImPlot::PlotLine(sig.name.c_str(), sig.dataX.data(),
                             sig.dataY.data(), sig.dataX.size(), 0, sig.offset);
          }
        }
      }
      ImPlot::EndPlot();
    }
    ImGui::End();
  }

  // 2. CLEANUP PHASE (The "Erase-Remove Idiom")
  activePlots.erase(
      std::remove_if(activePlots.begin(), activePlots.end(),
                     [](const PlotWindow &p) { return !p.isOpen; }),
      activePlots.end());

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
  // Setup SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
    printf("Error: SDL_Init failed: %s\n", SDL_GetError());
    return -1;
  }

#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    printf("WSAStartup failed\n");
    return -1;
  }
#endif

#ifdef __EMSCRIPTEN__
  const char *glsl_version = "#version 100";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
  const char *glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                        SDL_WINDOW_ALLOW_HIGHDPI);
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

#ifdef _WIN32
  // Windows-specific hints for smoother vsync and timing
  SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
  SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "0");
#endif

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();

  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // --- NEW STYLING CODE START ---

  // 1. Load a bigger, smoother font
#ifdef __EMSCRIPTEN__
  if (!io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 20.0f)) {
    printf("Warning: Could not load font fonts/Roboto-Medium.ttf, using default font\n");
  }
#else
  if (!io.Fonts->AddFontFromFileTTF("Roboto-Medium.ttf", 20.0f)) {
    printf("Warning: Could not load font Roboto-Medium.ttf, using default font\n");
  }
#endif

  // 2. Apply our custom visual style
  SetupImGuiStyle();

  // --- NEW STYLING CODE END ---
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // No initial plots - user can create them via "Add New Plot Window" button

  GlobalContext ctx;
  ctx.window = window;
  ctx.gl_context = gl_context;

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(MainLoopStep, &ctx, 0, 1);
#else
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
#endif

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

#ifdef _WIN32
  WSACleanup();
#endif

  return 0;
}