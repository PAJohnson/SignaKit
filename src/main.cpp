#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "implot.h"
#include "ImGuiFileDialog.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm> // For std::find
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <stdio.h>
#include <string>
#include <thread>
#include <vector>

// Network Includes
#include "SignalConfigLoader.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// DPI Awareness
#include <windows.h>
#include <ShellScalingApi.h>

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

// Represents one Readout Box
struct ReadoutBox {
  int id;
  std::string title;
  std::string signalName; // Single signal to display (empty if none assigned)
  bool isOpen = true;
};

// Represents one X/Y Plot
struct XYPlotWindow {
  int id;
  std::string title;
  std::string xSignalName; // Signal for X axis
  std::string ySignalName; // Signal for Y axis
  bool paused = false;
  bool isOpen = true;

  // History of X/Y points with fade effect
  std::vector<double> historyX;
  std::vector<double> historyY;
  int maxHistorySize = 500; // Number of points to keep
  int historyOffset = 0;
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

// Data logging state
std::ofstream logFile;
std::mutex logFileMutex;

// The Registry: Maps a string ID to the actual data buffer
std::map<std::string, Signal> signalRegistry;

// The Layout: List of active plot windows
std::vector<PlotWindow> activePlots;
int nextPlotId = 1; // Auto-increment ID for window titles

// Readout boxes
std::vector<ReadoutBox> activeReadoutBoxes;
int nextReadoutBoxId = 1; // Auto-increment ID for readout box titles

// X/Y plots
std::vector<XYPlotWindow> activeXYPlots;
int nextXYPlotId = 1; // Auto-increment ID for X/Y plot titles

// -------------------------------------------------------------------------
// NETWORK THREAD
// -------------------------------------------------------------------------
// Helper to generate timestamped filename
std::string GenerateLogFilename() {
  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  std::tm tm_utc;
  gmtime_s(&tm_utc, &time_t_now);

  std::ostringstream oss;
  oss << std::setfill('0')
      << std::setw(2) << (tm_utc.tm_mon + 1)
      << std::setw(2) << tm_utc.tm_mday
      << std::setw(4) << (tm_utc.tm_year + 1900)
      << "_"
      << std::setw(2) << tm_utc.tm_hour
      << std::setw(2) << tm_utc.tm_min
      << std::setw(2) << tm_utc.tm_sec
      << ".bin";
  return oss.str();
}

// Helper to read a value from the buffer based on type
// Supports all stdint.h types plus legacy C types for backwards compatibility
// All values are cast to double for storage in the signal buffers
double ReadValue(const char *buffer, const std::string &type, int offset) {
  // Floating point types
  if (type == "double") {
    double val;
    memcpy(&val, buffer + offset, sizeof(double));
    return val;
  } else if (type == "float") {
    float val;
    memcpy(&val, buffer + offset, sizeof(float));
    return (double)val;
  }
  // Signed integer types
  else if (type == "int8_t" || type == "int8") {
    int8_t val;
    memcpy(&val, buffer + offset, sizeof(int8_t));
    return (double)val;
  } else if (type == "int16_t" || type == "int16") {
    int16_t val;
    memcpy(&val, buffer + offset, sizeof(int16_t));
    return (double)val;
  } else if (type == "int32_t" || type == "int32" || type == "int") {
    int32_t val;
    memcpy(&val, buffer + offset, sizeof(int32_t));
    return (double)val;
  } else if (type == "int64_t" || type == "int64") {
    int64_t val;
    memcpy(&val, buffer + offset, sizeof(int64_t));
    return (double)val;
  }
  // Unsigned integer types
  else if (type == "uint8_t" || type == "uint8") {
    uint8_t val;
    memcpy(&val, buffer + offset, sizeof(uint8_t));
    return (double)val;
  } else if (type == "uint16_t" || type == "uint16") {
    uint16_t val;
    memcpy(&val, buffer + offset, sizeof(uint16_t));
    return (double)val;
  } else if (type == "uint32_t" || type == "uint32") {
    uint32_t val;
    memcpy(&val, buffer + offset, sizeof(uint32_t));
    return (double)val;
  } else if (type == "uint64_t" || type == "uint64") {
    uint64_t val;
    memcpy(&val, buffer + offset, sizeof(uint64_t));
    return (double)val;
  }
  // Standard C types (for backwards compatibility)
  else if (type == "char") {
    char val;
    memcpy(&val, buffer + offset, sizeof(char));
    return (double)val;
  } else if (type == "short") {
    short val;
    memcpy(&val, buffer + offset, sizeof(short));
    return (double)val;
  } else if (type == "long") {
    long val;
    memcpy(&val, buffer + offset, sizeof(long));
    return (double)val;
  } else if (type == "unsigned char") {
    unsigned char val;
    memcpy(&val, buffer + offset, sizeof(unsigned char));
    return (double)val;
  } else if (type == "unsigned short") {
    unsigned short val;
    memcpy(&val, buffer + offset, sizeof(unsigned short));
    return (double)val;
  } else if (type == "unsigned int") {
    unsigned int val;
    memcpy(&val, buffer + offset, sizeof(unsigned int));
    return (double)val;
  } else if (type == "unsigned long") {
    unsigned long val;
    memcpy(&val, buffer + offset, sizeof(unsigned long));
    return (double)val;
  }

  // Unknown type - return 0 and warn
  fprintf(stderr, "Warning: Unknown field type '%s'\n", type.c_str());
  return 0.0;
}

// -------------------------------------------------------------------------
// LAYOUT SAVE/LOAD
// -------------------------------------------------------------------------
bool SaveLayout(const std::string &filename, const std::vector<PlotWindow> &plots, const std::vector<ReadoutBox> &readouts, const std::vector<XYPlotWindow> &xyPlots) {
  try {
    YAML::Emitter out;
    out << YAML::BeginMap;

    // Save plots
    out << YAML::Key << "plots" << YAML::Value << YAML::BeginSeq;
    for (const auto &plot : plots) {
      out << YAML::BeginMap;
      out << YAML::Key << "id" << YAML::Value << plot.id;
      out << YAML::Key << "title" << YAML::Value << plot.title;
      out << YAML::Key << "paused" << YAML::Value << plot.paused;
      out << YAML::Key << "signals" << YAML::Value << YAML::BeginSeq;
      for (const auto &signal : plot.signalNames) {
        out << signal;
      }
      out << YAML::EndSeq;
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    // Save readout boxes
    out << YAML::Key << "readouts" << YAML::Value << YAML::BeginSeq;
    for (const auto &readout : readouts) {
      out << YAML::BeginMap;
      out << YAML::Key << "id" << YAML::Value << readout.id;
      out << YAML::Key << "title" << YAML::Value << readout.title;
      out << YAML::Key << "signal" << YAML::Value << readout.signalName;
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    // Save X/Y plots
    out << YAML::Key << "xyplots" << YAML::Value << YAML::BeginSeq;
    for (const auto &xyPlot : xyPlots) {
      out << YAML::BeginMap;
      out << YAML::Key << "id" << YAML::Value << xyPlot.id;
      out << YAML::Key << "title" << YAML::Value << xyPlot.title;
      out << YAML::Key << "paused" << YAML::Value << xyPlot.paused;
      out << YAML::Key << "xSignal" << YAML::Value << xyPlot.xSignalName;
      out << YAML::Key << "ySignal" << YAML::Value << xyPlot.ySignalName;
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::EndMap;

    std::ofstream fout(filename);
    if (!fout.is_open()) {
      fprintf(stderr, "Failed to open file for writing: %s\n", filename.c_str());
      return false;
    }

    fout << out.c_str();
    fout.close();
    printf("Layout saved to: %s\n", filename.c_str());
    return true;
  } catch (const std::exception &e) {
    fprintf(stderr, "Error saving layout: %s\n", e.what());
    return false;
  }
}

bool LoadLayout(const std::string &filename, std::vector<PlotWindow> &plots, int &nextPlotId,
                std::vector<ReadoutBox> &readouts, int &nextReadoutId,
                std::vector<XYPlotWindow> &xyPlots, int &nextXYPlotId) {
  try {
    YAML::Node config = YAML::LoadFile(filename);
    if (!config["plots"]) {
      fprintf(stderr, "Invalid layout file: missing 'plots' key\n");
      return false;
    }

    std::vector<PlotWindow> loadedPlots;
    std::vector<ReadoutBox> loadedReadouts;
    std::vector<XYPlotWindow> loadedXYPlots;
    int maxPlotId = 0;
    int maxReadoutId = 0;
    int maxXYPlotId = 0;

    // Load plots
    for (const auto &plotNode : config["plots"]) {
      PlotWindow plot;
      plot.id = plotNode["id"].as<int>();
      plot.title = plotNode["title"].as<std::string>();
      plot.paused = plotNode["paused"] ? plotNode["paused"].as<bool>() : false;
      plot.isOpen = true;

      if (plotNode["signals"]) {
        for (const auto &signalNode : plotNode["signals"]) {
          plot.signalNames.push_back(signalNode.as<std::string>());
        }
      }

      loadedPlots.push_back(plot);
      if (plot.id > maxPlotId) {
        maxPlotId = plot.id;
      }
    }

    // Load readout boxes (if present)
    if (config["readouts"]) {
      for (const auto &readoutNode : config["readouts"]) {
        ReadoutBox readout;
        readout.id = readoutNode["id"].as<int>();
        readout.title = readoutNode["title"].as<std::string>();
        readout.signalName = readoutNode["signal"] ? readoutNode["signal"].as<std::string>() : "";
        readout.isOpen = true;

        loadedReadouts.push_back(readout);
        if (readout.id > maxReadoutId) {
          maxReadoutId = readout.id;
        }
      }
    }

    // Load X/Y plots (if present)
    if (config["xyplots"]) {
      for (const auto &xyPlotNode : config["xyplots"]) {
        XYPlotWindow xyPlot;
        xyPlot.id = xyPlotNode["id"].as<int>();
        xyPlot.title = xyPlotNode["title"].as<std::string>();
        xyPlot.paused = xyPlotNode["paused"] ? xyPlotNode["paused"].as<bool>() : false;
        xyPlot.xSignalName = xyPlotNode["xSignal"] ? xyPlotNode["xSignal"].as<std::string>() : "";
        xyPlot.ySignalName = xyPlotNode["ySignal"] ? xyPlotNode["ySignal"].as<std::string>() : "";
        xyPlot.isOpen = true;

        loadedXYPlots.push_back(xyPlot);
        if (xyPlot.id > maxXYPlotId) {
          maxXYPlotId = xyPlot.id;
        }
      }
    }

    plots = loadedPlots;
    readouts = loadedReadouts;
    xyPlots = loadedXYPlots;
    nextPlotId = maxPlotId + 1;
    nextReadoutId = maxReadoutId + 1;
    nextXYPlotId = maxXYPlotId + 1;
    printf("Layout loaded from: %s\n", filename.c_str());
    return true;
  } catch (const std::exception &e) {
    fprintf(stderr, "Error loading layout: %s\n", e.what());
    return false;
  }
}

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

  SOCKET sockfd = INVALID_SOCKET;
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
      sockfd = socket(AF_INET, SOCK_DGRAM, 0);
      if (sockfd == INVALID_SOCKET) {
        printf("Failed to create socket\n");
        networkShouldConnect = false;
        continue;
      }

      // Set socket to non-blocking mode
      u_long mode = 1;
      ioctlsocket(sockfd, FIONBIO, &mode);

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
        closesocket(sockfd);
        sockfd = INVALID_SOCKET;
        networkShouldConnect = false;
        continue;
      }

      networkConnected = true;
      printf("Connected to %s:%d\n", ip.c_str(), port);

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
    }

    // Check if we should disconnect
    if (!networkShouldConnect && networkConnected) {
      if (sockfd != INVALID_SOCKET) {
        closesocket(sockfd);
        sockfd = INVALID_SOCKET;
      }
      networkConnected = false;
      printf("Disconnected\n");

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
    if (networkConnected) {
      bool dataAvailable = true;

      // Process all available packets before sleeping
      while (dataAvailable && networkConnected) {
        int len = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
        if (len == SOCKET_ERROR) {
          int error = WSAGetLastError();
          if (error == WSAEWOULDBLOCK) {
            // No more data available
            dataAvailable = false;
          } else {
            // Real error occurred
            printf("Socket error: %d\n", error);
            dataAvailable = false;
          }
          break;
        }

        if (len > 0) {
          // Log raw packet to file
          {
            std::lock_guard<std::mutex> lock(logFileMutex);
            if (logFile.is_open()) {
              logFile.write(buffer, len);
            }
          }

          std::lock_guard<std::mutex> lock(stateMutex);

          // Find matching packet by header string
          for (const PacketDefinition& pkt : packets) {
            if (strncmp(buffer, pkt.headerString.c_str(),
                        pkt.headerString.length()) == 0) {

              // Matched packet! Process all signals in this packet
              for (const auto &sig : pkt.signals) {
                double t = ReadValue(buffer, sig.timeType, sig.timeOffset);
                double v = ReadValue(buffer, sig.type, sig.offset);
                signalRegistry[sig.key].AddPoint(t, v);
              }

              // Break after finding the matching packet type
              break;
            }
          }
        } else {
          // len == 0, connection closed
          dataAvailable = false;
        }
      }

      // Sleep briefly after processing all available packets
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } else {
      // Not connected, sleep to avoid busy-waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  // Cleanup on exit
  if (sockfd != INVALID_SOCKET) {
    closesocket(sockfd);
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
      if (ImGui::MenuItem("Save Layout")) {
        IGFD::FileDialogConfig config;
        config.path = ".";
        ImGuiFileDialog::Instance()->OpenDialog("SaveLayoutDlg", "Save Layout", ".yaml", config);
      }
      if (ImGui::MenuItem("Open Layout")) {
        IGFD::FileDialogConfig config;
        config.path = ".";
        ImGuiFileDialog::Instance()->OpenDialog("OpenLayoutDlg", "Open Layout", ".yaml", config);
      }
      ImGui::Separator();
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

  // Menu to create new plot or readout box
  if (ImGui::Button("Add New...", ImVec2(-1, 0))) {
    ImGui::OpenPopup("AddNewPopup");
  }

  if (ImGui::BeginPopup("AddNewPopup")) {
    if (ImGui::MenuItem("Time-based Plot")) {
      PlotWindow newPlot;
      newPlot.id = nextPlotId++;
      newPlot.title = "Plot " + std::to_string(newPlot.id);
      activePlots.push_back(newPlot);
    }
    if (ImGui::MenuItem("X/Y Plot")) {
      XYPlotWindow newXYPlot;
      newXYPlot.id = nextXYPlotId++;
      newXYPlot.title = "X/Y Plot " + std::to_string(newXYPlot.id);
      newXYPlot.xSignalName = "";
      newXYPlot.ySignalName = "";
      activeXYPlots.push_back(newXYPlot);
    }
    if (ImGui::MenuItem("Readout Box")) {
      ReadoutBox newReadout;
      newReadout.id = nextReadoutBoxId++;
      newReadout.title = "Readout " + std::to_string(newReadout.id);
      newReadout.signalName = ""; // Empty initially
      activeReadoutBoxes.push_back(newReadout);
    }
    ImGui::EndPopup();
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
      ImPlot::SetupAxes("Time (s)", "Value", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

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
          } else {
            // Plot empty data to show signal in legend
            double empty[1] = {0};
            ImPlot::PlotLine(sig.name.c_str(), empty, empty, 0);
          }
        }
      }
      ImPlot::EndPlot();
    }
    ImGui::End();
  }

  // ---------------------------------------------------------
  // UI: READOUT BOXES
  // ---------------------------------------------------------

  // Loop through all active readout boxes
  for (auto &readout : activeReadoutBoxes) {
    if (!readout.isOpen)
      continue;

    // Set initial position below menu bar for new readout boxes
    ImGui::SetNextWindowPos(ImVec2(350, menuBarHeight + 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);

    // Use a stable ID (based on readout.id) while displaying dynamic title
    // Format: "DisplayTitle##UniqueID"
    std::string windowID = readout.title + "##Readout" + std::to_string(readout.id);
    ImGui::Begin(windowID.c_str(), &readout.isOpen);

    // Create a child region to fill the entire window for drag-and-drop
    ImGui::BeginChild("ReadoutContent", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);

    // Display content based on whether a signal is assigned
    if (readout.signalName.empty()) {
      // Center the text vertically and horizontally
      ImVec2 windowSize = ImGui::GetWindowSize();
      const char* text = "Drag and drop a signal here";
      ImVec2 textSize = ImGui::CalcTextSize(text);
      ImGui::SetCursorPosX((windowSize.x - textSize.x) * 0.5f);
      ImGui::SetCursorPosY((windowSize.y - textSize.y) * 0.5f);
      ImGui::TextWrapped("%s", text);
    } else {
      // Display the current value
      if (signalRegistry.count(readout.signalName)) {
        Signal &sig = signalRegistry[readout.signalName];
        if (!sig.dataY.empty()) {
          // Get the most recent value
          int idx = (sig.offset == 0) ? sig.dataY.size() - 1 : sig.offset - 1;
          double currentValue = sig.dataY[idx];

          // Display with larger text, centered
          ImVec2 windowSize = ImGui::GetWindowSize();
          char valueStr[64];
          snprintf(valueStr, sizeof(valueStr), "%.6f", currentValue);

          ImGui::PushFont(io.Fonts->Fonts[0]);
          ImGui::SetWindowFontScale(2.0f);
          ImVec2 textSize = ImGui::CalcTextSize(valueStr);
          ImGui::SetCursorPosX((windowSize.x - textSize.x) * 0.5f);
          ImGui::SetCursorPosY((windowSize.y - textSize.y) * 0.5f - 20);
          ImGui::Text("%s", valueStr);
          ImGui::SetWindowFontScale(1.0f);
          ImGui::PopFont();
        } else {
          ImGui::TextDisabled("No data");
        }
      } else {
        ImGui::TextDisabled("Signal not found");
      }

      // Button to clear the signal at the bottom
      ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 30);
      if (ImGui::Button("Clear Signal", ImVec2(-1, 0))) {
        readout.signalName = "";
        readout.title = "Readout " + std::to_string(readout.id);
      }
    }

    // Accept drag-and-drop anywhere in the child region (entire window)
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SIGNAL_NAME")) {
        std::string droppedName = (const char *)payload->Data;
        readout.signalName = droppedName;
        readout.title = droppedName;
      }
      ImGui::EndDragDropTarget();
    }

