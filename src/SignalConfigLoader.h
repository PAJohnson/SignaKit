#pragma once

#include <map>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

// Represents a single signal in a packet (e.g., "IMU.accelX")
struct SignalDefinition {
  std::string key;          // "IMU.accelX"
  std::string name;         // "accelX"
  std::string type;         // "float", "double", etc.
  int offset;               // Byte offset in packet for this signal's value
  int timeOffset;           // Byte offset in packet for the time field
  std::string timeType;     // Type of the time field (usually "double")
};

// Represents a complete packet type with all its signals
struct PacketDefinition {
  std::string id;           // "IMU"
  std::string headerString; // e.g. "IMU"
  size_t sizeCheck;         // Expected packet size in bytes
  std::vector<SignalDefinition> signals; // All signals in this packet
};

class SignalConfigLoader {
public:
  static bool Load(const std::string &filename,
                   std::vector<PacketDefinition> &packets);
};
