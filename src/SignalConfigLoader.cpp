#include "SignalConfigLoader.h"
#include <iostream>

bool SignalConfigLoader::Load(const std::string &filename,
                              std::vector<PacketDefinition> &packets,
                              std::vector<SignalDefinition> &signals) {
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

        if (pktNode["fields"]) {
          for (const auto &fieldNode : pktNode["fields"]) {
            PacketDefinition::Field field;
            field.name = fieldNode["name"].as<std::string>();
            field.type = fieldNode["type"].as<std::string>();
            field.offset = fieldNode["offset"].as<int>();
            pkt.fields.push_back(field);

            // Auto-generate signal if this is not the time field
            if (field.name != timeFieldName) {
              SignalDefinition sig;
              sig.key = pkt.id + "." + field.name;
              sig.packetId = pkt.id;
              sig.valueField = field.name;
              sig.timeField = timeFieldName;
              signals.push_back(sig);
            }
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
