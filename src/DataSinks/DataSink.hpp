#pragma once

#include "../types.hpp"
#include "../SignalConfigLoader.h"
#include <map>
#include <vector>

// This is a base class for a way to connect to a data source
// This can be UDP packets, serial, CAN, whatever
// By inheriting from this class and implementing its methods, the parsing
// logic can be kept out of the GUI

class DataSink
{
protected:
    // packets constitute a packet definition - this is a length and a header
    std::map<std::string, Signal>& signalRegistry;
    std::vector<PacketDefinition>& packets;

public:
    DataSink(std::map<std::string, Signal>& registry, std::vector<PacketDefinition>& packetDefs)
        : signalRegistry(registry), packets(packetDefs) {}

    virtual ~DataSink() = default;

    // Open the connection/data source
    virtual bool open() = 0;

    // Process one iteration (receive data, parse, update signals)
    // Returns true if data was processed, false otherwise
    virtual bool step() = 0;

    // Close the connection/data source
    virtual void close() = 0;

    // Check if the data sink is currently connected/open
    virtual bool isOpen() const = 0;
};