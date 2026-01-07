#pragma once

#include <string>
#include <vector>
#include "pffft.h"

// -------------------------------------------------------------------------
// PLOT WINDOW DATA STRUCTURES
// -------------------------------------------------------------------------
// These structures represent the state of different plot window types.
// They hold configuration and runtime state, but no rendering logic.

// Represents one Plot Panel (time-series line plot)
struct PlotWindow {
  int id;
  std::string title;
  std::vector<std::string> signalNames; // List of keys to look up in Registry
  bool paused = false;
  bool isOpen = true;

};

// Represents one Readout Box (single numeric value display)
struct ReadoutBox {
  int id;
  std::string title;
  std::string signalName; // Single signal to display (empty if none assigned)
  bool isOpen = true;

};

// Represents one X/Y Plot (scatter plot with history)
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

// Represents one Histogram (distribution visualization)
struct HistogramWindow {
  int id;
  std::string title;
  std::string signalName; // Single signal to display (empty if none assigned)
  bool isOpen = true;
  int numBins = 50; // Number of histogram bins

};

// Represents one FFT Plot (frequency domain analysis)
struct FFTWindow {
  int id;
  std::string title;
  std::string signalName; // Single signal to display (empty if none assigned)
  bool isOpen = true;
  int fftSize = 2048; // Number of samples for FFT (power of 2)
  bool useHanning = true; // Apply Hanning window to reduce spectral leakage
  bool logScale = true; // Display magnitude in dB scale

};

// Colormap types for spectrogram visualization
enum class Colormap {
  Viridis,
  Plasma,
  Magma,
  Inferno,
  ImPlotDefault
};

// Represents one Spectrogram Plot (time-frequency visualization)
struct SpectrogramWindow {
  int id;
  std::string title;
  std::string signalName; // Single signal to display (empty if none assigned)
  bool isOpen = true;
  int fftSize = 512; // Number of samples per FFT window (power of 2)
  int hopSize = 256; // Number of samples to advance between FFT windows (default 50% overlap)
  bool useHanning = true; // Apply Hanning window to reduce spectral leakage
  bool logScale = true; // Display magnitude in dB scale
  double timeWindow = 5.0; // Time duration to display (seconds)
  int maxFrequency = 0; // Maximum frequency to display (0 = auto, uses Nyquist/2)
  Colormap colormap = Colormap::Viridis; // Colormap selection
  bool useInterpolation = true; // Enable bilinear interpolation for smoother appearance



  // Cache for spectrogram computation (performance optimization)
  std::vector<double> cachedTimeBins;
  std::vector<double> cachedFreqBins;
  std::vector<double> cachedMagnitudeMatrix;
  std::vector<double> transposedMagnitudeMatrix; // Format: [freq][time] for ImPlot::PlotHeatmap
  size_t cachedDataSize = 0; // Size of data when cache was computed
  int cachedOffset = -1;     // Circular buffer offset when computed
  double cachedWindowStart = -1.0; // Offline window start when computed
  double cachedWindowWidth = -1.0; // Offline window width when computed
  int cachedFftSize = -1;
  int cachedHopSize = -1;
  bool cachedLogScale = false;
  bool cachedUseHanning = false;
  int cachedMaxFrequency = -1;
  double cachedFs = 0.0;
  double lastComputeTime = 0.0; // Time when last computed
  double updateThrottleSeconds = 0.1; // Minimum seconds between updates (10 FPS max)

  // Worker buffers to avoid allocations in hot path
  std::vector<double> analyzerData;
  std::vector<double> analyzerTime;
  std::vector<float> magnitudesFloat;
  
  // Aligned buffers for pffft (managed manually to ensure SIMD alignment)
  float* pffft_work = nullptr;
  float* pffft_output = nullptr;
  float* pffft_input = nullptr;
  int pffft_buffer_size = 0;

  ~SpectrogramWindow() {
    if (pffft_work) pffft_aligned_free(pffft_work);
    if (pffft_output) pffft_aligned_free(pffft_output);
    if (pffft_input) pffft_aligned_free(pffft_input);
  }
};

// -------------------------------------------------------------------------
// CONTROL ELEMENT DATA STRUCTURES (Tier 4)
// -------------------------------------------------------------------------
// These structures represent interactive GUI control elements that expose
// their state to Lua frame callbacks.

// Represents one Button control element
struct ButtonControl {
  int id;
  std::string title;          // Window title (editable)
  std::string buttonLabel;    // Button text (editable)
  bool isOpen = true;
  bool clicked = false;       // State exposed to Lua - true for one frame after click
  bool wasClickedLastFrame = false; // Internal state to reset clicked flag

};

// Represents one Toggle control element
struct ToggleControl {
  int id;
  std::string title;          // Window title (editable)
  std::string toggleLabel;    // Toggle text (editable)
  bool isOpen = true;
  bool state = false;         // Toggle state exposed to Lua

};

// Represents one Text Input control element
struct TextInputControl {
  int id;
  std::string title;          // Window title (editable)
  bool isOpen = true;
  char textBuffer[256] = "";  // Input text exposed to Lua
  bool enterPressed = false;  // True for one frame after Enter is pressed
  bool wasEnterPressedLastFrame = false; // Internal state to reset enterPressed flag

};
