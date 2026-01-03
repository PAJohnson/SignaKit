#pragma once
#include <string>
#include <vector>

// Playback mode enum
enum class PlaybackMode {
  ONLINE,   // Real-time from network
  OFFLINE   // File playback
};

// A single signal (e.g., "IMU.AccelX") holding its own history
struct Signal {
  std::string name;
  int offset;
  std::vector<double> dataX; // Time
  std::vector<double> dataY; // Value
  int maxSize;
  PlaybackMode mode;

  Signal(std::string n = "", int size = 2000, PlaybackMode m = PlaybackMode::ONLINE)
      : name(n), maxSize(size), offset(0), mode(m) {
    if (mode == PlaybackMode::ONLINE) {
      dataX.reserve(maxSize);
      dataY.reserve(maxSize);
    }
  }

  void AddPoint(double x, double y) {
    if (mode == PlaybackMode::ONLINE) {
      // Online mode: circular buffer with fixed size
      if (dataX.size() < maxSize) {
        dataX.push_back(x);
        dataY.push_back(y);
      } else {
        dataX[offset] = x;
        dataY[offset] = y;
        offset = (offset + 1) % maxSize;
      }
    } else {
      // Offline mode: grow indefinitely
      dataX.push_back(x);
      dataY.push_back(y);
    }
  }

  void Clear() {
    dataX.clear();
    dataY.clear();
    offset = 0;
  }

  void SetMode(PlaybackMode m) {
    mode = m;
    if (mode == PlaybackMode::ONLINE && dataX.capacity() < maxSize) {
      dataX.reserve(maxSize);
      dataY.reserve(maxSize);
    }
  }
};