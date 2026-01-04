#pragma once

#include "imgui.h"
#include "implot.h"
#include "ImGuiFileDialog.h"
#include "plot_types.hpp"
#include "ui_state.hpp"
#include "types.hpp"
#include "signal_processing.hpp"
#include "LuaScriptManager.hpp"
#include <map>
#include <string>
#include <atomic>
#include <mutex>

// Forward declarations for global state (defined in main.cpp)
extern std::atomic<bool> appRunning;
extern PlaybackMode currentPlaybackMode;
extern OfflinePlaybackState offlineState;
extern std::map<std::string, Signal> signalRegistry;
extern LuaScriptManager luaScriptManager;
extern std::vector<std::string> availableParsers;

// -------------------------------------------------------------------------
// PARSER SELECTION HELPERS
// -------------------------------------------------------------------------
void scanAvailableParsers(std::vector<std::string>& availableParsers) {
  availableParsers.clear();

  std::string parsersPath = "scripts/parsers";
  if (!std::filesystem::exists(parsersPath)) {
    printf("[ParserSelection] Parsers directory not found: %s\n", parsersPath.c_str());
    return;
  }

  for (const auto& entry : std::filesystem::directory_iterator(parsersPath)) {
    if (entry.is_regular_file() && entry.path().extension() == ".lua") {
      std::string parserName = entry.path().stem().string();
      availableParsers.push_back(parserName);
    }
  }

  // Sort alphabetically for consistent ordering
  std::sort(availableParsers.begin(), availableParsers.end());

  printf("[ParserSelection] Found %zu parsers\n", availableParsers.size());
}

// -------------------------------------------------------------------------
// WINDOW POSITION MANAGEMENT HELPERS
// -------------------------------------------------------------------------

// Helper function to set window position/size with clamping to screen bounds
// Also uses saved position/size if available
template<typename WindowType>
inline void SetupWindowPositionAndSize(WindowType& window,
                                        const ImVec2& defaultPos,
                                        const ImVec2& defaultSize) {
    ImGuiIO& io = ImGui::GetIO();
    float screenWidth = io.DisplaySize.x;
    float screenHeight = io.DisplaySize.y;

    // If window has saved position, use it (with clamping) on first appearance only
    if (window.posX >= 0.0f && window.posY >= 0.0f) {
        // Clamp position to screen bounds
        float clampedX = std::max(0.0f, std::min(window.posX, screenWidth - 100.0f));  // Keep at least 100px visible
        float clampedY = std::max(0.0f, std::min(window.posY, screenHeight - 50.0f)); // Keep at least 50px visible
        ImGui::SetNextWindowPos(ImVec2(clampedX, clampedY), ImGuiCond_Once);
    } else {
        ImGui::SetNextWindowPos(defaultPos, ImGuiCond_FirstUseEver);
    }

    // If window has saved size, use it (with minimum size enforcement) on first appearance only
    if (window.sizeX >= 0.0f && window.sizeY >= 0.0f) {
        // Enforce minimum sizes and clamp to screen
        float minWidth = 200.0f;
        float minHeight = 150.0f;
        float clampedWidth = std::max(minWidth, std::min(window.sizeX, screenWidth));
        float clampedHeight = std::max(minHeight, std::min(window.sizeY, screenHeight));
        ImGui::SetNextWindowSize(ImVec2(clampedWidth, clampedHeight), ImGuiCond_Once);
    } else {
        ImGui::SetNextWindowSize(defaultSize, ImGuiCond_FirstUseEver);
    }
}

// Helper function to capture current window position and size after rendering
template<typename WindowType>
inline void CaptureWindowPositionAndSize(WindowType& window) {
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    window.posX = pos.x;
    window.posY = pos.y;
    window.sizeX = size.x;
    window.sizeY = size.y;
}

// -------------------------------------------------------------------------
// MENU BAR RENDERING
// -------------------------------------------------------------------------

