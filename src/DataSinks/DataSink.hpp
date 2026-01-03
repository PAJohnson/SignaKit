#pragma once

#include "../types.hpp"
#include "../SignalConfigLoader.h"
#include <map>
#include <vector>

// Forward declaration to avoid circular dependency
class LuaScriptManager;

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

    // Optional Lua script manager for custom parsers (Tier 2 feature)
    LuaScriptManager* luaManager;

public:
    DataSink(std::map<std::string, Signal>& registry, std::vector<PacketDefinition>& packetDefs, LuaScriptManager* luaMgr = nullptr)
        : signalRegistry(registry), packets(packetDefs), luaManager(luaMgr) {}

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