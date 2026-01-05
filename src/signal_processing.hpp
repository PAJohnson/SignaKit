#pragma once

#include "imgui.h"
#include "plot_types.hpp"
#include "pffft.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <memory>
#include <map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// -------------------------------------------------------------------------
// FFT AND SIGNAL PROCESSING (using pffft for SIMD optimization)
// -------------------------------------------------------------------------

// RAII wrapper for pffft setup to ensure proper cleanup
struct PFFTSetupDeleter {
  void operator()(PFFFT_Setup* setup) const {
    if (setup) {
      pffft_destroy_setup(setup);
    }
  }
};

using PFFTSetupPtr = std::unique_ptr<PFFFT_Setup, PFFTSetupDeleter>;

// Cache for pffft setups (expensive to create, should be reused)
// Key: FFT size, Value: setup pointer
inline std::map<int, PFFTSetupPtr>& GetPFFTSetupCache() {
  static std::map<int, PFFTSetupPtr> cache;
  return cache;
}

// Get or create cached pffft setup for given size
inline PFFFT_Setup* GetCachedPFFTSetup(int N) {
  auto& cache = GetPFFTSetupCache();
  auto it = cache.find(N);

  if (it != cache.end()) {
    return it->second.get();
  }

  // Create new setup and cache it
  PFFFT_Setup* setup = pffft_new_setup(N, PFFFT_REAL);
  if (setup) {
    cache[N] = PFFTSetupPtr(setup);
  }
  return setup;
}

// Calculate average sampling frequency from time points
inline double CalculateSamplingFrequency(const std::vector<double>& timePoints, int startIdx, int numSamples) {
  if (numSamples < 2 || startIdx + numSamples > (int)timePoints.size()) {
    return 1.0; // Default fallback
  }

  // Calculate average time delta
  double totalDelta = 0.0;
  int deltaCount = 0;

  for (int i = startIdx; i < startIdx + numSamples - 1; i++) {
    double delta = timePoints[i + 1] - timePoints[i];
    if (delta > 0.0) { // Only count positive deltas
      totalDelta += delta;
      deltaCount++;
    }
  }

  if (deltaCount > 0) {
    double avgDelta = totalDelta / deltaCount;
    return 1.0 / avgDelta; // Sampling frequency = 1 / average period
  }

  return 1.0; // Fallback
}

// Apply Hanning window to reduce spectral leakage (operates on float for pffft)
inline void ApplyHanningWindow(std::vector<float>& data) {
  int N = data.size();
  for (int i = 0; i < N; i++) {
    float window = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (N - 1)));
    data[i] *= window;
  }
}

// Optimized FFT using pffft library (SIMD-accelerated)
// Input: real-valued signal in 'input' (size N)
// Returns magnitude spectrum (first N/2 bins only, since input is real)
inline void ComputeRealFFT(const std::vector<float>& input, std::vector<float>& magnitudes) {
  int N = input.size();

  // pffft requires N >= 32 and power of 2
  if (N < 32 || (N & (N - 1)) != 0) {
    fprintf(stderr, "pffft FFT size must be >= 32 and power of 2, got %d\n", N);
    magnitudes.clear();
    return;
  }

  // Get cached pffft setup (expensive to create, so we cache and reuse)
  PFFFT_Setup* setup = GetCachedPFFTSetup(N);
  if (!setup) {
    fprintf(stderr, "Failed to create pffft setup for size %d\n", N);
    magnitudes.clear();
    return;
  }

  // Use pffft_aligned_malloc for proper SIMD alignment
  float* work = (float*)pffft_aligned_malloc(N * sizeof(float));
  float* output = (float*)pffft_aligned_malloc(N * sizeof(float));
  float* inputCopy = (float*)pffft_aligned_malloc(N * sizeof(float));

  // Copy input data
  memcpy(inputCopy, input.data(), N * sizeof(float));

  // Perform forward FFT
  // pffft_transform_ordered for PFFFT_REAL produces:
  // [DC, r1, r2, ..., r(N/2-1), Nyquist, i(N/2-1), ..., i2, i1]
  // where DC and Nyquist are purely real
  pffft_transform_ordered(setup, inputCopy, output, work, PFFFT_FORWARD);

  // Extract magnitude spectrum
  int numBins = N / 2;
  magnitudes.resize(numBins);

  // DC component (bin 0)
  magnitudes[0] = fabsf(output[0]);

  // Regular bins 1 to N/2-1
  // After testing, pffft real format appears to be:
  // [r0, r(N/2), r1, i1, r2, i2, ..., r(N/2-1), i(N/2-1)]
  // This is "interleaved" format after the first two elements
  for (int k = 1; k < numBins; k++) {
    float real = output[2 * k];
    float imag = output[2 * k + 1];
    magnitudes[k] = sqrtf(real * real + imag * imag);
  }

  // Free aligned memory
  pffft_aligned_free(work);
  pffft_aligned_free(output);
  pffft_aligned_free(inputCopy);
}

