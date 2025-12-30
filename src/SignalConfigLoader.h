#pragma once

#include <map>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

struct PacketDefinition {
  std::string id;
  std::string headerString; // e.g. "IMU"
  size_t sizeCheck;

  struct Field {
    std::string name;
    std::string type; // "float", "double", "int", etc.
    int offset;
  };
  std::vector<Field> fields;
};

struct SignalDefinition {
  std::string key;        // "IMU.AccelX"
  std::string packetId;   // "IMU"
  std::string valueField; // "accelX"
  std::string timeField;  // "time"
};

class SignalConfigLoader {
public:
  static bool Load(const std::string &filename,
                   std::vector<PacketDefinition> &packets,
                   std::vector<SignalDefinition> &signals);
};
