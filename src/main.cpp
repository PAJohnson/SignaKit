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
#include <cmath> // For FFT functions
#include <cstdint>
#include <ctime>
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

#define UDP_PORT 5000

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

// The Layout: List of active plot windows
std::vector<PlotWindow> activePlots;
int nextPlotId = 1; // Auto-increment ID for window titles

// Readout boxes
std::vector<ReadoutBox> activeReadoutBoxes;
int nextReadoutBoxId = 1; // Auto-increment ID for readout box titles

// X/Y plots
std::vector<XYPlotWindow> activeXYPlots;
int nextXYPlotId = 1; // Auto-increment ID for X/Y plot titles

// Histograms
std::vector<HistogramWindow> activeHistograms;
int nextHistogramId = 1; // Auto-increment ID for histogram titles

// FFT plots
std::vector<FFTWindow> activeFFTs;
int nextFFTId = 1; // Auto-increment ID for FFT titles

// Spectrogram plots
std::vector<SpectrogramWindow> activeSpectrograms;
int nextSpectrogramId = 1; // Auto-increment ID for spectrogram titles

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

// Note: ReadValue function moved to UDPDataSink.hpp

// Helper to load a binary log file and populate signal registry
bool LoadLogFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    fprintf(stderr, "Failed to open log file: %s\n", filename.c_str());
    return false;
  }

  // Get file size
  std::streamsize fileSize = file.tellg();
  file.seekg(0, std::ios::beg);

  printf("Loading log file: %s (%lld bytes)\n", filename.c_str(), (long long)fileSize);

  // Load packet definitions
  std::vector<PacketDefinition> packets;
  if (!SignalConfigLoader::Load("signals.yaml", packets)) {
    fprintf(stderr, "Failed to load signals.yaml for offline playback\n");
    return false;
  }

  // Clear existing signal registry and reinitialize for offline mode
  // NOTE: Caller must already hold stateMutex
  signalRegistry.clear();
  for (const auto &pkt : packets) {
    for (const auto &sig : pkt.signals) {
      signalRegistry[sig.key] = Signal(sig.key, 2000, PlaybackMode::OFFLINE);
    }
  }

  // Read and parse the entire file
  char buffer[1024];
  int packetsProcessed = 0;
  double minTime = std::numeric_limits<double>::max();
  double maxTime = std::numeric_limits<double>::lowest();

  std::streampos currentPos = 0;
  file.seekg(0, std::ios::beg);

  while (currentPos < fileSize) {
    // Read a chunk at current position
    file.seekg(currentPos);
    std::streamsize remainingBytes = fileSize - currentPos;
    std::streamsize bytesToRead = std::min(remainingBytes, (std::streamsize)sizeof(buffer));

    file.read(buffer, bytesToRead);
    std::streamsize bytesRead = file.gcount();

    if (bytesRead == 0) {
      printf("Warning: Failed to read at position %lld\n", (long long)currentPos);
      break;
    }

    // Try to match packet by header string
    bool matched = false;
    for (const PacketDefinition &pkt : packets) {
      if (bytesRead >= (std::streamsize)pkt.sizeCheck &&
          strncmp(buffer, pkt.headerString.c_str(), pkt.headerString.length()) == 0) {

        // Matched packet! Process all signals
        // NOTE: Caller already holds stateMutex
        for (const auto &sig : pkt.signals) {
          double t = ReadValue(buffer, sig.timeType, sig.timeOffset);
          double v = ReadValue(buffer, sig.type, sig.offset);

          // Update signal registry (no lock needed - caller holds it)
          signalRegistry[sig.key].AddPoint(t, v);

          // Track time range
          if (t < minTime) minTime = t;
          if (t > maxTime) maxTime = t;
        }

        // Execute Lua packet callbacks (NOTE: caller holds stateMutex)
        luaScriptManager.executePacketCallbacks(pkt.id, signalRegistry, PlaybackMode::OFFLINE);

        packetsProcessed++;
        matched = true;

        // Advance to next packet
        currentPos += pkt.sizeCheck;
        break;
      }
    }

    if (!matched) {
      // Unknown data, skip one byte and try again
      currentPos += 1;
    }

    // Progress indicator every 10000 packets
    if (packetsProcessed % 10000 == 0 && packetsProcessed > 0) {
      printf("Progress: %d packets processed (%.1f%%)...\n",
             packetsProcessed,
             100.0 * currentPos / fileSize);
    }
  }

  file.close();
  printf("Parsing complete.\n");

  // Update offline playback state
  offlineState.minTime = minTime;
  offlineState.maxTime = maxTime;
  offlineState.currentWindowStart = minTime;
  offlineState.windowWidth = maxTime - minTime;  // Show entire file initially
  offlineState.fileLoaded = true;
  offlineState.loadedFilePath = filename;

  printf("Log file loaded: %d packets processed\n", packetsProcessed);
  printf("Time range: %.3f - %.3f seconds (duration: %.3f s)\n",
         minTime, maxTime, maxTime - minTime);

  return true;
}

