#pragma once

#include "plot_types.hpp"
#include "types.hpp"
#include "SignalConfigLoader.h"
#include "LuaScriptManager.hpp"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <limits>
#include <map>
#include <string>
#include <vector>

// Forward declarations (these are defined in main.cpp)
struct OfflinePlaybackState;

// -------------------------------------------------------------------------
// LOG FILE UTILITIES
// -------------------------------------------------------------------------

// Helper to generate timestamped filename
inline std::string GenerateLogFilename() {
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

// Helper to load a binary log file and populate signal registry
// NOTE: Caller must hold stateMutex before calling this function
inline bool LoadLogFile(const std::string &filename,
                       std::map<std::string, Signal>& signalRegistry,
                       OfflinePlaybackState& offlineState,
                       LuaScriptManager& luaScriptManager) {
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

        // Execute Lua packet callbacks via trigger_packet_callbacks() (NOTE: caller holds stateMutex)
        // Get the timestamp from the first signal
        double packetTime = 0.0;
        if (!pkt.signals.empty()) {
          packetTime = ReadValue(buffer, pkt.signals[0].timeType, pkt.signals[0].timeOffset);
        }

        // Call Lua trigger_packet_callbacks(packetType, timestamp)
        luaScriptManager.getLuaState().safe_script(
          "if trigger_packet_callbacks then trigger_packet_callbacks('" + pkt.id + "', " + std::to_string(packetTime) + ") end"
        );

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

inline bool SaveLayout(const std::string &filename,
                      const std::vector<PlotWindow> &plots,
                      const std::vector<ReadoutBox> &readouts,
                      const std::vector<XYPlotWindow> &xyPlots,
                      const std::vector<HistogramWindow> &histograms,
                      const std::vector<FFTWindow> &ffts,
                      const std::vector<SpectrogramWindow> &spectrograms,
                      const std::vector<ButtonControl> &buttons = std::vector<ButtonControl>(),
                      const std::vector<ToggleControl> &toggles = std::vector<ToggleControl>(),
                      const std::vector<TextInputControl> &textInputs = std::vector<TextInputControl>(),
                      bool editMode = true) {
  try {
    YAML::Emitter out;
    out << YAML::BeginMap;

    // Save UI settings
    out << YAML::Key << "editMode" << YAML::Value << editMode;

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
      // Save window position and size if set
      if (plot.posX >= 0.0f) {
        out << YAML::Key << "posX" << YAML::Value << plot.posX;
        out << YAML::Key << "posY" << YAML::Value << plot.posY;
      }
      if (plot.sizeX >= 0.0f) {
        out << YAML::Key << "sizeX" << YAML::Value << plot.sizeX;
        out << YAML::Key << "sizeY" << YAML::Value << plot.sizeY;
      }
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
      if (readout.posX >= 0.0f) {
        out << YAML::Key << "posX" << YAML::Value << readout.posX;
        out << YAML::Key << "posY" << YAML::Value << readout.posY;
      }
      if (readout.sizeX >= 0.0f) {
        out << YAML::Key << "sizeX" << YAML::Value << readout.sizeX;
        out << YAML::Key << "sizeY" << YAML::Value << readout.sizeY;
      }
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
      if (xyPlot.posX >= 0.0f) {
        out << YAML::Key << "posX" << YAML::Value << xyPlot.posX;
        out << YAML::Key << "posY" << YAML::Value << xyPlot.posY;
      }
      if (xyPlot.sizeX >= 0.0f) {
        out << YAML::Key << "sizeX" << YAML::Value << xyPlot.sizeX;
        out << YAML::Key << "sizeY" << YAML::Value << xyPlot.sizeY;
      }
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
      if (histogram.posX >= 0.0f) {
        out << YAML::Key << "posX" << YAML::Value << histogram.posX;
        out << YAML::Key << "posY" << YAML::Value << histogram.posY;
      }
      if (histogram.sizeX >= 0.0f) {
        out << YAML::Key << "sizeX" << YAML::Value << histogram.sizeX;
        out << YAML::Key << "sizeY" << YAML::Value << histogram.sizeY;
      }
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
      if (fft.posX >= 0.0f) {
        out << YAML::Key << "posX" << YAML::Value << fft.posX;
        out << YAML::Key << "posY" << YAML::Value << fft.posY;
      }
      if (fft.sizeX >= 0.0f) {
        out << YAML::Key << "sizeX" << YAML::Value << fft.sizeX;
        out << YAML::Key << "sizeY" << YAML::Value << fft.sizeY;
      }
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
      if (spectrogram.posX >= 0.0f) {
        out << YAML::Key << "posX" << YAML::Value << spectrogram.posX;
        out << YAML::Key << "posY" << YAML::Value << spectrogram.posY;
      }
      if (spectrogram.sizeX >= 0.0f) {
        out << YAML::Key << "sizeX" << YAML::Value << spectrogram.sizeX;
        out << YAML::Key << "sizeY" << YAML::Value << spectrogram.sizeY;
      }
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    // Save buttons (Tier 4)
    out << YAML::Key << "buttons" << YAML::Value << YAML::BeginSeq;
    for (const auto &button : buttons) {
      out << YAML::BeginMap;
      out << YAML::Key << "id" << YAML::Value << button.id;
      out << YAML::Key << "title" << YAML::Value << button.title;
      out << YAML::Key << "buttonLabel" << YAML::Value << button.buttonLabel;
      if (button.posX >= 0.0f) {
        out << YAML::Key << "posX" << YAML::Value << button.posX;
        out << YAML::Key << "posY" << YAML::Value << button.posY;
      }
      if (button.sizeX >= 0.0f) {
        out << YAML::Key << "sizeX" << YAML::Value << button.sizeX;
        out << YAML::Key << "sizeY" << YAML::Value << button.sizeY;
      }
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    // Save toggles (Tier 4)
    out << YAML::Key << "toggles" << YAML::Value << YAML::BeginSeq;
    for (const auto &toggle : toggles) {
      out << YAML::BeginMap;
      out << YAML::Key << "id" << YAML::Value << toggle.id;
      out << YAML::Key << "title" << YAML::Value << toggle.title;
      out << YAML::Key << "toggleLabel" << YAML::Value << toggle.toggleLabel;
      out << YAML::Key << "state" << YAML::Value << toggle.state;
      if (toggle.posX >= 0.0f) {
        out << YAML::Key << "posX" << YAML::Value << toggle.posX;
        out << YAML::Key << "posY" << YAML::Value << toggle.posY;
      }
      if (toggle.sizeX >= 0.0f) {
        out << YAML::Key << "sizeX" << YAML::Value << toggle.sizeX;
        out << YAML::Key << "sizeY" << YAML::Value << toggle.sizeY;
      }
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    // Save text inputs (Tier 4)
    out << YAML::Key << "textInputs" << YAML::Value << YAML::BeginSeq;
    for (const auto &textInput : textInputs) {
      out << YAML::BeginMap;
      out << YAML::Key << "id" << YAML::Value << textInput.id;
      out << YAML::Key << "title" << YAML::Value << textInput.title;
      out << YAML::Key << "textBuffer" << YAML::Value << std::string(textInput.textBuffer);
      if (textInput.posX >= 0.0f) {
        out << YAML::Key << "posX" << YAML::Value << textInput.posX;
        out << YAML::Key << "posY" << YAML::Value << textInput.posY;
      }
      if (textInput.sizeX >= 0.0f) {
        out << YAML::Key << "sizeX" << YAML::Value << textInput.sizeX;
        out << YAML::Key << "sizeY" << YAML::Value << textInput.sizeY;
      }
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

inline bool LoadLayout(const std::string &filename,
                      std::vector<PlotWindow> &plots, int &nextPlotId,
                      std::vector<ReadoutBox> &readouts, int &nextReadoutId,
                      std::vector<XYPlotWindow> &xyPlots, int &nextXYPlotId,
                      std::vector<HistogramWindow> &histograms, int &nextHistogramId,
                      std::vector<FFTWindow> &ffts, int &nextFFTId,
                      std::vector<SpectrogramWindow> &spectrograms, int &nextSpectrogramId,
                      std::vector<ButtonControl> *buttons = nullptr, int *nextButtonId = nullptr,
                      std::vector<ToggleControl> *toggles = nullptr, int *nextToggleId = nullptr,
                      std::vector<TextInputControl> *textInputs = nullptr, int *nextTextInputId = nullptr,
                      bool *editMode = nullptr) {
  try {
    YAML::Node config = YAML::LoadFile(filename);
    if (!config["plots"]) {
      fprintf(stderr, "Invalid layout file: missing 'plots' key\n");
      return false;
    }

    // Load UI settings
    if (config["editMode"] && editMode != nullptr) {
      *editMode = config["editMode"].as<bool>();
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

      // Load window position and size if present
      if (plotNode["posX"]) {
        plot.posX = plotNode["posX"].as<float>();
        plot.posY = plotNode["posY"].as<float>();
      }
      if (plotNode["sizeX"]) {
        plot.sizeX = plotNode["sizeX"].as<float>();
        plot.sizeY = plotNode["sizeY"].as<float>();
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

        if (readoutNode["posX"]) {
          readout.posX = readoutNode["posX"].as<float>();
          readout.posY = readoutNode["posY"].as<float>();
        }
        if (readoutNode["sizeX"]) {
          readout.sizeX = readoutNode["sizeX"].as<float>();
          readout.sizeY = readoutNode["sizeY"].as<float>();
        }

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

        if (xyPlotNode["posX"]) {
          xyPlot.posX = xyPlotNode["posX"].as<float>();
          xyPlot.posY = xyPlotNode["posY"].as<float>();
        }
        if (xyPlotNode["sizeX"]) {
          xyPlot.sizeX = xyPlotNode["sizeX"].as<float>();
          xyPlot.sizeY = xyPlotNode["sizeY"].as<float>();
        }

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

        if (histogramNode["posX"]) {
          histogram.posX = histogramNode["posX"].as<float>();
          histogram.posY = histogramNode["posY"].as<float>();
        }
        if (histogramNode["sizeX"]) {
          histogram.sizeX = histogramNode["sizeX"].as<float>();
          histogram.sizeY = histogramNode["sizeY"].as<float>();
        }

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

        if (fftNode["posX"]) {
          fft.posX = fftNode["posX"].as<float>();
          fft.posY = fftNode["posY"].as<float>();
        }
        if (fftNode["sizeX"]) {
          fft.sizeX = fftNode["sizeX"].as<float>();
          fft.sizeY = fftNode["sizeY"].as<float>();
        }

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

        if (spectrogramNode["posX"]) {
          spectrogram.posX = spectrogramNode["posX"].as<float>();
          spectrogram.posY = spectrogramNode["posY"].as<float>();
        }
        if (spectrogramNode["sizeX"]) {
          spectrogram.sizeX = spectrogramNode["sizeX"].as<float>();
          spectrogram.sizeY = spectrogramNode["sizeY"].as<float>();
        }

        loadedSpectrograms.push_back(spectrogram);
        if (spectrogram.id > maxSpectrogramId) {
          maxSpectrogramId = spectrogram.id;
        }
      }
    }

    // Load buttons (Tier 4, if present)
    std::vector<ButtonControl> loadedButtons;
    int maxButtonId = 0;
    if (config["buttons"] && buttons != nullptr && nextButtonId != nullptr) {
      for (const auto &buttonNode : config["buttons"]) {
        ButtonControl button;
        button.id = buttonNode["id"].as<int>();
        button.title = buttonNode["title"].as<std::string>();
        button.buttonLabel = buttonNode["buttonLabel"] ? buttonNode["buttonLabel"].as<std::string>() : "Click me!";
        button.isOpen = true;

        if (buttonNode["posX"]) {
          button.posX = buttonNode["posX"].as<float>();
          button.posY = buttonNode["posY"].as<float>();
        }
        if (buttonNode["sizeX"]) {
          button.sizeX = buttonNode["sizeX"].as<float>();
          button.sizeY = buttonNode["sizeY"].as<float>();
        }

        loadedButtons.push_back(button);
        if (button.id > maxButtonId) {
          maxButtonId = button.id;
        }
      }
    }

    // Load toggles (Tier 4, if present)
    std::vector<ToggleControl> loadedToggles;
    int maxToggleId = 0;
    if (config["toggles"] && toggles != nullptr && nextToggleId != nullptr) {
      for (const auto &toggleNode : config["toggles"]) {
        ToggleControl toggle;
        toggle.id = toggleNode["id"].as<int>();
        toggle.title = toggleNode["title"].as<std::string>();
        toggle.toggleLabel = toggleNode["toggleLabel"] ? toggleNode["toggleLabel"].as<std::string>() : "Enable";
        toggle.state = toggleNode["state"] ? toggleNode["state"].as<bool>() : false;
        toggle.isOpen = true;

        if (toggleNode["posX"]) {
          toggle.posX = toggleNode["posX"].as<float>();
          toggle.posY = toggleNode["posY"].as<float>();
        }
        if (toggleNode["sizeX"]) {
          toggle.sizeX = toggleNode["sizeX"].as<float>();
          toggle.sizeY = toggleNode["sizeY"].as<float>();
        }

        loadedToggles.push_back(toggle);
        if (toggle.id > maxToggleId) {
          maxToggleId = toggle.id;
        }
      }
    }

    // Load text inputs (Tier 4, if present)
    std::vector<TextInputControl> loadedTextInputs;
    int maxTextInputId = 0;
    if (config["textInputs"] && textInputs != nullptr && nextTextInputId != nullptr) {
      for (const auto &textInputNode : config["textInputs"]) {
        TextInputControl textInput;
        textInput.id = textInputNode["id"].as<int>();
        textInput.title = textInputNode["title"].as<std::string>();
        textInput.isOpen = true;

        if (textInputNode["textBuffer"]) {
          std::string bufferText = textInputNode["textBuffer"].as<std::string>();
          strncpy(textInput.textBuffer, bufferText.c_str(), sizeof(textInput.textBuffer) - 1);
          textInput.textBuffer[sizeof(textInput.textBuffer) - 1] = '\0';
        }

        if (textInputNode["posX"]) {
          textInput.posX = textInputNode["posX"].as<float>();
          textInput.posY = textInputNode["posY"].as<float>();
        }
        if (textInputNode["sizeX"]) {
          textInput.sizeX = textInputNode["sizeX"].as<float>();
          textInput.sizeY = textInputNode["sizeY"].as<float>();
        }

        loadedTextInputs.push_back(textInput);
        if (textInput.id > maxTextInputId) {
          maxTextInputId = textInput.id;
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

    // Assign control elements if pointers are provided
    if (buttons != nullptr && nextButtonId != nullptr) {
      *buttons = loadedButtons;
      *nextButtonId = maxButtonId + 1;
    }
    if (toggles != nullptr && nextToggleId != nullptr) {
      *toggles = loadedToggles;
      *nextToggleId = maxToggleId + 1;
    }
    if (textInputs != nullptr && nextTextInputId != nullptr) {
      *textInputs = loadedTextInputs;
      *nextTextInputId = maxTextInputId + 1;
    }

    printf("Layout loaded from: %s\n", filename.c_str());
    return true;
  } catch (const std::exception &e) {
    fprintf(stderr, "Error loading layout: %s\n", e.what());
    return false;
  }
}
