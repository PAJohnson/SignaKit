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
extern std::atomic<bool> networkConnected;
extern std::atomic<bool> networkShouldConnect;
extern std::mutex networkConfigMutex;
extern std::string networkIP;
extern int networkPort;
extern PlaybackMode currentPlaybackMode;
extern OfflinePlaybackState offlineState;
extern std::map<std::string, Signal> signalRegistry;
extern LuaScriptManager luaScriptManager;
extern std::ofstream logFile;
extern std::mutex logFileMutex;

// -------------------------------------------------------------------------
// MENU BAR RENDERING
// -------------------------------------------------------------------------

inline void RenderMenuBar(UIPlotState& uiPlotState) {
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
}

// -------------------------------------------------------------------------
// SIGNAL BROWSER RENDERING (Left Panel)
// -------------------------------------------------------------------------

inline void RenderSignalBrowser() {
  ImGui::SetNextWindowPos(ImVec2(0, 20), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(250, 400), ImGuiCond_FirstUseEver);

  if (ImGui::Begin("Signals")) {
    ImGui::Text("Available Signals:");
    ImGui::Separator();

    for (const auto &entry : signalRegistry) {
      const std::string &key = entry.first;

      // Make each signal a drag source
      ImGui::Selectable(key.c_str());
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        // Set payload to carry the signal key
        ImGui::SetDragDropPayload("SIGNAL_KEY", key.c_str(), key.size() + 1);
        ImGui::Text("%s", key.c_str());
        ImGui::EndDragDropSource();
      }
    }
  }
  ImGui::End();
}

// -------------------------------------------------------------------------
// PLACEHOLDER FOR ADDITIONAL RENDERING FUNCTIONS
// -------------------------------------------------------------------------
// Additional rendering functions for plots, readouts, etc. will be added here
// This header file will be extended in subsequent refactoring steps