// -------------------------------------------------------------------------
// LAYOUT SAVE/LOAD
// -------------------------------------------------------------------------
bool SaveLayout(const std::string &filename, const std::vector<PlotWindow> &plots, const std::vector<ReadoutBox> &readouts, const std::vector<XYPlotWindow> &xyPlots, const std::vector<HistogramWindow> &histograms, const std::vector<FFTWindow> &ffts, const std::vector<SpectrogramWindow> &spectrograms) {
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

    // Save histograms
    out << YAML::Key << "histograms" << YAML::Value << YAML::BeginSeq;
    for (const auto &histogram : histograms) {
      out << YAML::BeginMap;
      out << YAML::Key << "id" << YAML::Value << histogram.id;
      out << YAML::Key << "title" << YAML::Value << histogram.title;
      out << YAML::Key << "signal" << YAML::Value << histogram.signalName;
      out << YAML::Key << "numBins" << YAML::Value << histogram.numBins;
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    // Save FFT plots
    out << YAML::Key << "ffts" << YAML::Value << YAML::BeginSeq;
    for (const auto &fft : ffts) {
      out << YAML::BeginMap;
      out << YAML::Key << "id" << YAML::Value << fft.id;
      out << YAML::Key << "title" << YAML::Value << fft.title;
      out << YAML::Key << "signal" << YAML::Value << fft.signalName;
      out << YAML::Key << "fftSize" << YAML::Value << fft.fftSize;
      out << YAML::Key << "useHanning" << YAML::Value << fft.useHanning;
      out << YAML::Key << "logScale" << YAML::Value << fft.logScale;
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    // Save spectrograms
    out << YAML::Key << "spectrograms" << YAML::Value << YAML::BeginSeq;
    for (const auto &spectrogram : spectrograms) {
      out << YAML::BeginMap;
      out << YAML::Key << "id" << YAML::Value << spectrogram.id;
      out << YAML::Key << "title" << YAML::Value << spectrogram.title;
      out << YAML::Key << "signal" << YAML::Value << spectrogram.signalName;
      out << YAML::Key << "fftSize" << YAML::Value << spectrogram.fftSize;
      out << YAML::Key << "hopSize" << YAML::Value << spectrogram.hopSize;
      out << YAML::Key << "useHanning" << YAML::Value << spectrogram.useHanning;
      out << YAML::Key << "logScale" << YAML::Value << spectrogram.logScale;
      out << YAML::Key << "timeWindow" << YAML::Value << spectrogram.timeWindow;
      out << YAML::Key << "maxFrequency" << YAML::Value << spectrogram.maxFrequency;
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
                std::vector<XYPlotWindow> &xyPlots, int &nextXYPlotId,
                std::vector<HistogramWindow> &histograms, int &nextHistogramId,
                std::vector<FFTWindow> &ffts, int &nextFFTId,
                std::vector<SpectrogramWindow> &spectrograms, int &nextSpectrogramId) {
  try {
    YAML::Node config = YAML::LoadFile(filename);
    if (!config["plots"]) {
      fprintf(stderr, "Invalid layout file: missing 'plots' key\n");
      return false;
    }

    std::vector<PlotWindow> loadedPlots;
    std::vector<ReadoutBox> loadedReadouts;
    std::vector<XYPlotWindow> loadedXYPlots;
    std::vector<HistogramWindow> loadedHistograms;
    std::vector<FFTWindow> loadedFFTs;
    std::vector<SpectrogramWindow> loadedSpectrograms;
    int maxPlotId = 0;
    int maxReadoutId = 0;
    int maxXYPlotId = 0;
    int maxHistogramId = 0;
    int maxFFTId = 0;
    int maxSpectrogramId = 0;

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

    // Load histograms (if present)
    if (config["histograms"]) {
      for (const auto &histogramNode : config["histograms"]) {
        HistogramWindow histogram;
        histogram.id = histogramNode["id"].as<int>();
        histogram.title = histogramNode["title"].as<std::string>();
        histogram.signalName = histogramNode["signal"] ? histogramNode["signal"].as<std::string>() : "";
        histogram.numBins = histogramNode["numBins"] ? histogramNode["numBins"].as<int>() : 50;
        histogram.isOpen = true;

        loadedHistograms.push_back(histogram);
        if (histogram.id > maxHistogramId) {
          maxHistogramId = histogram.id;
        }
      }
    }

    // Load FFT plots (if present)
    if (config["ffts"]) {
      for (const auto &fftNode : config["ffts"]) {
        FFTWindow fft;
        fft.id = fftNode["id"].as<int>();
        fft.title = fftNode["title"].as<std::string>();
        fft.signalName = fftNode["signal"] ? fftNode["signal"].as<std::string>() : "";
        fft.fftSize = fftNode["fftSize"] ? fftNode["fftSize"].as<int>() : 2048;
        fft.useHanning = fftNode["useHanning"] ? fftNode["useHanning"].as<bool>() : true;
        fft.logScale = fftNode["logScale"] ? fftNode["logScale"].as<bool>() : true;
        fft.isOpen = true;

        loadedFFTs.push_back(fft);
        if (fft.id > maxFFTId) {
          maxFFTId = fft.id;
        }
      }
    }

    // Load spectrograms (if present)
    if (config["spectrograms"]) {
      for (const auto &spectrogramNode : config["spectrograms"]) {
        SpectrogramWindow spectrogram;
        spectrogram.id = spectrogramNode["id"].as<int>();
        spectrogram.title = spectrogramNode["title"].as<std::string>();
        spectrogram.signalName = spectrogramNode["signal"] ? spectrogramNode["signal"].as<std::string>() : "";
        spectrogram.fftSize = spectrogramNode["fftSize"] ? spectrogramNode["fftSize"].as<int>() : 512;
        spectrogram.hopSize = spectrogramNode["hopSize"] ? spectrogramNode["hopSize"].as<int>() : 128;
        spectrogram.useHanning = spectrogramNode["useHanning"] ? spectrogramNode["useHanning"].as<bool>() : true;
        spectrogram.logScale = spectrogramNode["logScale"] ? spectrogramNode["logScale"].as<bool>() : true;
        spectrogram.timeWindow = spectrogramNode["timeWindow"] ? spectrogramNode["timeWindow"].as<double>() : 5.0;
        spectrogram.maxFrequency = spectrogramNode["maxFrequency"] ? spectrogramNode["maxFrequency"].as<int>() : 0;
        spectrogram.isOpen = true;

        loadedSpectrograms.push_back(spectrogram);
        if (spectrogram.id > maxSpectrogramId) {
          maxSpectrogramId = spectrogram.id;
        }
      }
    }

    plots = loadedPlots;
    readouts = loadedReadouts;
    xyPlots = loadedXYPlots;
    histograms = loadedHistograms;
    ffts = loadedFFTs;
    spectrograms = loadedSpectrograms;
    nextPlotId = maxPlotId + 1;
    nextReadoutId = maxReadoutId + 1;
    nextXYPlotId = maxXYPlotId + 1;
    nextHistogramId = maxHistogramId + 1;
    nextFFTId = maxFFTId + 1;
    nextSpectrogramId = maxSpectrogramId + 1;
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
      udpSink = new UDPDataSink(signalRegistry, packets, ip, port, &logFile, &logFileMutex);

      // Set Lua packet callback
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

    if (ImGui::BeginMenu("Scripts")) {
      if (ImGui::MenuItem("Reload All Scripts")) {
        luaScriptManager.reloadAllScripts();
      }
      if (ImGui::MenuItem("Load Script...")) {
        IGFD::FileDialogConfig config;
        config.path = "scripts";
        ImGuiFileDialog::Instance()->OpenDialog("LoadScriptDlg", "Load Lua Script", ".lua", config);
      }
      ImGui::Separator();

      // List loaded scripts
      const auto& scripts = luaScriptManager.getScripts();
      if (scripts.empty()) {
        ImGui::TextDisabled("No scripts loaded");
      } else {
        for (const auto& script : scripts) {
          bool enabled = script.enabled;
          if (ImGui::MenuItem(script.name.c_str(), nullptr, &enabled)) {
            luaScriptManager.setScriptEnabled(script.name, enabled);
          }
          if (script.hasError) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "ERROR");
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("%s", script.lastError.c_str());
            }
          }
        }
      }

      ImGui::EndMenu();
    }

    // Add spacing
    ImGui::Separator();

    // Online/Offline Mode Toggle
    ImGui::Text("Mode:");
    ImGui::SameLine();
    bool isOnline = (currentPlaybackMode == PlaybackMode::ONLINE);
    if (ImGui::RadioButton("Online", isOnline)) {
      if (!isOnline) {
        // Switching to online mode
        currentPlaybackMode = PlaybackMode::ONLINE;
        offlineState.fileLoaded = false;

        // Disconnect network if connected
        if (networkConnected.load()) {
          networkShouldConnect = false;
        }

        // Clear and reinitialize signal registry for online mode
        std::lock_guard<std::mutex> lock(stateMutex);
        signalRegistry.clear();
        // Signals will be reinitialized by network thread
      }
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Offline", !isOnline)) {
      if (isOnline) {
        // Switching to offline mode
        currentPlaybackMode = PlaybackMode::OFFLINE;

        // Disconnect network if connected
        if (networkConnected.load()) {
          networkShouldConnect = false;
        }
      }
    }

    ImGui::Separator();

    // Conditional UI based on mode
    if (currentPlaybackMode == PlaybackMode::ONLINE) {
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
    } else {
      // Offline mode: Open File button
      if (ImGui::Button("Open Log File")) {
        IGFD::FileDialogConfig config;
        config.path = ".";
        ImGuiFileDialog::Instance()->OpenDialog("OpenLogFileDlg", "Open Log File", ".bin", config);
      }

      // Display loaded file info
      if (offlineState.fileLoaded) {
        ImGui::SameLine();
        ImGui::Text("Loaded: %s (%.2fs)",
                    offlineState.loadedFilePath.c_str(),
                    offlineState.maxTime - offlineState.minTime);
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
    if (ImGui::MenuItem("Histogram")) {
      HistogramWindow newHistogram;
      newHistogram.id = nextHistogramId++;
      newHistogram.title = "Histogram " + std::to_string(newHistogram.id);
      newHistogram.signalName = ""; // Empty initially
      activeHistograms.push_back(newHistogram);
    }
    if (ImGui::MenuItem("FFT Plot")) {
      FFTWindow newFFT;
      newFFT.id = nextFFTId++;
      newFFT.title = "FFT " + std::to_string(newFFT.id);
      newFFT.signalName = ""; // Empty initially
      activeFFTs.push_back(newFFT);
    }
    if (ImGui::MenuItem("Spectrogram")) {
      SpectrogramWindow newSpectrogram;
      newSpectrogram.id = nextSpectrogramId++;
      newSpectrogram.title = "Spectrogram " + std::to_string(newSpectrogram.id);
      newSpectrogram.signalName = ""; // Empty initially
      activeSpectrograms.push_back(newSpectrogram);
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
      if (currentPlaybackMode == PlaybackMode::OFFLINE && offlineState.fileLoaded) {
        // Offline mode: use time window from slider
        double windowEnd = offlineState.currentWindowStart + offlineState.windowWidth;
        ImPlot::SetupAxisLimits(ImAxis_X1, offlineState.currentWindowStart, windowEnd,
                                ImGuiCond_Always);
      } else if (!plot.paused && !plot.signalNames.empty()) {
        // Online mode: auto-scroll to show last 5 seconds
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
          // Get the value to display based on mode
          double currentValue;
          if (currentPlaybackMode == PlaybackMode::OFFLINE && offlineState.fileLoaded) {
            // Offline mode: find the value at the current time window end
            double targetTime = offlineState.currentWindowStart + offlineState.windowWidth;
            // Find the last value before or at targetTime
            int idx = sig.dataY.size() - 1;
            for (int i = sig.dataX.size() - 1; i >= 0; i--) {
              if (sig.dataX[i] <= targetTime) {
                idx = i;
                break;
              }
            }
            currentValue = sig.dataY[idx];
          } else {
            // Online mode: get the most recent value using circular buffer logic
            int idx = (sig.offset == 0) ? sig.dataY.size() - 1 : sig.offset - 1;
            currentValue = sig.dataY[idx];
          }

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

    // Update history based on mode
    if (!xyPlot.paused && !xyPlot.xSignalName.empty() && !xyPlot.ySignalName.empty()) {
      if (signalRegistry.count(xyPlot.xSignalName) && signalRegistry.count(xyPlot.ySignalName)) {
        Signal &xSig = signalRegistry[xyPlot.xSignalName];
        Signal &ySig = signalRegistry[xyPlot.ySignalName];

        if (currentPlaybackMode == PlaybackMode::OFFLINE && offlineState.fileLoaded) {
          // Offline mode: rebuild history from signals within time window
          xyPlot.historyX.clear();
          xyPlot.historyY.clear();
          xyPlot.historyOffset = 0;

          double windowStart = offlineState.currentWindowStart;
          double windowEnd = offlineState.currentWindowStart + offlineState.windowWidth;

          // Find overlapping time points (use xSig's time as reference)
          for (size_t i = 0; i < xSig.dataX.size(); i++) {
            if (xSig.dataX[i] >= windowStart && xSig.dataX[i] <= windowEnd) {
              // Find corresponding Y value at the same or closest time
              // For simplicity, assume signals are sampled together (same index)
              if (i < ySig.dataY.size()) {
                xyPlot.historyX.push_back(xSig.dataY[i]);
                xyPlot.historyY.push_back(ySig.dataY[i]);
              }
            }
          }
        } else if (!xSig.dataY.empty() && !ySig.dataY.empty()) {
          // Online mode: get the most recent values and add to circular buffer
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
  // UI: HISTOGRAMS
  // ---------------------------------------------------------

  // Loop through all active histograms
  for (auto &histogram : activeHistograms) {
    if (!histogram.isOpen)
      continue;

    // Set initial position below menu bar for new histograms
    ImGui::SetNextWindowPos(ImVec2(350, menuBarHeight + 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

    // Use a stable ID (based on histogram.id) while displaying dynamic title
    std::string windowID = histogram.title + "##Histogram" + std::to_string(histogram.id);
    ImGui::Begin(windowID.c_str(), &histogram.isOpen);

    // Display content based on whether a signal is assigned
    if (histogram.signalName.empty()) {
      // Show drag-and-drop message
      ImVec2 windowSize = ImGui::GetWindowSize();
      const char* text = "Drag and drop a signal here";
      ImVec2 textSize = ImGui::CalcTextSize(text);
      ImGui::SetCursorPosX((windowSize.x - textSize.x) * 0.5f);
      ImGui::SetCursorPosY((windowSize.y - textSize.y) * 0.5f);
      ImGui::TextWrapped("%s", text);

      // Accept drag-and-drop for the entire window
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SIGNAL_NAME")) {
          std::string droppedName = (const char *)payload->Data;
          histogram.signalName = droppedName;
          histogram.title = droppedName;
        }
        ImGui::EndDragDropTarget();
      }
    } else {
      // Display the histogram
      if (signalRegistry.count(histogram.signalName)) {
        Signal &sig = signalRegistry[histogram.signalName];

        // Controls at the top
        ImGui::Text("Bins:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::SliderInt("##Bins", &histogram.numBins, 10, 200);
        ImGui::SameLine();
        if (ImGui::Button("Clear Signal")) {
          histogram.signalName = "";
          histogram.title = "Histogram " + std::to_string(histogram.id);
        }

        if (!sig.dataY.empty()) {
          // Collect data points based on mode
          std::vector<double> dataToHistogram;

          if (currentPlaybackMode == PlaybackMode::OFFLINE && offlineState.fileLoaded) {
            // Offline mode: collect all data up to current time window end
            double targetTime = offlineState.currentWindowStart + offlineState.windowWidth;
            for (size_t i = 0; i < sig.dataX.size(); i++) {
              if (sig.dataX[i] <= targetTime) {
                dataToHistogram.push_back(sig.dataY[i]);
              } else {
                break; // Assuming data is chronologically ordered
              }
            }
          } else {
            // Online mode: use all data in the circular buffer
            if (sig.mode == PlaybackMode::ONLINE && sig.dataY.size() == sig.maxSize) {
              // Buffer is full, use circular logic
              for (size_t i = 0; i < sig.dataY.size(); i++) {
                int idx = (sig.offset + i) % sig.dataY.size();
                dataToHistogram.push_back(sig.dataY[idx]);
              }
            } else {
              // Buffer not full yet, use all data
              dataToHistogram = sig.dataY;
            }
          }

          if (!dataToHistogram.empty()) {
            // Plot the histogram
            if (ImPlot::BeginPlot("##Histogram", ImVec2(-1, -1))) {
              ImPlot::SetupAxes("Value", "Count", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

              ImPlot::PlotHistogram("##HistData", dataToHistogram.data(),
                                   (int)dataToHistogram.size(), histogram.numBins);

              ImPlot::EndPlot();
            }
          } else {
            ImGui::TextDisabled("No data in current time window");
          }
        } else {
          ImGui::TextDisabled("No data");
        }
      } else {
        ImGui::TextDisabled("Signal not found");
      }

      // Accept drag-and-drop to change signal
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SIGNAL_NAME")) {
          std::string droppedName = (const char *)payload->Data;
          histogram.signalName = droppedName;
          histogram.title = droppedName;
        }
        ImGui::EndDragDropTarget();
      }
    }

    ImGui::End();
  }

  // ---------------------------------------------------------
  // UI: FFT PLOTS
  // ---------------------------------------------------------

  // Loop through all active FFT plots
  for (auto &fft : activeFFTs) {
    if (!fft.isOpen)
      continue;

    // Set initial position below menu bar for new FFT windows
    ImGui::SetNextWindowPos(ImVec2(350, menuBarHeight + 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    // Use a stable ID (based on fft.id) while displaying dynamic title
    std::string windowID = fft.title + "##FFT" + std::to_string(fft.id);
    ImGui::Begin(windowID.c_str(), &fft.isOpen);

    // Display content based on whether a signal is assigned
    if (fft.signalName.empty()) {
      // Show drag-and-drop message
      ImVec2 windowSize = ImGui::GetWindowSize();
      const char* text = "Drag and drop a signal here";
      ImVec2 textSize = ImGui::CalcTextSize(text);
      ImGui::SetCursorPosX((windowSize.x - textSize.x) * 0.5f);
      ImGui::SetCursorPosY((windowSize.y - textSize.y) * 0.5f);
      ImGui::TextWrapped("%s", text);

      // Accept drag-and-drop for the entire window
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SIGNAL_NAME")) {
          std::string droppedName = (const char *)payload->Data;
          fft.signalName = droppedName;
          fft.title = droppedName + " FFT";
        }
        ImGui::EndDragDropTarget();
      }
    } else {
      // Display the FFT
      if (signalRegistry.count(fft.signalName)) {
        Signal &sig = signalRegistry[fft.signalName];

        // Controls at the top
        ImGui::Text("FFT Size:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        const char* fftSizeItems[] = { "128", "256", "512", "1024", "2048", "4096", "8192" };
        int fftSizeIdx = 4; // Default to 2048
        if (fft.fftSize == 128) fftSizeIdx = 0;
        else if (fft.fftSize == 256) fftSizeIdx = 1;
        else if (fft.fftSize == 512) fftSizeIdx = 2;
        else if (fft.fftSize == 1024) fftSizeIdx = 3;
        else if (fft.fftSize == 2048) fftSizeIdx = 4;
        else if (fft.fftSize == 4096) fftSizeIdx = 5;
        else if (fft.fftSize == 8192) fftSizeIdx = 6;

        if (ImGui::Combo("##FFTSize", &fftSizeIdx, fftSizeItems, IM_ARRAYSIZE(fftSizeItems))) {
          fft.fftSize = 1 << (fftSizeIdx + 7); // 2^(idx+7): 128, 256, 512, 1024, 2048, 4096, 8192
        }

        ImGui::SameLine();
        ImGui::Checkbox("Hanning Window", &fft.useHanning);

        ImGui::SameLine();
        ImGui::Checkbox("Log Scale (dB)", &fft.logScale);

        ImGui::SameLine();
        if (ImGui::Button("Clear Signal")) {
          fft.signalName = "";
          fft.title = "FFT " + std::to_string(fft.id);
        }

        if (!sig.dataY.empty()) {
          // Collect data points based on mode
          std::vector<double> dataToFFT;
          std::vector<double> timeToFFT;

          if (currentPlaybackMode == PlaybackMode::OFFLINE && offlineState.fileLoaded) {
            // Offline mode: collect all data up to current time window end
            double targetTime = offlineState.currentWindowStart + offlineState.windowWidth;
            for (size_t i = 0; i < sig.dataX.size(); i++) {
              if (sig.dataX[i] <= targetTime) {
                timeToFFT.push_back(sig.dataX[i]);
                dataToFFT.push_back(sig.dataY[i]);
              } else {
                break; // Assuming data is chronologically ordered
              }
            }
          } else {
            // Online mode: use all data in the circular buffer
            if (sig.mode == PlaybackMode::ONLINE && sig.dataY.size() == sig.maxSize) {
              // Buffer is full, use circular logic
              for (size_t i = 0; i < sig.dataY.size(); i++) {
                int idx = (sig.offset + i) % sig.dataY.size();
                timeToFFT.push_back(sig.dataX[idx]);
                dataToFFT.push_back(sig.dataY[idx]);
              }
            } else {
              // Buffer not full yet, use all data
              timeToFFT = sig.dataX;
              dataToFFT = sig.dataY;
            }
          }

          if (dataToFFT.size() >= (size_t)fft.fftSize) {
            // Compute FFT spectrum
            std::vector<double> freqBins;
            std::vector<double> magnitude;

            ComputeFFTSpectrum(dataToFFT, timeToFFT, fft.fftSize, fft.useHanning, fft.logScale, freqBins, magnitude);

            if (!freqBins.empty() && !magnitude.empty()) {
              // Calculate sampling frequency for display
              double fs = CalculateSamplingFrequency(timeToFFT, std::max(0, (int)timeToFFT.size() - fft.fftSize),
                                                     std::min(fft.fftSize, (int)timeToFFT.size()));

              ImGui::Text("Sampling Frequency: %.2f Hz | Frequency Resolution: %.3f Hz",
                         fs, fs / fft.fftSize);

              // Plot the FFT spectrum
              if (ImPlot::BeginPlot("##FFTPlot", ImVec2(-1, -1))) {
                const char* yAxisLabel = fft.logScale ? "Magnitude (dB)" : "Magnitude";
                ImPlot::SetupAxes("Frequency (Hz)", yAxisLabel, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

                ImPlot::PlotLine("##FFTLine", freqBins.data(), magnitude.data(), (int)freqBins.size());

                ImPlot::EndPlot();
              }
            } else {
              ImGui::TextDisabled("FFT computation failed");
            }
          } else {
            ImGui::TextDisabled("Not enough data for FFT (need %d samples, have %zu)", fft.fftSize, dataToFFT.size());
          }
        } else {
          ImGui::TextDisabled("No data");
        }
      } else {
        ImGui::TextDisabled("Signal not found");
      }

      // Accept drag-and-drop to change signal
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SIGNAL_NAME")) {
          std::string droppedName = (const char *)payload->Data;
          fft.signalName = droppedName;
          fft.title = droppedName + " FFT";
        }
        ImGui::EndDragDropTarget();
      }
    }

    ImGui::End();
  }

  // ---------------------------------------------------------
  // UI: SPECTROGRAMS
  // ---------------------------------------------------------

  // Loop through all active spectrograms
  for (auto &spectrogram : activeSpectrograms) {
    if (!spectrogram.isOpen)
      continue;

    // Set initial position below menu bar for new spectrograms
    ImGui::SetNextWindowPos(ImVec2(350, menuBarHeight + 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);

    // Use a stable ID (based on spectrogram.id) while displaying dynamic title
    std::string windowID = spectrogram.title + "##Spectrogram" + std::to_string(spectrogram.id);
    ImGui::Begin(windowID.c_str(), &spectrogram.isOpen);

    // Display content based on whether a signal is assigned
    if (spectrogram.signalName.empty()) {
      // Show drag-and-drop message
      ImVec2 windowSize = ImGui::GetWindowSize();
      const char* text = "Drag and drop a signal here";
      ImVec2 textSize = ImGui::CalcTextSize(text);
      ImGui::SetCursorPosX((windowSize.x - textSize.x) * 0.5f);
      ImGui::SetCursorPosY((windowSize.y - textSize.y) * 0.5f);
      ImGui::TextWrapped("%s", text);

      // Accept drag-and-drop for the entire window
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SIGNAL_NAME")) {
          std::string droppedName = (const char *)payload->Data;
          spectrogram.signalName = droppedName;
          spectrogram.title = droppedName + " Spectrogram";
        }
        ImGui::EndDragDropTarget();
      }
    } else {
      // Display the spectrogram
      if (signalRegistry.count(spectrogram.signalName)) {
        Signal &sig = signalRegistry[spectrogram.signalName];

        // Controls at the top
        ImGui::Text("FFT Size:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        const char* fftSizeItems[] = { "128", "256", "512", "1024", "2048" };
        int fftSizeIdx = 2; // Default to 512
        if (spectrogram.fftSize == 128) fftSizeIdx = 0;
        else if (spectrogram.fftSize == 256) fftSizeIdx = 1;
        else if (spectrogram.fftSize == 512) fftSizeIdx = 2;
        else if (spectrogram.fftSize == 1024) fftSizeIdx = 3;
        else if (spectrogram.fftSize == 2048) fftSizeIdx = 4;

        if (ImGui::Combo("##SpectFFTSize", &fftSizeIdx, fftSizeItems, IM_ARRAYSIZE(fftSizeItems))) {
          spectrogram.fftSize = 1 << (fftSizeIdx + 7); // 2^(idx+7): 128, 256, 512, 1024, 2048
          // Adjust hop size to be fftSize/4 for 75% overlap
          spectrogram.hopSize = spectrogram.fftSize / 4;
        }

        ImGui::SameLine();
        ImGui::Text("Time Window:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        float timeWindowFloat = (float)spectrogram.timeWindow;
        if (ImGui::DragFloat("##TimeWindow", &timeWindowFloat, 0.1f, 0.5f, 60.0f, "%.1f s")) {
          spectrogram.timeWindow = (double)timeWindowFloat;
        }

        ImGui::SameLine();
        ImGui::Checkbox("Hanning", &spectrogram.useHanning);

        ImGui::SameLine();
        ImGui::Checkbox("Log (dB)", &spectrogram.logScale);

        ImGui::SameLine();
        if (ImGui::Button("Clear Signal")) {
          spectrogram.signalName = "";
          spectrogram.title = "Spectrogram " + std::to_string(spectrogram.id);
        }

        // Second row of controls
        ImGui::Text("Max Frequency:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::DragInt("##MaxFreq", &spectrogram.maxFrequency, 10.0f, 0, 5000, spectrogram.maxFrequency == 0 ? "Auto" : "%d Hz");

        ImGui::SameLine();
        ImGui::Text("Colormap:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        const char* colormapItems[] = { "Viridis", "Plasma", "Magma", "Inferno", "ImPlot Default" };
        int colormapIdx = (int)spectrogram.colormap;
        if (ImGui::Combo("##Colormap", &colormapIdx, colormapItems, IM_ARRAYSIZE(colormapItems))) {
          spectrogram.colormap = (Colormap)colormapIdx;
        }

        ImGui::SameLine();
        ImGui::Checkbox("Interpolation", &spectrogram.useInterpolation);

        if (!sig.dataY.empty()) {
          // Collect data points based on mode
          std::vector<double> dataToAnalyze;
          std::vector<double> timeToAnalyze;

          if (currentPlaybackMode == PlaybackMode::OFFLINE && offlineState.fileLoaded) {
            // Offline mode: collect all data up to current time window end
            double targetTime = offlineState.currentWindowStart + offlineState.windowWidth;
            for (size_t i = 0; i < sig.dataX.size(); i++) {
              if (sig.dataX[i] <= targetTime) {
                timeToAnalyze.push_back(sig.dataX[i]);
                dataToAnalyze.push_back(sig.dataY[i]);
              } else {
                break;
              }
            }
          } else {
            // Online mode: use all data in the circular buffer
            if (sig.mode == PlaybackMode::ONLINE && sig.dataY.size() == sig.maxSize) {
              // Buffer is full, use circular logic
              for (size_t i = 0; i < sig.dataY.size(); i++) {
                int idx = (sig.offset + i) % sig.dataY.size();
                timeToAnalyze.push_back(sig.dataX[idx]);
                dataToAnalyze.push_back(sig.dataY[idx]);
              }
            } else {
              // Buffer not full yet, use all data
              timeToAnalyze = sig.dataX;
              dataToAnalyze = sig.dataY;
            }
          }

          if (dataToAnalyze.size() >= (size_t)spectrogram.fftSize) {
            // Compute spectrogram
            std::vector<double> timeBins;
            std::vector<double> freqBins;
            std::vector<double> magnitudeMatrix;

            ComputeSpectrogram(dataToAnalyze, timeToAnalyze, spectrogram.fftSize, spectrogram.hopSize,
                              spectrogram.useHanning, spectrogram.logScale, spectrogram.timeWindow,
                              spectrogram.maxFrequency, timeBins, freqBins, magnitudeMatrix);

            if (!timeBins.empty() && !freqBins.empty() && !magnitudeMatrix.empty()) {
              // Calculate sampling frequency for display
              double fs = CalculateSamplingFrequency(timeToAnalyze, std::max(0, (int)timeToAnalyze.size() - spectrogram.fftSize),
                                                     std::min(spectrogram.fftSize, (int)timeToAnalyze.size()));

              ImGui::Text("Sampling Freq: %.2f Hz | Time Bins: %zu | Freq Bins: %zu | Freq Res: %.3f Hz",
                         fs, timeBins.size(), freqBins.size(), fs / spectrogram.fftSize);

              // Plot the spectrogram
              if (ImPlot::BeginPlot("##SpectrogramPlot", ImVec2(-1, -1))) {
                const char* yAxisLabel = "Frequency (Hz)";
                ImPlot::SetupAxes("Time (s)", yAxisLabel, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

                // Determine bounds
                int numTimeBins = timeBins.size();
                int numFreqBins = freqBins.size();
                double timeMin = timeBins.front();
                double timeMax = timeBins.back();
                double freqMin = freqBins.front();
                double freqMax = freqBins.back();

                // Find min/max magnitude for normalization
                double magMin = *std::min_element(magnitudeMatrix.begin(), magnitudeMatrix.end());
                double magMax = *std::max_element(magnitudeMatrix.begin(), magnitudeMatrix.end());
                double magRange = magMax - magMin;
                if (magRange < 1e-10) magRange = 1.0; // avoid division by zero

                // Use custom colormap rendering if not ImPlotDefault
                if (spectrogram.colormap != Colormap::ImPlotDefault) {
                  // Setup axis limits explicitly for custom rendering
                  ImPlot::SetupAxisLimits(ImAxis_X1, timeMin, timeMax, ImGuiCond_Always);
                  ImPlot::SetupAxisLimits(ImAxis_Y1, freqMin, freqMax, ImGuiCond_Always);

                  // Custom rendering with matplotlib-style colormaps
                  ImDrawList* draw_list = ImPlot::GetPlotDrawList();

                  // Determine pixel resolution for interpolation
                  int pixelsPerTimeBin = spectrogram.useInterpolation ? 4 : 1;
                  int pixelsPerFreqBin = spectrogram.useInterpolation ? 4 : 1;

                  // Render rectangles for each bin/pixel
                  for (int t = 0; t < numTimeBins - 1; t++) {
                    for (int tSub = 0; tSub < pixelsPerTimeBin; tSub++) {
                      double tFrac = (double)tSub / pixelsPerTimeBin;
                      double time0 = timeBins[t] + tFrac * (timeBins[t+1] - timeBins[t]);
                      double time1 = timeBins[t] + (tFrac + 1.0/pixelsPerTimeBin) * (timeBins[t+1] - timeBins[t]);

                      for (int f = 0; f < numFreqBins - 1; f++) {
                        for (int fSub = 0; fSub < pixelsPerFreqBin; fSub++) {
                          double fFrac = (double)fSub / pixelsPerFreqBin;
                          double freq0 = freqBins[f] + fFrac * (freqBins[f+1] - freqBins[f]);
                          double freq1 = freqBins[f] + (fFrac + 1.0/pixelsPerFreqBin) * (freqBins[f+1] - freqBins[f]);

                          // Bilinear interpolation
                          double mag;
                          if (spectrogram.useInterpolation) {
                            double t00 = magnitudeMatrix[t * numFreqBins + f];
                            double t10 = magnitudeMatrix[(t+1) * numFreqBins + f];
                            double t01 = magnitudeMatrix[t * numFreqBins + (f+1)];
                            double t11 = magnitudeMatrix[(t+1) * numFreqBins + (f+1)];

                            double interpT = tFrac;
                            double interpF = fFrac;
                            mag = (1-interpT)*(1-interpF)*t00 + interpT*(1-interpF)*t10 +
                                  (1-interpT)*interpF*t01 + interpT*interpF*t11;
                          } else {
                            mag = magnitudeMatrix[t * numFreqBins + f];
                          }

                          // Normalize to [0,1]
                          double normalized = (mag - magMin) / magRange;
                          ImU32 color = GetColormapColor(spectrogram.colormap, normalized);

                          // Convert to plot coordinates
                          ImVec2 p0 = ImPlot::PlotToPixels(time0, freq0);
                          ImVec2 p1 = ImPlot::PlotToPixels(time1, freq1);

                          draw_list->AddRectFilled(p0, p1, color);
                        }
                      }
                    }
                  }
                } else {
                  // Use ImPlot default heatmap
                  std::vector<double> transposedMatrix(numTimeBins * numFreqBins);
                  for (int t = 0; t < numTimeBins; t++) {
                    for (int f = 0; f < numFreqBins; f++) {
                      int srcIdx = t * numFreqBins + f;
                      int dstIdx = (numFreqBins - 1 - f) * numTimeBins + t;
                      transposedMatrix[dstIdx] = magnitudeMatrix[srcIdx];
                    }
                  }
                  ImPlot::PlotHeatmap("##HeatmapData", transposedMatrix.data(),
                                     numFreqBins, numTimeBins,
                                     0.0, 0.0,
                                     nullptr,
                                     ImPlotPoint(timeMin, freqMin),
                                     ImPlotPoint(timeMax, freqMax));
                }

                ImPlot::EndPlot();
              }
            } else {
              ImGui::TextDisabled("Spectrogram computation failed");
            }
          } else {
            ImGui::TextDisabled("Not enough data for spectrogram (need %d samples, have %zu)", spectrogram.fftSize, dataToAnalyze.size());
          }
        } else {
          ImGui::TextDisabled("No data");
        }
      } else {
        ImGui::TextDisabled("Signal not found");
      }

      // Accept drag-and-drop to change signal
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SIGNAL_NAME")) {
          std::string droppedName = (const char *)payload->Data;
          spectrogram.signalName = droppedName;
          spectrogram.title = droppedName + " Spectrogram";
        }
        ImGui::EndDragDropTarget();
      }
    }

    ImGui::End();
  }

  // ---------------------------------------------------------
  // UI: TIME SLIDER (Offline Mode Only)
  // ---------------------------------------------------------
  if (currentPlaybackMode == PlaybackMode::OFFLINE && offlineState.fileLoaded) {
    // Position at bottom of screen, above file dialogs
    float sliderHeight = 80.0f;
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - sliderHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, sliderHeight), ImGuiCond_Always);

    ImGui::Begin("Time Control", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // Time range info
    ImGui::Text("Time Range: %.3f - %.3f s (Total: %.3f s)",
                offlineState.minTime, offlineState.maxTime,
                offlineState.maxTime - offlineState.minTime);

    // Window width control with scroll wheel
    ImGui::SameLine();
    ImGui::Text("  |  Window Width: %.3f s", offlineState.windowWidth);

    if (ImGui::IsWindowHovered()) {
      float mouseWheel = io.MouseWheel;
      if (mouseWheel != 0.0f) {
        // Zoom in/out with scroll wheel
        float zoomFactor = 1.0f + (mouseWheel * 0.1f);
        offlineState.windowWidth *= zoomFactor;

        // Clamp window width
        float minWidth = 0.1f;  // Minimum 0.1 second window
        float maxWidth = offlineState.maxTime - offlineState.minTime;
        if (offlineState.windowWidth < minWidth) offlineState.windowWidth = minWidth;
        if (offlineState.windowWidth > maxWidth) offlineState.windowWidth = maxWidth;

        // Adjust currentWindowStart to keep it in bounds
        if (offlineState.currentWindowStart + offlineState.windowWidth > offlineState.maxTime) {
          offlineState.currentWindowStart = offlineState.maxTime - offlineState.windowWidth;
        }
        if (offlineState.currentWindowStart < offlineState.minTime) {
          offlineState.currentWindowStart = offlineState.minTime;
        }
      }
    }

    // Time slider
    float sliderPos = (float)offlineState.currentWindowStart;
    float sliderMax = (float)(offlineState.maxTime - offlineState.windowWidth);
    if (sliderMax < offlineState.minTime) sliderMax = (float)offlineState.minTime;

    ImGui::SetNextItemWidth(-1);  // Full width
    if (ImGui::SliderFloat("##TimeSlider", &sliderPos, (float)offlineState.minTime, sliderMax,
                           "Start: %.3f s")) {
      offlineState.currentWindowStart = sliderPos;
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
      SaveLayout(filePathName, activePlots, activeReadoutBoxes, activeXYPlots, activeHistograms, activeFFTs, activeSpectrograms);
    }
    ImGuiFileDialog::Instance()->Close();
  }

  // Display Open Layout dialog
  if (ImGuiFileDialog::Instance()->Display("OpenLayoutDlg", ImGuiWindowFlags_None, ImVec2(800, 600))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      if (LoadLayout(filePathName, activePlots, nextPlotId, activeReadoutBoxes, nextReadoutBoxId, activeXYPlots, nextXYPlotId, activeHistograms, nextHistogramId, activeFFTs, nextFFTId, activeSpectrograms, nextSpectrogramId)) {
        // Layout loaded successfully
      }
    }
    ImGuiFileDialog::Instance()->Close();
  }

  // Display Open Log File dialog
  if (ImGuiFileDialog::Instance()->Display("OpenLogFileDlg", ImGuiWindowFlags_None, ImVec2(800, 600))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      if (LoadLogFile(filePathName)) {
        // Log file loaded successfully
        printf("Successfully loaded log file into offline mode\n");
      }
    }
    ImGuiFileDialog::Instance()->Close();
  }

  // Display Load Script dialog
  if (ImGuiFileDialog::Instance()->Display("LoadScriptDlg", ImGuiWindowFlags_None, ImVec2(800, 600))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      luaScriptManager.loadScript(filePathName);
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

  // Load Lua scripts from scripts/ directory
  printf("Loading Lua scripts...\n");
  luaScriptManager.loadScriptsFromDirectory("scripts");

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