// Compute FFT magnitude spectrum from signal data using pffft
// Returns frequency bins and magnitude values
inline void ComputeFFTSpectrum(const std::vector<double>& signalData,
                       const std::vector<double>& timePoints,
                       int fftSize,
                       bool useHanning,
                       bool logScale,
                       std::vector<double>& freqBins,
                       std::vector<double>& magnitude) {

  if (signalData.size() < (size_t)fftSize) {
    // Not enough data
    freqBins.clear();
    magnitude.clear();
    return;
  }

  // pffft requires N >= 32 and power of 2
  if (fftSize < 32 || (fftSize & (fftSize - 1)) != 0) {
    freqBins.clear();
    magnitude.clear();
    return;
  }

  // Calculate sampling frequency
  double fs = CalculateSamplingFrequency(timePoints, 0, std::min(fftSize, (int)timePoints.size()));

  // Prepare data for FFT (use most recent samples) - convert to float for pffft
  int startIdx = signalData.size() - fftSize;
  std::vector<float> windowedData(fftSize);
  for (int i = 0; i < fftSize; i++) {
    windowedData[i] = static_cast<float>(signalData[startIdx + i]);
  }

  // Apply window function if requested
  if (useHanning) {
    ApplyHanningWindow(windowedData);
  }

  // Perform FFT using pffft
  std::vector<float> magnitudesFloat;
  ComputeRealFFT(windowedData, magnitudesFloat);

  // Extract magnitude spectrum (only first half, since FFT is symmetric for real signals)
  int numFreqBins = fftSize / 2;
  freqBins.resize(numFreqBins);
  magnitude.resize(numFreqBins);

  double freqResolution = fs / fftSize;

  for (int i = 0; i < numFreqBins; i++) {
    freqBins[i] = i * freqResolution;
    double mag = static_cast<double>(magnitudesFloat[i]);

    // Normalize by FFT size
    mag = mag / fftSize;

    // Convert to dB if requested
    if (logScale) {
      // Add small epsilon to avoid log(0)
      const double epsilon = 1e-10;
      magnitude[i] = 20.0 * log10(mag + epsilon);
    } else {
      magnitude[i] = mag;
    }
  }
}

// -------------------------------------------------------------------------
// COLORMAP FUNCTIONS (matplotlib-inspired)
// -------------------------------------------------------------------------

// Helper function to convert RGB to ImU32 color
inline ImU32 RGBToImU32(double r, double g, double b) {
  return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255);
}

// Matplotlib Viridis colormap
inline ImU32 GetViridisColor(double t) {
  t = std::max(0.0, std::min(1.0, t)); // clamp to [0,1]

  // Viridis color data (simplified - 8 control points)
  const double viridis_data[8][3] = {
    {0.267004, 0.004874, 0.329415}, // dark purple
    {0.282623, 0.140926, 0.457517}, // purple
    {0.253935, 0.265254, 0.529983}, // blue-purple
    {0.206756, 0.371758, 0.553117}, // blue
    {0.163625, 0.471133, 0.558148}, // blue-green
    {0.127568, 0.566949, 0.550556}, // cyan
    {0.267004, 0.670681, 0.464667}, // green
    {0.993248, 0.906157, 0.143936}  // yellow
  };

  int idx = (int)(t * 7);
  double frac = t * 7 - idx;
  if (idx >= 7) { idx = 6; frac = 1.0; }

  double r = viridis_data[idx][0] + frac * (viridis_data[idx+1][0] - viridis_data[idx][0]);
  double g = viridis_data[idx][1] + frac * (viridis_data[idx+1][1] - viridis_data[idx][1]);
  double b = viridis_data[idx][2] + frac * (viridis_data[idx+1][2] - viridis_data[idx][2]);

  return RGBToImU32(r, g, b);
}