    ImGui::EndChild();
    ImGui::End();
  }

  // ---------------------------------------------------------
  // UI: X/Y PLOTS
  // ---------------------------------------------------------

  // Loop through all active X/Y plots
  for (auto &xyPlot : activeXYPlots) {
    if (!xyPlot.isOpen)
      continue;

    // Set initial position below menu bar for new X/Y plots
    ImGui::SetNextWindowPos(ImVec2(350, menuBarHeight + 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    ImGui::Begin(xyPlot.title.c_str(), &xyPlot.isOpen);

    // Plot Header Controls
    if (ImGui::Button(xyPlot.paused ? "Resume" : "Pause")) {
      xyPlot.paused = !xyPlot.paused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Signals")) {
      xyPlot.xSignalName = "";
      xyPlot.ySignalName = "";
      xyPlot.historyX.clear();
      xyPlot.historyY.clear();
      xyPlot.historyOffset = 0;
    }

    // Update history if both signals are assigned and not paused
    if (!xyPlot.paused && !xyPlot.xSignalName.empty() && !xyPlot.ySignalName.empty()) {
      if (signalRegistry.count(xyPlot.xSignalName) && signalRegistry.count(xyPlot.ySignalName)) {
        Signal &xSig = signalRegistry[xyPlot.xSignalName];
        Signal &ySig = signalRegistry[xyPlot.ySignalName];

        if (!xSig.dataY.empty() && !ySig.dataY.empty()) {
          // Get the most recent values
          int xIdx = (xSig.offset == 0) ? xSig.dataY.size() - 1 : xSig.offset - 1;
          int yIdx = (ySig.offset == 0) ? ySig.dataY.size() - 1 : ySig.offset - 1;
          double xVal = xSig.dataY[xIdx];
          double yVal = ySig.dataY[yIdx];

          // Add to history with circular buffer
          if (xyPlot.historyX.size() < xyPlot.maxHistorySize) {
            xyPlot.historyX.push_back(xVal);
            xyPlot.historyY.push_back(yVal);
          } else {
            xyPlot.historyX[xyPlot.historyOffset] = xVal;
            xyPlot.historyY[xyPlot.historyOffset] = yVal;
            xyPlot.historyOffset = (xyPlot.historyOffset + 1) % xyPlot.maxHistorySize;
          }
        }
      }
    }

    if (ImPlot::BeginPlot("##XYPlot", ImVec2(-1, -1))) {
      // Set axis labels
      std::string xLabel = xyPlot.xSignalName.empty() ? "X Axis" : xyPlot.xSignalName;
      std::string yLabel = xyPlot.ySignalName.empty() ? "Y Axis" : xyPlot.ySignalName;
      ImPlot::SetupAxes(xLabel.c_str(), yLabel.c_str(), ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

      // Show instructions if no signals assigned
      if (xyPlot.xSignalName.empty() || xyPlot.ySignalName.empty()) {
        // Draw drop zones for X and Y signals
        ImVec2 plotPos = ImPlot::GetPlotPos();
        ImVec2 plotSize = ImPlot::GetPlotSize();

        // Text instructions
        ImPlot::PlotText(xyPlot.xSignalName.empty() ? "Drop signal for X axis" : "X: OK",
                        0, 0);
        if (xyPlot.ySignalName.empty()) {
          ImPlot::PlotText("Drop signal for Y axis", 0, 0.5);
        }
      }

      // Drag & Drop Target for X/Y signals
      if (ImPlot::BeginDragDropTargetPlot()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SIGNAL_NAME")) {
          std::string droppedName = (const char *)payload->Data;

          // First drop goes to X, second to Y
          if (xyPlot.xSignalName.empty()) {
            xyPlot.xSignalName = droppedName;
          } else if (xyPlot.ySignalName.empty()) {
            xyPlot.ySignalName = droppedName;
          } else {
            // Both assigned, replace X and shift Y to X
            xyPlot.xSignalName = xyPlot.ySignalName;
            xyPlot.ySignalName = droppedName;
            xyPlot.historyX.clear();
            xyPlot.historyY.clear();
            xyPlot.historyOffset = 0;
          }
        }
        ImPlot::EndDragDropTarget();
      }

      // Render X/Y plot with fading effect
      if (!xyPlot.historyX.empty() && xyPlot.historyX.size() == xyPlot.historyY.size()) {
        // Draw segments with fading alpha
        int numPoints = xyPlot.historyX.size();

        for (int i = 1; i < numPoints; i++) {
          int idx1 = (xyPlot.historyOffset + i - 1) % numPoints;
          int idx2 = (xyPlot.historyOffset + i) % numPoints;

          // Calculate fade: newer points (closer to offset) are more opaque
          float progress = (float)i / (float)numPoints;
          float alpha = 0.2f + 0.8f * progress; // Range from 0.2 to 1.0

          ImVec4 color = ImPlot::GetColormapColor(0);
          color.w = alpha;

          ImPlot::SetNextLineStyle(color, 2.0f);

          double x[2] = {xyPlot.historyX[idx1], xyPlot.historyX[idx2]};
          double y[2] = {xyPlot.historyY[idx1], xyPlot.historyY[idx2]};

          ImPlot::PlotLine("##segment", x, y, 2);
        }
      }

      ImPlot::EndPlot();
    }
    ImGui::End();
  }

  // ---------------------------------------------------------
  // UI: FILE DIALOGS
  // ---------------------------------------------------------

  // Display Save Layout dialog
  if (ImGuiFileDialog::Instance()->Display("SaveLayoutDlg", ImGuiWindowFlags_None, ImVec2(800, 600))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      SaveLayout(filePathName, activePlots, activeReadoutBoxes, activeXYPlots);
    }
    ImGuiFileDialog::Instance()->Close();
  }

  // Display Open Layout dialog
  if (ImGuiFileDialog::Instance()->Display("OpenLayoutDlg", ImGuiWindowFlags_None, ImVec2(800, 600))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      if (LoadLayout(filePathName, activePlots, nextPlotId, activeReadoutBoxes, nextReadoutBoxId, activeXYPlots, nextXYPlotId)) {
        // Layout loaded successfully
      }
    }
    ImGuiFileDialog::Instance()->Close();
  }

  // ---------------------------------------------------------
  // CLEANUP PHASE (The "Erase-Remove Idiom")
  // ---------------------------------------------------------
  activePlots.erase(
      std::remove_if(activePlots.begin(), activePlots.end(),
                     [](const PlotWindow &p) { return !p.isOpen; }),
      activePlots.end());

  activeReadoutBoxes.erase(
      std::remove_if(activeReadoutBoxes.begin(), activeReadoutBoxes.end(),
                     [](const ReadoutBox &r) { return !r.isOpen; }),
      activeReadoutBoxes.end());

  activeXYPlots.erase(
      std::remove_if(activeXYPlots.begin(), activeXYPlots.end(),
                     [](const XYPlotWindow &xy) { return !xy.isOpen; }),
      activeXYPlots.end());

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