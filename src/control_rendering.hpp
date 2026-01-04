#pragma once

#include "imgui.h"
#include "plot_types.hpp"
#include "ui_state.hpp"
#include <string>
#include <vector>

// Forward declaration for window positioning helper
template<typename WindowType>
inline void SetupWindowPositionAndSize(WindowType& window,
                                        const ImVec2& defaultPos,
                                        const ImVec2& defaultSize);

template<typename WindowType>
inline void CaptureWindowPositionAndSize(WindowType& window);

// -------------------------------------------------------------------------
// EDITABLE TEXT RENDERING HELPER
// -------------------------------------------------------------------------
// Renders text that can be edited on double-click or right-click
inline bool RenderEditableText(const char* id, std::string& text, float fontSize = 1.0f) {
  static std::string editingId = "";
  static char editBuffer[256] = "";
  bool textChanged = false;

  std::string uniqueId = std::string(id);

  if (editingId == uniqueId) {
    // Currently editing this text
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##edit", editBuffer, sizeof(editBuffer),
                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
      // Enter pressed - commit the change
      text = std::string(editBuffer);
      editingId = "";
      textChanged = true;
    }
    // Exit edit mode if clicked outside or ESC pressed
    if (!ImGui::IsItemActive() && (ImGui::IsMouseClicked(0) || ImGui::IsKeyPressed(ImGuiKey_Escape))) {
      editingId = "";
    }
  } else {
    // Display mode
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
    if (fontSize != 1.0f) {
      ImGui::SetWindowFontScale(fontSize);
    }

    bool selected = false;
    ImGui::Selectable(text.c_str(), &selected, ImGuiSelectableFlags_None, ImVec2(0, 0));

    if (fontSize != 1.0f) {
      ImGui::SetWindowFontScale(1.0f);
    }
    ImGui::PopStyleVar();

    // Enter edit mode on double-click or right-click
    if (ImGui::IsItemHovered() && (ImGui::IsMouseDoubleClicked(0) || ImGui::IsMouseClicked(1))) {
      editingId = uniqueId;
      strncpy(editBuffer, text.c_str(), sizeof(editBuffer) - 1);
      editBuffer[sizeof(editBuffer) - 1] = '\0';
    }
  }

  return textChanged;
}

// -------------------------------------------------------------------------
// BUTTON CONTROL RENDERING
// -------------------------------------------------------------------------
inline void RenderButtonControls(UIPlotState& uiPlotState, float menuBarHeight) {
  // Loop through all active button controls
  for (auto &button : uiPlotState.activeButtons) {
    if (!button.isOpen)
      continue;

    // Reset clicked state from previous frame
    if (button.wasClickedLastFrame) {
      button.clicked = false;
      button.wasClickedLastFrame = false;
    }

    // Set window position and size (with screen clamping)
    SetupWindowPositionAndSize(button, ImVec2(350, menuBarHeight + 20), ImVec2(300, 150));

    // Use a stable ID (based on button.id) while displaying dynamic title
    std::string windowID = button.title + "##Button" + std::to_string(button.id);
    ImGui::Begin(windowID.c_str(), &button.isOpen);

    // Editable title and label (only shown in Edit Mode)
    if (uiPlotState.editMode) {
      ImGui::Text("Title:");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-1);
      char titleBuffer[256];
      strncpy(titleBuffer, button.title.c_str(), sizeof(titleBuffer) - 1);
      titleBuffer[sizeof(titleBuffer) - 1] = '\0';
      if (ImGui::InputText(("##ButtonTitle" + std::to_string(button.id)).c_str(), titleBuffer, sizeof(titleBuffer))) {
        button.title = std::string(titleBuffer);
      }

      ImGui::Separator();

      ImGui::Text("Label:");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-1);
      char labelBuffer[256];
      strncpy(labelBuffer, button.buttonLabel.c_str(), sizeof(labelBuffer) - 1);
      labelBuffer[sizeof(labelBuffer) - 1] = '\0';
      if (ImGui::InputText(("##ButtonLabel" + std::to_string(button.id)).c_str(), labelBuffer, sizeof(labelBuffer))) {
        button.buttonLabel = std::string(labelBuffer);
      }

      ImGui::Spacing();
    }

    // Center the button vertically and horizontally
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 buttonSize = ImVec2(200, 40);

    // Calculate button position to center it
    float buttonPosX = (windowSize.x - buttonSize.x) * 0.5f;
    float buttonPosY = (windowSize.y - buttonSize.y) * 0.5f + 20;

    ImGui::SetCursorPosX(buttonPosX);
    ImGui::SetCursorPosY(buttonPosY);

    // Render the button
    if (ImGui::Button(button.buttonLabel.c_str(), buttonSize)) {
      button.clicked = true;
      button.wasClickedLastFrame = true;
    }

    // Capture current window position and size
    CaptureWindowPositionAndSize(button);

    ImGui::End();
  }
}