// Matplotlib Plasma colormap
inline ImU32 GetPlasmaColor(double t) {
  t = std::max(0.0, std::min(1.0, t));

  const double plasma_data[8][3] = {
    {0.050383, 0.029803, 0.527975}, // dark blue
    {0.285282, 0.012916, 0.627158}, // purple-blue
    {0.471456, 0.001749, 0.657879}, // purple
    {0.617331, 0.055384, 0.621654}, // magenta
    {0.752299, 0.134462, 0.532027}, // red-magenta
    {0.862356, 0.237835, 0.402367}, // red
    {0.952978, 0.391973, 0.257267}, // orange
    {0.940015, 0.975158, 0.131326}  // yellow
  };

  int idx = (int)(t * 7);
  double frac = t * 7 - idx;
  if (idx >= 7) { idx = 6; frac = 1.0; }

  double r = plasma_data[idx][0] + frac * (plasma_data[idx+1][0] - plasma_data[idx][0]);
  double g = plasma_data[idx][1] + frac * (plasma_data[idx+1][1] - plasma_data[idx][1]);
  double b = plasma_data[idx][2] + frac * (plasma_data[idx+1][2] - plasma_data[idx][2]);

  return RGBToImU32(r, g, b);
}

// Matplotlib Magma colormap
inline ImU32 GetMagmaColor(double t) {
  t = std::max(0.0, std::min(1.0, t));

  const double magma_data[8][3] = {
    {0.001462, 0.000466, 0.013866}, // near black
    {0.103093, 0.050344, 0.298563}, // dark purple
    {0.258234, 0.089412, 0.456520}, // purple
    {0.427397, 0.123488, 0.563896}, // blue-purple
    {0.619043, 0.176991, 0.569720}, // magenta
    {0.822586, 0.304561, 0.487138}, // pink-red
    {0.968932, 0.534931, 0.383229}, // orange
    {0.987053, 0.991438, 0.749504}  // pale yellow
  };

  int idx = (int)(t * 7);
  double frac = t * 7 - idx;
  if (idx >= 7) { idx = 6; frac = 1.0; }

  double r = magma_data[idx][0] + frac * (magma_data[idx+1][0] - magma_data[idx][0]);
  double g = magma_data[idx][1] + frac * (magma_data[idx+1][1] - magma_data[idx][1]);
  double b = magma_data[idx][2] + frac * (magma_data[idx+1][2] - magma_data[idx][2]);

  return RGBToImU32(r, g, b);
}

// Matplotlib Inferno colormap
inline ImU32 GetInfernoColor(double t) {
  t = std::max(0.0, std::min(1.0, t));

  const double inferno_data[8][3] = {
    {0.001462, 0.000466, 0.013866}, // near black
    {0.087411, 0.044556, 0.224813}, // dark purple
    {0.258234, 0.038571, 0.406485}, // purple
    {0.451465, 0.042786, 0.463605}, // blue-purple
    {0.659135, 0.105015, 0.408614}, // magenta-red
    {0.847074, 0.246514, 0.249368}, // red
    {0.965225, 0.495257, 0.120458}, // orange
    {0.988362, 0.998364, 0.644924}  // pale yellow
  };

  int idx = (int)(t * 7);
  double frac = t * 7 - idx;
  if (idx >= 7) { idx = 6; frac = 1.0; }

  double r = inferno_data[idx][0] + frac * (inferno_data[idx+1][0] - inferno_data[idx][0]);
  double g = inferno_data[idx][1] + frac * (inferno_data[idx+1][1] - inferno_data[idx][1]);
  double b = inferno_data[idx][2] + frac * (inferno_data[idx+1][2] - inferno_data[idx][2]);

  return RGBToImU32(r, g, b);
}

// Get color from selected colormap
inline ImU32 GetColormapColor(Colormap colormap, double t) {
  switch (colormap) {
    case Colormap::Viridis: return GetViridisColor(t);
    case Colormap::Plasma: return GetPlasmaColor(t);
    case Colormap::Magma: return GetMagmaColor(t);
    case Colormap::Inferno: return GetInfernoColor(t);
    case Colormap::ImPlotDefault:
    default:
      // Fallback to simple gradient if ImPlot default is selected
      return IM_COL32((int)(t * 255), 0, (int)((1.0 - t) * 255), 255);
  }
}

