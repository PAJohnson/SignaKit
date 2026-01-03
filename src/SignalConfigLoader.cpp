#include "SignalConfigLoader.h"
#include <iostream>

bool SignalConfigLoader::Load(const std::string &filename,
                              std::vector<PacketDefinition> &packets) {
  try {
    YAML::Node config = YAML::LoadFile(filename);

    if (config["packets"]) {
      for (const auto &pktNode : config["packets"]) {
        PacketDefinition pkt;
        pkt.id = pktNode["id"].as<std::string>();
        pkt.headerString = pktNode["header_string"].as<std::string>();
        pkt.sizeCheck = pktNode["size_check"].as<size_t>();

        std::string timeFieldName = "time";
        if (pktNode["time_field"]) {
          timeFieldName = pktNode["time_field"].as<std::string>();
        }

        // Find time field info first
        int timeOffset = -1;
        std::string timeType;

        if (pktNode["fields"]) {
          for (const auto &fieldNode : pktNode["fields"]) {
            std::string fieldName = fieldNode["name"].as<std::string>();
            if (fieldName == timeFieldName) {
              timeOffset = fieldNode["offset"].as<int>();
              timeType = fieldNode["type"].as<std::string>();
              break;
            }
          }
        }

        if (timeOffset < 0) {
          std::cerr << "Error: Packet '" << pkt.id
                    << "' has no time field named '" << timeFieldName << "'" << std::endl;
          return false;
        }

        // Now create signals for all non-time fields
        if (pktNode["fields"]) {
          for (const auto &fieldNode : pktNode["fields"]) {
            std::string fieldName = fieldNode["name"].as<std::string>();

            // Skip the time field - it's not a signal
            if (fieldName == timeFieldName) {
              continue;
            }

            SignalDefinition sig;
            sig.key = pkt.id + "." + fieldName;
            sig.name = fieldName;
            sig.type = fieldNode["type"].as<std::string>();
            sig.offset = fieldNode["offset"].as<int>();
            sig.timeOffset = timeOffset;
            sig.timeType = timeType;

            pkt.signals.push_back(sig);
          }
        }

        packets.push_back(pkt);
      }
    }

    return true;
  } catch (const YAML::Exception &e) {
    std::cerr << "Error loading config: " << e.what() << std::endl;
    return false;
  }
}

// Helper to read a value from the buffer based on type
// Supports all stdint.h types plus legacy C types for backwards compatibility
// All values are cast to double for storage in the signal buffers
double ReadValue(const char *buffer, const std::string &type, int offset) {
  // Floating point types
  if (type == "double") {
    double val;
    memcpy(&val, buffer + offset, sizeof(double));
    return val;
  } else if (type == "float") {
    float val;
    memcpy(&val, buffer + offset, sizeof(float));
    return (double)val;
  }
  // Signed integer types
  else if (type == "int8_t" || type == "int8") {
    int8_t val;
    memcpy(&val, buffer + offset, sizeof(int8_t));
    return (double)val;
  } else if (type == "int16_t" || type == "int16") {
    int16_t val;
    memcpy(&val, buffer + offset, sizeof(int16_t));
    return (double)val;
  } else if (type == "int32_t" || type == "int32" || type == "int") {
    int32_t val;
    memcpy(&val, buffer + offset, sizeof(int32_t));
    return (double)val;
  } else if (type == "int64_t" || type == "int64") {
    int64_t val;
    memcpy(&val, buffer + offset, sizeof(int64_t));
    return (double)val;
  }
  // Unsigned integer types
  else if (type == "uint8_t" || type == "uint8") {
    uint8_t val;
    memcpy(&val, buffer + offset, sizeof(uint8_t));
    return (double)val;
  } else if (type == "uint16_t" || type == "uint16") {
    uint16_t val;
    memcpy(&val, buffer + offset, sizeof(uint16_t));
    return (double)val;
  } else if (type == "uint32_t" || type == "uint32") {
    uint32_t val;
    memcpy(&val, buffer + offset, sizeof(uint32_t));
    return (double)val;
  } else if (type == "uint64_t" || type == "uint64") {
    uint64_t val;
    memcpy(&val, buffer + offset, sizeof(uint64_t));
    return (double)val;
  }
  // Standard C types (for backwards compatibility)
  else if (type == "char") {
    char val;
    memcpy(&val, buffer + offset, sizeof(char));
    return (double)val;
  } else if (type == "short") {
    short val;
    memcpy(&val, buffer + offset, sizeof(short));
    return (double)val;
  } else if (type == "long") {
    long val;
    memcpy(&val, buffer + offset, sizeof(long));
    return (double)val;
  } else if (type == "unsigned char") {
    unsigned char val;
    memcpy(&val, buffer + offset, sizeof(unsigned char));
    return (double)val;
  } else if (type == "unsigned short") {
    unsigned short val;
    memcpy(&val, buffer + offset, sizeof(unsigned short));
    return (double)val;
  } else if (type == "unsigned int") {
    unsigned int val;
    memcpy(&val, buffer + offset, sizeof(unsigned int));
    return (double)val;
  } else if (type == "unsigned long") {
    unsigned long val;
    memcpy(&val, buffer + offset, sizeof(unsigned long));
    return (double)val;
  }

  // Unknown type - return 0 and warn
  fprintf(stderr, "Warning: Unknown field type '%s'\n", type.c_str());
  return 0.0;
}
