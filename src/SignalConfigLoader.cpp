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