inline void RenderMenuBar(UIPlotState& uiPlotState) {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Menu")) {
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
      ImGui::MenuItem("Edit Mode", nullptr, &uiPlotState.editMode);
      ImGui::Separator();
      if (ImGui::MenuItem("Close")) {
        appRunning = false;
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scripts")) {
      if (ImGui::MenuItem("Reload All Scripts")) {
        luaScriptManager.reloadAllScripts();
        scanAvailableParsers(availableParsers);  // Rescan parsers after reload
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

    // Parser Selection
    ImGui::Text("Parser:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    {
      std::lock_guard<std::mutex> lock(parserSelectionMutex);
      if (ImGui::BeginCombo("##ParserSelect", selectedParser.c_str())) {
        for (const auto& parser : availableParsers) {
          bool isSelected = (selectedParser == parser);
          if (ImGui::Selectable(parser.c_str(), isSelected)) {
            selectedParser = parser;
            printf("[ParserSelection] Selected parser: %s\n", selectedParser.c_str());
          }
          if (isSelected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
    }

    // Note: Online/Offline mode and all data source controls are now managed in Lua
    // via DataSource.lua - Users create a Toggle control titled "Online" to switch between modes

    ImGui::EndMainMenuBar();
  }
}

// -------------------------------------------------------------------------
// SIGNAL BROWSER RENDERING (Left Panel)
// -------------------------------------------------------------------------

inline void RenderSignalBrowser(UIPlotState& uiPlotState, float menuBarHeight) {
  ImGuiIO &io = ImGui::GetIO();

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
      newPlot.id = uiPlotState.nextPlotId++;
      newPlot.title = "Plot " + std::to_string(newPlot.id);
      uiPlotState.activePlots.push_back(newPlot);
    }
    if (ImGui::MenuItem("X/Y Plot")) {
      XYPlotWindow newXYPlot;
      newXYPlot.id = uiPlotState.nextXYPlotId++;
      newXYPlot.title = "X/Y Plot " + std::to_string(newXYPlot.id);
      newXYPlot.xSignalName = "";
      newXYPlot.ySignalName = "";
      uiPlotState.activeXYPlots.push_back(newXYPlot);
    }
    if (ImGui::MenuItem("Readout Box")) {
      ReadoutBox newReadout;
      newReadout.id = uiPlotState.nextReadoutBoxId++;
      newReadout.title = "Readout " + std::to_string(newReadout.id);
      newReadout.signalName = ""; // Empty initially
      uiPlotState.activeReadoutBoxes.push_back(newReadout);
    }
    if (ImGui::MenuItem("Histogram")) {
      HistogramWindow newHistogram;
      newHistogram.id = uiPlotState.nextHistogramId++;
      newHistogram.title = "Histogram " + std::to_string(newHistogram.id);
      newHistogram.signalName = ""; // Empty initially
      uiPlotState.activeHistograms.push_back(newHistogram);
    }
    if (ImGui::MenuItem("FFT Plot")) {
      FFTWindow newFFT;
      newFFT.id = uiPlotState.nextFFTId++;
      newFFT.title = "FFT " + std::to_string(newFFT.id);
      newFFT.signalName = ""; // Empty initially
      uiPlotState.activeFFTs.push_back(newFFT);
    }
    if (ImGui::MenuItem("Spectrogram")) {
      SpectrogramWindow newSpectrogram;
      newSpectrogram.id = uiPlotState.nextSpectrogramId++;
      newSpectrogram.title = "Spectrogram " + std::to_string(newSpectrogram.id);
      newSpectrogram.signalName = ""; // Empty initially
      uiPlotState.activeSpectrograms.push_back(newSpectrogram);
    }
    ImGui::Separator();
    // Tier 4: Control Elements
    if (ImGui::MenuItem("Button")) {
      ButtonControl newButton;
      newButton.id = uiPlotState.nextButtonId++;
      newButton.title = "Button " + std::to_string(newButton.id);
      newButton.buttonLabel = "Click me!";
      uiPlotState.activeButtons.push_back(newButton);
    }
    if (ImGui::MenuItem("Toggle")) {
      ToggleControl newToggle;
      newToggle.id = uiPlotState.nextToggleId++;
      newToggle.title = "Toggle " + std::to_string(newToggle.id);
      newToggle.toggleLabel = "Enable";
      uiPlotState.activeToggles.push_back(newToggle);
    }
    if (ImGui::MenuItem("Text Input")) {
      TextInputControl newTextInput;
      newTextInput.id = uiPlotState.nextTextInputId++;
      newTextInput.title = "Text Input " + std::to_string(newTextInput.id);
      uiPlotState.activeTextInputs.push_back(newTextInput);
    }
    ImGui::EndPopup();
  }
  ImGui::Separator();

  // Iterate over registry to show draggable items
  ImGui::BeginChild("SignalList");
  for (auto &[key, signal] : signalRegistry) {

    // Display the item
    ImGui::Selectable(key.c_str());

    // START DRAG SOURCE
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
}

// -------------------------------------------------------------------------
// TIME-BASED PLOT RENDERING
// -------------------------------------------------------------------------

inline void RenderTimePlots(UIPlotState& uiPlotState, float menuBarHeight) {
  // Loop through all active plot windows
  for (auto &plot : uiPlotState.activePlots) {
    if (!plot.isOpen)
      continue;

    // Set window position and size (with screen clamping)
    SetupWindowPositionAndSize(plot, ImVec2(350, menuBarHeight + 20), ImVec2(800, 600));

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

    // Capture current window position and size
    CaptureWindowPositionAndSize(plot);

    ImGui::End();
  }
}

// -------------------------------------------------------------------------
// READOUT BOX RENDERING
// -------------------------------------------------------------------------

inline void RenderReadoutBoxes(UIPlotState& uiPlotState, float menuBarHeight) {
  ImGuiIO &io = ImGui::GetIO();

  // Loop through all active readout boxes
  for (auto &readout : uiPlotState.activeReadoutBoxes) {
    if (!readout.isOpen)
      continue;

    // Set window position and size (with screen clamping)
    SetupWindowPositionAndSize(readout, ImVec2(350, menuBarHeight + 20), ImVec2(300, 150));

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

    // Capture current window position and size
    CaptureWindowPositionAndSize(readout);

    ImGui::End();
  }
}

// -------------------------------------------------------------------------
// X/Y PLOT RENDERING
// -------------------------------------------------------------------------

inline void RenderXYPlots(UIPlotState& uiPlotState, float menuBarHeight) {
  // Loop through all active X/Y plots
  for (auto &xyPlot : uiPlotState.activeXYPlots) {
    if (!xyPlot.isOpen)
      continue;

    // Set window position and size (with screen clamping)
    SetupWindowPositionAndSize(xyPlot, ImVec2(350, menuBarHeight + 20), ImVec2(800, 600));

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

    // Capture current window position and size
    CaptureWindowPositionAndSize(xyPlot);

    ImGui::End();
  }
}

// -------------------------------------------------------------------------
// HISTOGRAM RENDERING
// -------------------------------------------------------------------------

inline void RenderHistograms(UIPlotState& uiPlotState, float menuBarHeight) {
  // Loop through all active histograms
  for (auto &histogram : uiPlotState.activeHistograms) {
    if (!histogram.isOpen)
      continue;

    // Set window position and size (with screen clamping)
    SetupWindowPositionAndSize(histogram, ImVec2(350, menuBarHeight + 20), ImVec2(600, 400));

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

    // Capture current window position and size
    CaptureWindowPositionAndSize(histogram);

    ImGui::End();
  }
}

// -------------------------------------------------------------------------
// FFT PLOT RENDERING
// -------------------------------------------------------------------------

inline void RenderFFTPlots(UIPlotState& uiPlotState, float menuBarHeight) {
  // Loop through all active FFT plots
  for (auto &fft : uiPlotState.activeFFTs) {
    if (!fft.isOpen)
      continue;

    // Set window position and size (with screen clamping)
    SetupWindowPositionAndSize(fft, ImVec2(350, menuBarHeight + 20), ImVec2(800, 600));

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

    // Capture current window position and size
    CaptureWindowPositionAndSize(fft);

    ImGui::End();
  }
}

// -------------------------------------------------------------------------
// SPECTROGRAM RENDERING
// -------------------------------------------------------------------------

inline void RenderSpectrograms(UIPlotState& uiPlotState, float menuBarHeight) {
  // Loop through all active spectrograms
  for (auto &spectrogram : uiPlotState.activeSpectrograms) {
    if (!spectrogram.isOpen)
      continue;

    // Set window position and size (with screen clamping)
    SetupWindowPositionAndSize(spectrogram, ImVec2(350, menuBarHeight + 20), ImVec2(900, 600));

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

    // Capture current window position and size
    CaptureWindowPositionAndSize(spectrogram);

    ImGui::End();
  }
}

// -------------------------------------------------------------------------
// TIME SLIDER RENDERING (Offline Mode Only)
// -------------------------------------------------------------------------

inline void RenderTimeSlider() {
  if (currentPlaybackMode == PlaybackMode::OFFLINE && offlineState.fileLoaded) {
    ImGuiIO &io = ImGui::GetIO();

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
}

// -------------------------------------------------------------------------
// FILE DIALOG RENDERING
// -------------------------------------------------------------------------

inline void RenderFileDialogs(UIPlotState& uiPlotState) {
  // Display Save Layout dialog
  if (ImGuiFileDialog::Instance()->Display("SaveLayoutDlg", ImGuiWindowFlags_None, ImVec2(800, 600))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      SaveLayout(filePathName, uiPlotState.activePlots, uiPlotState.activeReadoutBoxes, uiPlotState.activeXYPlots, uiPlotState.activeHistograms, uiPlotState.activeFFTs, uiPlotState.activeSpectrograms, uiPlotState.activeButtons, uiPlotState.activeToggles, uiPlotState.activeTextInputs, uiPlotState.editMode);
    }
    ImGuiFileDialog::Instance()->Close();
  }

  // Display Open Layout dialog
  if (ImGuiFileDialog::Instance()->Display("OpenLayoutDlg", ImGuiWindowFlags_None, ImVec2(800, 600))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      if (LoadLayout(filePathName, uiPlotState.activePlots, uiPlotState.nextPlotId, uiPlotState.activeReadoutBoxes, uiPlotState.nextReadoutBoxId, uiPlotState.activeXYPlots, uiPlotState.nextXYPlotId, uiPlotState.activeHistograms, uiPlotState.nextHistogramId, uiPlotState.activeFFTs, uiPlotState.nextFFTId, uiPlotState.activeSpectrograms, uiPlotState.nextSpectrogramId, &uiPlotState.activeButtons, &uiPlotState.nextButtonId, &uiPlotState.activeToggles, &uiPlotState.nextToggleId, &uiPlotState.activeTextInputs, &uiPlotState.nextTextInputId, &uiPlotState.editMode)) {
        // Layout loaded successfully
      }
    }
    ImGuiFileDialog::Instance()->Close();
  }

  // Display Open Log File dialog
  if (ImGuiFileDialog::Instance()->Display("OpenLogFileDlg", ImGuiWindowFlags_None, ImVec2(800, 600))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      if (LoadLogFile(filePathName, signalRegistry, offlineState, luaScriptManager)) {
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
}
