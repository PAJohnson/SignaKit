#pragma once

#include "plot_types.hpp"
#include "types.hpp"
#include "LuaScriptManager.hpp"
// Note: Offline playback is now handled by Lua (scripts/io/DataSource.lua)
// The LoadLogFile function below is deprecated and kept for compatibility only
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "imgui.h"

// Forward declarations (these are defined in main.cpp)
struct OfflinePlaybackState;

// Data structure to hold loaded layout before applying to global state
struct LayoutData {
    std::vector<PlotWindow> plots;
    int nextPlotId = 1;
    std::vector<ReadoutBox> readouts;
    int nextReadoutId = 1;
    std::vector<XYPlotWindow> xyPlots;
    int nextXYPlotId = 1;
    std::vector<HistogramWindow> histograms;
    int nextHistogramId = 1;
    std::vector<FFTWindow> ffts;
    int nextFFTId = 1;
    std::vector<SpectrogramWindow> spectrograms;
    int nextSpectrogramId = 1;
    std::vector<ButtonControl> buttons;
    int nextButtonId = 1;
    std::vector<ToggleControl> toggles;
    int nextToggleId = 1;
    std::vector<TextInputControl> textInputs;
    int nextTextInputId = 1;
    bool editMode = true;
    std::string imguiSettings;
};

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

    // Save ImGui Settings (Docking, Window Layout, etc.)
    size_t settingsSize = 0;
    const char* settingsData = ImGui::SaveIniSettingsToMemory(&settingsSize);
    if (settingsData && settingsSize > 0) {
        std::string settingsStr(settingsData, settingsSize);
        out << YAML::Key << "imgui_settings" << YAML::Value << YAML::Literal << settingsStr;
    }

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

    // Save buttons (Tier 4)
    out << YAML::Key << "buttons" << YAML::Value << YAML::BeginSeq;
    for (const auto &button : buttons) {
      out << YAML::BeginMap;
      out << YAML::Key << "id" << YAML::Value << button.id;
      out << YAML::Key << "title" << YAML::Value << button.title;
      out << YAML::Key << "buttonLabel" << YAML::Value << button.buttonLabel;
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

inline bool LoadLayout(const std::string &filename, LayoutData& data) {
  try {
    YAML::Node config = YAML::LoadFile(filename);
    if (!config["plots"]) {
      fprintf(stderr, "Invalid layout file: missing 'plots' key\n");
      return false;
    }

    // Load UI settings
    if (config["editMode"]) {
      data.editMode = config["editMode"].as<bool>();
    }

    // Load ImGui Settings if present
    bool hasImGuiSettings = false;
    if (config["imgui_settings"]) {
        data.imguiSettings = config["imgui_settings"].as<std::string>();
        hasImGuiSettings = true;
    }

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



      data.plots.push_back(plot);
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



        data.readouts.push_back(readout);
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



        data.xyPlots.push_back(xyPlot);
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



        data.histograms.push_back(histogram);
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



        data.ffts.push_back(fft);
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



        data.spectrograms.push_back(spectrogram);
        if (spectrogram.id > maxSpectrogramId) {
          maxSpectrogramId = spectrogram.id;
        }
      }
    }

    // Load buttons (Tier 4, if present)
    int maxButtonId = 0;
    if (config["buttons"]) {
      for (const auto &buttonNode : config["buttons"]) {
        ButtonControl button;
        button.id = buttonNode["id"].as<int>();
        button.title = buttonNode["title"].as<std::string>();
        button.buttonLabel = buttonNode["buttonLabel"] ? buttonNode["buttonLabel"].as<std::string>() : "Click me!";
        button.isOpen = true;



        data.buttons.push_back(button);
        if (button.id > maxButtonId) {
          maxButtonId = button.id;
        }
      }
    }

    // Load toggles (Tier 4, if present)
    int maxToggleId = 0;
    if (config["toggles"]) {
      for (const auto &toggleNode : config["toggles"]) {
        ToggleControl toggle;
        toggle.id = toggleNode["id"].as<int>();
        toggle.title = toggleNode["title"].as<std::string>();
        toggle.toggleLabel = toggleNode["toggleLabel"] ? toggleNode["toggleLabel"].as<std::string>() : "Enable";
        toggle.state = toggleNode["state"] ? toggleNode["state"].as<bool>() : false;
        toggle.isOpen = true;



        data.toggles.push_back(toggle);
        if (toggle.id > maxToggleId) {
          maxToggleId = toggle.id;
        }
      }
    }

    // Load text inputs (Tier 4, if present)
    int maxTextInputId = 0;
    if (config["textInputs"]) {
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



        data.textInputs.push_back(textInput);
        if (textInput.id > maxTextInputId) {
          maxTextInputId = textInput.id;
        }
      }
    }

    data.nextPlotId = maxPlotId + 1;
    data.nextReadoutId = maxReadoutId + 1;
    data.nextXYPlotId = maxXYPlotId + 1;
    data.nextHistogramId = maxHistogramId + 1;
    data.nextFFTId = maxFFTId + 1;
    data.nextSpectrogramId = maxSpectrogramId + 1;
    data.nextButtonId = maxButtonId + 1;
    data.nextToggleId = maxToggleId + 1;
    data.nextTextInputId = maxTextInputId + 1;

    printf("Layout loaded from: %s\n", filename.c_str());
    return true;
  } catch (const std::exception &e) {
    fprintf(stderr, "Error loading layout: %s\n", e.what());
    return false;
  }
}