// -------------------------------------------------------------------------
// TOGGLE CONTROL RENDERING
// -------------------------------------------------------------------------
inline void RenderToggleControls(UIPlotState& uiPlotState, float menuBarHeight) {
  // Loop through all active toggle controls
  for (auto &toggle : uiPlotState.activeToggles) {
    if (!toggle.isOpen)
      continue;

    // Set window position and size (with screen clamping)
    SetupWindowPositionAndSize(toggle, ImVec2(350, menuBarHeight + 20), ImVec2(300, 140));

    // Use a stable ID (based on toggle.id) while displaying dynamic title
    std::string windowID = toggle.title + "##Toggle" + std::to_string(toggle.id);
    ImGui::Begin(windowID.c_str(), &toggle.isOpen);

    // Editable title and label (only shown in Edit Mode)
    if (uiPlotState.editMode) {
      ImGui::Text("Title:");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-1);
      char titleBuffer[256];
      strncpy(titleBuffer, toggle.title.c_str(), sizeof(titleBuffer) - 1);
      titleBuffer[sizeof(titleBuffer) - 1] = '\0';
      if (ImGui::InputText(("##ToggleTitle" + std::to_string(toggle.id)).c_str(), titleBuffer, sizeof(titleBuffer))) {
        toggle.title = std::string(titleBuffer);
      }

      ImGui::Separator();

      ImGui::Text("Label:");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-1);
      char labelBuffer[256];
      strncpy(labelBuffer, toggle.toggleLabel.c_str(), sizeof(labelBuffer) - 1);
      labelBuffer[sizeof(labelBuffer) - 1] = '\0';
      if (ImGui::InputText(("##ToggleLabel" + std::to_string(toggle.id)).c_str(), labelBuffer, sizeof(labelBuffer))) {
        toggle.toggleLabel = std::string(labelBuffer);
      }

      ImGui::Spacing();
    }

    // Center the toggle vertically and horizontally
    ImVec2 windowSize = ImGui::GetWindowSize();

    // Center the checkbox and label together
    std::string checkboxLabel = toggle.toggleLabel;
    ImVec2 textSize = ImGui::CalcTextSize(checkboxLabel.c_str());
    float totalWidth = textSize.x + 30; // Approximate checkbox width
    float togglePosX = (windowSize.x - totalWidth) * 0.5f;
    float togglePosY = (windowSize.y - 40) * 0.5f + 20;

    ImGui::SetCursorPosX(togglePosX);
    ImGui::SetCursorPosY(togglePosY);
    ImGui::Checkbox(checkboxLabel.c_str(), &toggle.state);

    // Capture current window position and size
    CaptureWindowPositionAndSize(toggle);

    ImGui::End();
  }
}

// -------------------------------------------------------------------------
// TEXT INPUT CONTROL RENDERING
// -------------------------------------------------------------------------
inline void RenderTextInputControls(UIPlotState& uiPlotState, float menuBarHeight) {
  // Loop through all active text input controls
  for (auto &textInput : uiPlotState.activeTextInputs) {
    if (!textInput.isOpen)
      continue;

    // Reset enterPressed state from previous frame
    if (textInput.wasEnterPressedLastFrame) {
      textInput.enterPressed = false;
      textInput.wasEnterPressedLastFrame = false;
    }

    // Set window position and size (with screen clamping)
    SetupWindowPositionAndSize(textInput, ImVec2(350, menuBarHeight + 20), ImVec2(350, 170));

    // Use a stable ID (based on textInput.id) while displaying dynamic title
    std::string windowID = textInput.title + "##TextInput" + std::to_string(textInput.id);
    ImGui::Begin(windowID.c_str(), &textInput.isOpen);

    // Editable title (only shown in Edit Mode)
    if (uiPlotState.editMode) {
      ImGui::Text("Title:");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-1);
      char titleBuffer[256];
      strncpy(titleBuffer, textInput.title.c_str(), sizeof(titleBuffer) - 1);
      titleBuffer[sizeof(titleBuffer) - 1] = '\0';
      if (ImGui::InputText(("##TextInputTitle" + std::to_string(textInput.id)).c_str(), titleBuffer, sizeof(titleBuffer))) {
        textInput.title = std::string(titleBuffer);
      }

      ImGui::Separator();
      ImGui::Spacing();
    }

    // Text input field
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText(("##TextInputField_" + std::to_string(textInput.id)).c_str(),
                         textInput.textBuffer,
                         sizeof(textInput.textBuffer),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      // Enter was pressed
      textInput.enterPressed = true;
      textInput.wasEnterPressedLastFrame = true;
    }

    // Show hint
    ImGui::TextDisabled("Press Enter to submit");

    // Capture current window position and size
    CaptureWindowPositionAndSize(textInput);

    ImGui::End();
  }
}
