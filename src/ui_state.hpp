#pragma once

#include "plot_types.hpp"
#include <vector>

// -------------------------------------------------------------------------
// UI PLOT STATE MANAGEMENT
// -------------------------------------------------------------------------
// This structure consolidates all the plot window state that was previously
// scattered as individual global variables. It makes it easier to see all
// UI state in one place and simplifies passing state to rendering functions.

struct UIPlotState {
  // Time-series line plots
  std::vector<PlotWindow> activePlots;
  int nextPlotId = 1;

  // Readout boxes (single numeric value displays)
  std::vector<ReadoutBox> activeReadoutBoxes;
  int nextReadoutBoxId = 1;

  // X/Y scatter plots
  std::vector<XYPlotWindow> activeXYPlots;
  int nextXYPlotId = 1;

  // Histograms
  std::vector<HistogramWindow> activeHistograms;
  int nextHistogramId = 1;

  // FFT plots
  std::vector<FFTWindow> activeFFTs;
  int nextFFTId = 1;

  // Spectrograms
  std::vector<SpectrogramWindow> activeSpectrograms;
  int nextSpectrogramId = 1;

  // Control elements (Tier 4)
  std::vector<ButtonControl> activeButtons;
  int nextButtonId = 1;

  std::vector<ToggleControl> activeToggles;
  int nextToggleId = 1;

  std::vector<TextInputControl> activeTextInputs;
  int nextTextInputId = 1;

  // UI Settings
  bool editMode = true; // When true, show Title/Label editing fields in control elements
  bool showMemoryProfiler = false; // Toggle for diagnostic window
};