// Compute spectrogram from signal data using Short-Time Fourier Transform (STFT)
// Returns time bins, frequency bins, and 2D magnitude matrix (row-major: [time][freq])
inline void ComputeSpectrogram(const std::vector<double>& signalData,
                       const std::vector<double>& timePoints,
                       int fftSize,
                       int hopSize,
                       bool useHanning,
                       bool logScale,
                       double timeWindowDuration,
                       int maxFrequency,
                       std::vector<double>& timeBins,
                       std::vector<double>& freqBins,
                       std::vector<double>& magnitudeMatrix) {

  if (signalData.size() < (size_t)fftSize) {
    // Not enough data
    timeBins.clear();
    freqBins.clear();
    magnitudeMatrix.clear();
    return;
  }

  // Calculate sampling frequency
  double fs = CalculateSamplingFrequency(timePoints, 0, std::min(fftSize, (int)timePoints.size()));

  // Determine how many samples to analyze based on time window
  int numSamplesToAnalyze = signalData.size();
  int dataStartIdx = 0;

  if (timeWindowDuration > 0.0 && !timePoints.empty()) {
    // Find the time range
    double endTime = timePoints.back();
    double startTime = endTime - timeWindowDuration;

    // Find the start index for this time range
    dataStartIdx = 0;
    for (size_t i = 0; i < timePoints.size(); i++) {
      if (timePoints[i] >= startTime) {
        dataStartIdx = i;
        break;
      }
    }
    numSamplesToAnalyze = timePoints.size() - dataStartIdx;
  } else {
    // Use all data
    dataStartIdx = 0;
    numSamplesToAnalyze = signalData.size();
  }

  // Calculate number of FFT windows that fit in the data
  int numWindows = 0;
  if (numSamplesToAnalyze >= fftSize) {
    numWindows = (numSamplesToAnalyze - fftSize) / hopSize + 1;
  }

  //printf("numWindows %d, numSampleToAnalyze %d\n", numWindows, numSamplesToAnalyze);

  if (numWindows <= 0) {
    timeBins.clear();
    freqBins.clear();
    magnitudeMatrix.clear();
    return;
  }

  // Determine frequency bins
  int numFreqBins = fftSize / 2;
  double freqResolution = fs / fftSize;

  // Apply max frequency limit if specified
  int actualNumFreqBins = numFreqBins;
  if (maxFrequency > 0 && maxFrequency < fs / 2.0) {
    actualNumFreqBins = std::min(numFreqBins, (int)(maxFrequency / freqResolution));
  }

  // Ensure we have at least 1 frequency bin
  if (actualNumFreqBins <= 0) {
    actualNumFreqBins = 1;
  }

  // Initialize output arrays
  timeBins.resize(numWindows);
  freqBins.resize(actualNumFreqBins);
  magnitudeMatrix.resize(numWindows * actualNumFreqBins);

  // Fill frequency bins
  for (int i = 0; i < actualNumFreqBins; i++) {
    freqBins[i] = i * freqResolution;
  }

  // Process each FFT window using pffft
  for (int windowIdx = 0; windowIdx < numWindows; windowIdx++) {
    int startIdx = dataStartIdx + windowIdx * hopSize;

    // Get the center time of this window
    int centerIdx = startIdx + fftSize / 2;
    if (centerIdx < (int)timePoints.size()) {
      timeBins[windowIdx] = timePoints[centerIdx];
    } else {
      timeBins[windowIdx] = timePoints.back();
    }

    // Extract window data - convert to float for pffft
    std::vector<float> windowedData(fftSize);
    for (int i = 0; i < fftSize; i++) {
      if (startIdx + i < (int)signalData.size()) {
        windowedData[i] = static_cast<float>(signalData[startIdx + i]);
      } else {
        windowedData[i] = 0.0f; // Zero-pad if needed
      }
    }

    // Apply window function if requested
    if (useHanning) {
      ApplyHanningWindow(windowedData);
    }

    // Perform FFT using pffft
    std::vector<float> magnitudesFloat;
    ComputeRealFFT(windowedData, magnitudesFloat);

    // Extract magnitude spectrum and store in matrix
    for (int i = 0; i < actualNumFreqBins; i++) {
      double mag = static_cast<double>(magnitudesFloat[i]);

      // Normalize by FFT size
      mag = mag / fftSize;

      // Convert to dB if requested
      if (logScale) {
        const double epsilon = 1e-10;
        mag = 20.0 * log10(mag + epsilon);
      }

      // Store in row-major format: magnitudeMatrix[windowIdx * numFreqBins + freqIdx]
      magnitudeMatrix[windowIdx * actualNumFreqBins + i] = mag;
    }
  }
}
