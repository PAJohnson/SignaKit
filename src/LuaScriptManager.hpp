#pragma once

#include <sol/sol.hpp>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <cstring>
#include "types.hpp"

namespace fs = std::filesystem;

// Represents a loaded Lua script with metadata
struct LuaScript {
    std::string name;
    std::string filepath;
    bool enabled;
    bool hasError;
    std::string lastError;

    LuaScript(const std::string& n, const std::string& fp)
        : name(n), filepath(fp), enabled(true), hasError(false) {}
};

class LuaScriptManager {
public:
    LuaScriptManager() {
        initializeLuaState();
    }

    // Initialize Lua state and expose API
    void initializeLuaState() {
        lua = sol::state();
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                          sol::lib::table, sol::lib::io, sol::lib::os, sol::lib::package);

        // Expose the signal registry API
        exposeSignalAPI();

        // Expose packet callback registration API
        // on_packet(packetType, outputName, function) - registers a callback for a specific packet
        lua.set_function("on_packet", [this](const std::string& packetType, const std::string& outputName, sol::protected_function func) {
            registerPacketCallback(packetType, outputName, func);
        });

        // Expose packet parser registration API
        // register_parser(name, function) - registers a parser that receives (buffer, length) and returns true if handled
        lua.set_function("register_parser", [this](const std::string& parserName, sol::protected_function func) {
            registerPacketParser(parserName, func);
        });

        // Expose function to trigger Tier 1 packet callbacks from Tier 2 parsers
        // trigger_packet_callbacks(packetType) - triggers all on_packet() callbacks for a packet type
        lua.set_function("trigger_packet_callbacks", [this](const std::string& packetType) {
            if (currentSignalRegistry != nullptr) {
                executePacketCallbacks(packetType, *currentSignalRegistry);
            }
        });

        // Expose logging API
        lua.set_function("log", [](const std::string& message) {
            printf("[Lua] %s\n", message.c_str());
        });

        // Tier 3: Frame callback registration API
        // on_frame(function) - registers a callback to run every GUI frame
        lua.set_function("on_frame", [this](sol::protected_function func) {
            registerFrameCallback(func);
        });

        // Tier 3: Alert/condition monitoring API
        // on_alert(name, conditionFunction, actionFunction, [cooldownSeconds]) - monitors a condition
        lua.set_function("on_alert", [this](const std::string& alertName,
                                            sol::protected_function conditionFunc,
                                            sol::protected_function actionFunc,
                                            sol::optional<double> cooldownSeconds) {
            registerAlert(alertName, conditionFunc, actionFunc, cooldownSeconds.value_or(0.0));
        });

        // Tier 3: Get frame number and delta time
        lua.set_function("get_frame_number", [this]() -> uint64_t {
            return currentFrameNumber;
        });

        lua.set_function("get_delta_time", [this]() -> double {
            return currentDeltaTime;
        });

        // Tier 3: Get plot count (for monitoring GUI state)
        lua.set_function("get_plot_count", [this]() -> int {
            return currentPlotCount;
        });
    }

    // Load a single script from file
    bool loadScript(const std::string& filepath) {
        try {
            fs::path path(filepath);
            std::string name = path.filename().string();

            // Check if already loaded
            for (auto& script : scripts) {
                if (script.filepath == filepath) {
                    // Reload the script
                    auto result = lua.safe_script_file(filepath);
                    if (!result.valid()) {
                        sol::error err = result;
                        script.hasError = true;
                        script.lastError = err.what();
                        printf("[LuaScriptManager] Error reloading %s: %s\n", name.c_str(), err.what());
                        return false;
                    }
                    script.hasError = false;
                    script.lastError = "";
                    printf("[LuaScriptManager] Reloaded script: %s\n", name.c_str());
                    return true;
                }
            }

            // Load new script
            auto result = lua.safe_script_file(filepath);
            if (!result.valid()) {
                sol::error err = result;
                scripts.push_back(LuaScript(name, filepath));
                scripts.back().hasError = true;
                scripts.back().lastError = err.what();
                printf("[LuaScriptManager] Error loading %s: %s\n", name.c_str(), err.what());
                return false;
            }

            scripts.push_back(LuaScript(name, filepath));
            printf("[LuaScriptManager] Loaded script: %s\n", name.c_str());
            return true;

        } catch (const std::exception& e) {
            printf("[LuaScriptManager] Exception loading %s: %s\n", filepath.c_str(), e.what());
            return false;
        }
    }

    // Auto-load all scripts from a directory
    void loadScriptsFromDirectory(const std::string& dirPath) {
        if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
            printf("[LuaScriptManager] Scripts directory not found: %s\n", dirPath.c_str());
            return;
        }

        printf("[LuaScriptManager] Loading scripts from: %s\n", dirPath.c_str());
        // Use recursive_directory_iterator to load from subdirectories (e.g., scripts/parsers/)
        for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                loadScript(entry.path().string());
            }
        }
    }

    // Reload all loaded scripts
    void reloadAllScripts() {
        printf("[LuaScriptManager] Reloading all scripts...\n");

        // Clear all callbacks and parsers (they'll be re-registered by scripts)
        packetCallbacks.clear();
        packetParsers.clear();
        frameCallbacks.clear();
        alerts.clear();

        // Reinitialize Lua state
        initializeLuaState();

        // Reload each script
        std::vector<std::string> filepaths;
        for (const auto& script : scripts) {
            filepaths.push_back(script.filepath);
        }
        scripts.clear();

        for (const auto& filepath : filepaths) {
            loadScript(filepath);
        }
    }

    // Execute packet callbacks for a specific packet type
    // NOTE: Caller must hold stateMutex lock before calling this function
    // packetType: e.g., "IMU", "GPS", "BAT"
    void executePacketCallbacks(const std::string& packetType, std::map<std::string, Signal>& signalRegistry, PlaybackMode m = PlaybackMode::ONLINE) {
        // Set the registry so Lua functions can access it
        currentSignalRegistry = &signalRegistry;

        // Get current timestamp from the packet's signals
        double currentTime = getCurrentTimestamp(signalRegistry, packetType);

        // Find all callbacks registered for this packet type
        auto range = packetCallbacks.equal_range(packetType);
        for (auto it = range.first; it != range.second; ++it) {
            auto& [outputName, func] = it->second;

            try {
                auto result = func();
                if (!result.valid()) {
                    sol::error err = result;
                    printf("[LuaScriptManager] Packet callback error for '%s' on packet '%s': %s\n",
                           outputName.c_str(), packetType.c_str(), err.what());
                    continue;
                }

                // Check if the function returned a value to store
                if (result.return_count() > 0) {
                    sol::object retVal = result;
                    if (retVal.is<double>()) {
                        double value = retVal.as<double>();

                        // Get or create the output signal
                        if (signalRegistry.find(outputName) == signalRegistry.end()) {
                            signalRegistry[outputName] = Signal(outputName, 2000, m);
                        }

                        Signal& sig = signalRegistry[outputName];
                        sig.AddPoint(currentTime, value);
                    }
                }
            } catch (const std::exception& e) {
                printf("[LuaScriptManager] Exception in packet callback '%s' on packet '%s': %s\n",
                       outputName.c_str(), packetType.c_str(), e.what());
            }
        }

        // Clear the registry pointer
        currentSignalRegistry = nullptr;
    }

    // Tier 3: Execute frame callbacks every GUI render frame
    // NOTE: Caller must hold stateMutex lock before calling this function
    // frameNumber: current frame number
    // deltaTime: time since last frame in seconds
    // plotCount: number of active plots
    void executeFrameCallbacks(std::map<std::string, Signal>& signalRegistry,
                              uint64_t frameNumber,
                              double deltaTime,
                              int plotCount) {
        // Set frame context
        currentSignalRegistry = &signalRegistry;
        currentFrameNumber = frameNumber;
        currentDeltaTime = deltaTime;
        currentPlotCount = plotCount;

        // Execute all frame callbacks
        for (auto& func : frameCallbacks) {
            try {
                auto result = func();
                if (!result.valid()) {
                    sol::error err = result;
                    printf("[LuaScriptManager] Frame callback error: %s\n", err.what());
                }
            } catch (const std::exception& e) {
                printf("[LuaScriptManager] Exception in frame callback: %s\n", e.what());
            }
        }

        // Execute alert monitoring
        executeAlerts();

        // Clear context
        currentSignalRegistry = nullptr;
    }

    // Get list of loaded scripts
    const std::vector<LuaScript>& getScripts() const {
        return scripts;
    }

    // Enable/disable a script
    void setScriptEnabled(const std::string& name, bool enabled) {
        for (auto& script : scripts) {
            if (script.name == name) {
                script.enabled = enabled;
                break;
            }
        }
    }

    // Access to Lua state for advanced usage
    sol::state& getLuaState() {
        return lua;
    }

    // Parse a packet using registered Lua parsers
    // NOTE: Caller must hold stateMutex lock before calling this function
    // Returns true if at least one parser handled the packet
    // If selectedParser is not empty, only that parser will be used
    bool parsePacket(const char* buffer, size_t length, std::map<std::string, Signal>& signalRegistry, PlaybackMode mode = PlaybackMode::ONLINE, const std::string& selectedParser = "") {
        // Set the registry so Lua functions can access it
        currentSignalRegistry = &signalRegistry;

        // Copy buffer into Lua string for safe memory handling
        std::string bufferStr(buffer, length);

        bool handled = false;

        // Try each registered parser in order
        for (const auto& [parserName, parserFunc] : packetParsers) {
            // If a specific parser is selected, skip others
            if (!selectedParser.empty() && parserName != selectedParser) {
                continue;
            }

            try {
                auto result = parserFunc(bufferStr, length);

                if (!result.valid()) {
                    sol::error err = result;
                    printf("[LuaScriptManager] Parser '%s' error: %s\n", parserName.c_str(), err.what());
                    continue;
                }

                // Check if parser handled the packet (returned true)
                if (result.return_count() > 0) {
                    sol::object retVal = result;
                    if (retVal.is<bool>() && retVal.as<bool>()) {
                        handled = true;
                        break; // First parser that handles it wins
                    }
                }
            } catch (const std::exception& e) {
                printf("[LuaScriptManager] Exception in parser '%s': %s\n", parserName.c_str(), e.what());
            }
        }

        // Clear the registry pointer
        currentSignalRegistry = nullptr;

        return handled;
    }

private:
    sol::state lua;
    std::vector<LuaScript> scripts;

    // Map: PacketType -> vector of (outputSignalName, luaCallback)
    std::multimap<std::string, std::pair<std::string, sol::protected_function>> packetCallbacks;

    // Vector of registered packet parsers: (parserName, parserFunction)
    std::vector<std::pair<std::string, sol::protected_function>> packetParsers;

    // Tier 3: Frame callbacks
    std::vector<sol::protected_function> frameCallbacks;

    // Tier 3: Alert monitoring
    struct Alert {
        std::string name;
        sol::protected_function conditionFunc;
        sol::protected_function actionFunc;
        double cooldownSeconds;
        double lastTriggerTime;

        Alert(const std::string& n, sol::protected_function cond, sol::protected_function act, double cooldown)
            : name(n), conditionFunc(cond), actionFunc(act), cooldownSeconds(cooldown), lastTriggerTime(-1e9) {}
    };
    std::vector<Alert> alerts;

    // Pointer to signal registry (set during executePacketCallbacks and executeFrameCallbacks)
    std::map<std::string, Signal>* currentSignalRegistry = nullptr;

    // Frame context (set during executeFrameCallbacks)
    uint64_t currentFrameNumber = 0;
    double currentDeltaTime = 0.0;
    int currentPlotCount = 0;

    // Helper functions for endianness conversion
    template<typename T>
    T swapEndian(T value) {
        static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8,
                      "Type must be 1, 2, 4, or 8 bytes");

        if constexpr (sizeof(T) == 1) {
            return value;
        } else if constexpr (sizeof(T) == 2) {
            return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
        } else if constexpr (sizeof(T) == 4) {
            return ((value & 0xFF000000) >> 24) |
                   ((value & 0x00FF0000) >> 8)  |
                   ((value & 0x0000FF00) << 8)  |
                   ((value & 0x000000FF) << 24);
        } else if constexpr (sizeof(T) == 8) {
            return ((value & 0xFF00000000000000ULL) >> 56) |
                   ((value & 0x00FF000000000000ULL) >> 40) |
                   ((value & 0x0000FF0000000000ULL) >> 24) |
                   ((value & 0x000000FF00000000ULL) >> 8)  |
                   ((value & 0x00000000FF000000ULL) << 8)  |
                   ((value & 0x0000000000FF0000ULL) << 24) |
                   ((value & 0x000000000000FF00ULL) << 40) |
                   ((value & 0x00000000000000FFULL) << 56);
        }
    }

    // Template function to read numeric values from buffer
    template<typename T>
    sol::optional<T> readNumeric(const std::string& buffer, size_t offset, bool littleEndian) {
        if (offset + sizeof(T) > buffer.size()) {
            return sol::nullopt;
        }

        T value;
        std::memcpy(&value, buffer.data() + offset, sizeof(T));

        // Determine if we need to swap
        bool isSystemLittleEndian = true; // Windows is always little-endian
        if (littleEndian != isSystemLittleEndian) {
            value = swapEndian(value);
        }

        return value;
    }

    // Expose buffer parsing API to Lua
    void exposeBufferParsingAPI() {
        // Unsigned integer readers
        lua.set_function("readUInt8", [this](const std::string& buffer, size_t offset) -> sol::optional<uint8_t> {
            if (offset >= buffer.size()) return sol::nullopt;
            return static_cast<uint8_t>(buffer[offset]);
        });

        lua.set_function("readUInt16", [this](const std::string& buffer, size_t offset, sol::optional<bool> littleEndian) -> sol::optional<uint16_t> {
            return readNumeric<uint16_t>(buffer, offset, littleEndian.value_or(true));
        });

        lua.set_function("readUInt32", [this](const std::string& buffer, size_t offset, sol::optional<bool> littleEndian) -> sol::optional<uint32_t> {
            return readNumeric<uint32_t>(buffer, offset, littleEndian.value_or(true));
        });

        lua.set_function("readUInt64", [this](const std::string& buffer, size_t offset, sol::optional<bool> littleEndian) -> sol::optional<uint64_t> {
            return readNumeric<uint64_t>(buffer, offset, littleEndian.value_or(true));
        });

        // Signed integer readers
        lua.set_function("readInt8", [this](const std::string& buffer, size_t offset) -> sol::optional<int8_t> {
            if (offset >= buffer.size()) return sol::nullopt;
            return static_cast<int8_t>(buffer[offset]);
        });

        lua.set_function("readInt16", [this](const std::string& buffer, size_t offset, sol::optional<bool> littleEndian) -> sol::optional<int16_t> {
            return readNumeric<int16_t>(buffer, offset, littleEndian.value_or(true));
        });

        lua.set_function("readInt32", [this](const std::string& buffer, size_t offset, sol::optional<bool> littleEndian) -> sol::optional<int32_t> {
            return readNumeric<int32_t>(buffer, offset, littleEndian.value_or(true));
        });

        lua.set_function("readInt64", [this](const std::string& buffer, size_t offset, sol::optional<bool> littleEndian) -> sol::optional<int64_t> {
            return readNumeric<int64_t>(buffer, offset, littleEndian.value_or(true));
        });

        // Floating point readers
        lua.set_function("readFloat", [this](const std::string& buffer, size_t offset, sol::optional<bool> littleEndian) -> sol::optional<float> {
            if (offset + sizeof(float) > buffer.size()) return sol::nullopt;

            uint32_t bits = readNumeric<uint32_t>(buffer, offset, littleEndian.value_or(true)).value_or(0);
            float value;
            std::memcpy(&value, &bits, sizeof(float));
            return value;
        });

        lua.set_function("readDouble", [this](const std::string& buffer, size_t offset, sol::optional<bool> littleEndian) -> sol::optional<double> {
            if (offset + sizeof(double) > buffer.size()) return sol::nullopt;

            uint64_t bits = readNumeric<uint64_t>(buffer, offset, littleEndian.value_or(true)).value_or(0);
            double value;
            std::memcpy(&value, &bits, sizeof(double));
            return value;
        });

        // String readers
        lua.set_function("readString", [this](const std::string& buffer, size_t offset, size_t length) -> sol::optional<std::string> {
            if (offset + length > buffer.size()) return sol::nullopt;
            return buffer.substr(offset, length);
        });

        lua.set_function("readCString", [this](const std::string& buffer, size_t offset) -> sol::optional<std::string> {
            if (offset >= buffer.size()) return sol::nullopt;

            size_t nullPos = buffer.find('\0', offset);
            if (nullPos == std::string::npos) {
                // No null terminator found, return rest of string
                return buffer.substr(offset);
            }
            return buffer.substr(offset, nullPos - offset);
        });

        // Buffer inspection utilities
        lua.set_function("getBufferLength", [](const std::string& buffer) -> size_t {
            return buffer.size();
        });

        lua.set_function("getBufferByte", [](const std::string& buffer, size_t index) -> sol::optional<uint8_t> {
            if (index >= buffer.size()) return sol::nullopt;
            return static_cast<uint8_t>(buffer[index]);
        });

        // Debug utility - convert bytes to hex string
        lua.set_function("bytesToHex", [](const std::string& buffer, size_t offset, size_t length) -> sol::optional<std::string> {
            if (offset + length > buffer.size()) return sol::nullopt;

            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            for (size_t i = 0; i < length; ++i) {
                ss << std::setw(2) << static_cast<int>(static_cast<uint8_t>(buffer[offset + i]));
                if (i < length - 1) ss << " ";
            }
            return ss.str();
        });

        // Signal manipulation functions
        lua.set_function("update_signal", [this](const std::string& name, double timestamp, double value) {
            if (currentSignalRegistry == nullptr) {
                printf("[Lua] Warning: Cannot update signal '%s' - no active signal registry\n", name.c_str());
                return;
            }

            // Get or create the signal
            if (currentSignalRegistry->find(name) == currentSignalRegistry->end()) {
                (*currentSignalRegistry)[name] = Signal(name, 2000, PlaybackMode::ONLINE);
            }

            Signal& sig = (*currentSignalRegistry)[name];
            sig.AddPoint(timestamp, value);
        });

        lua.set_function("create_signal", [this](const std::string& name) {
            if (currentSignalRegistry == nullptr) {
                printf("[Lua] Warning: Cannot create signal '%s' - no active signal registry\n", name.c_str());
                return;
            }

            if (currentSignalRegistry->find(name) == currentSignalRegistry->end()) {
                (*currentSignalRegistry)[name] = Signal(name, 2000, PlaybackMode::ONLINE);
                printf("[Lua] Created signal: %s\n", name.c_str());
            }
        });
    }

    // Get current timestamp for a packet type by looking at its signals
    double getCurrentTimestamp(std::map<std::string, Signal>& signalRegistry, const std::string& packetType) {
        double currentTime = 0.0;

        // Look for signals belonging to this packet type (e.g., "IMU.accelX" for packet "IMU")
        for (const auto& [name, sig] : signalRegistry) {
            // Check if signal name starts with packetType
            if (name.find(packetType + ".") == 0 && !sig.dataX.empty()) {
                double lastTime;
                if (sig.mode == PlaybackMode::ONLINE && sig.dataX.size() >= sig.maxSize) {
                    // Circular buffer is full - latest value is at (offset - 1)
                    int latestIndex = (sig.offset - 1 + sig.maxSize) % sig.maxSize;
                    lastTime = sig.dataX[latestIndex];
                } else {
                    // Buffer not full yet, or offline mode - latest is at back()
                    lastTime = sig.dataX.back();
                }

                if (lastTime > currentTime) {
                    currentTime = lastTime;
                }
            }
        }

        return currentTime;
    }

    // Expose signal registry API to Lua
    void exposeSignalAPI() {
        // Create a "signals" table that provides access to signal values
        lua["signals"] = lua.create_table();

        // Expose buffer parsing API
        exposeBufferParsingAPI();

        // Function to get the latest value of a signal
        lua.set_function("get_signal", [this](const std::string& name) -> sol::optional<double> {
            if (currentSignalRegistry == nullptr) {
                return sol::nullopt;
            }

            auto it = currentSignalRegistry->find(name);
            if (it == currentSignalRegistry->end() || it->second.dataY.empty()) {
                return sol::nullopt;
            }

            // Get the latest value from the circular buffer
            const Signal& sig = it->second;
            if (sig.dataY.empty()) {
                return sol::nullopt;
            }

            // Account for circular buffer - latest value is at (offset - 1), not back()
            if (sig.mode == PlaybackMode::ONLINE && sig.dataY.size() >= sig.maxSize) {
                // Circular buffer is full
                int latestIndex = (sig.offset - 1 + sig.maxSize) % sig.maxSize;
                return sig.dataY[latestIndex];
            } else {
                // Buffer not full yet, or offline mode
                return sig.dataY.back();
            }
        });

        // Function to get N latest values of a signal
        lua.set_function("get_signal_history", [this](const std::string& name, int count) -> sol::optional<std::vector<double>> {
            if (currentSignalRegistry == nullptr) {
                return sol::nullopt;
            }

            auto it = currentSignalRegistry->find(name);
            if (it == currentSignalRegistry->end()) {
                return sol::nullopt;
            }

            const Signal& sig = it->second;
            int available = (int)sig.dataY.size();
            int n = std::min(count, available);

            std::vector<double> result;
            result.reserve(n);

            for (int i = available - n; i < available; i++) {
                result.push_back(sig.dataY[i]);
            }

            return result;
        });

        // Function to check if a signal exists
        lua.set_function("signal_exists", [this](const std::string& name) -> bool {
            if (currentSignalRegistry == nullptr) {
                return false;
            }
            return currentSignalRegistry->find(name) != currentSignalRegistry->end();
        });
    }

    // Tier 3: Register a frame callback function
    void registerFrameCallback(sol::protected_function func) {
        frameCallbacks.push_back(func);
        printf("[LuaScriptManager] Registered frame callback (total: %zu)\n", frameCallbacks.size());
    }

    // Tier 3: Register an alert with condition monitoring
    void registerAlert(const std::string& alertName,
                      sol::protected_function conditionFunc,
                      sol::protected_function actionFunc,
                      double cooldownSeconds) {
        alerts.emplace_back(alertName, conditionFunc, actionFunc, cooldownSeconds);
        printf("[LuaScriptManager] Registered alert: %s (cooldown: %.1fs)\n", alertName.c_str(), cooldownSeconds);
    }

    // Tier 3: Execute all alerts and check conditions
    void executeAlerts() {
        if (currentSignalRegistry == nullptr) {
            return;
        }

        // Get current time from any signal (use the latest timestamp available)
        double currentTime = 0.0;
        for (const auto& [name, sig] : *currentSignalRegistry) {
            if (!sig.dataX.empty()) {
                double lastTime;
                if (sig.mode == PlaybackMode::ONLINE && sig.dataX.size() >= sig.maxSize) {
                    int latestIndex = (sig.offset - 1 + sig.maxSize) % sig.maxSize;
                    lastTime = sig.dataX[latestIndex];
                } else {
                    lastTime = sig.dataX.back();
                }
                if (lastTime > currentTime) {
                    currentTime = lastTime;
                }
            }
        }

        // Check each alert
        for (auto& alert : alerts) {
            try {
                // Check if enough time has passed since last trigger
                if (currentTime - alert.lastTriggerTime < alert.cooldownSeconds) {
                    continue;
                }

                // Evaluate condition
                auto condResult = alert.conditionFunc();
                if (!condResult.valid()) {
                    sol::error err = condResult;
                    printf("[LuaScriptManager] Alert '%s' condition error: %s\n", alert.name.c_str(), err.what());
                    continue;
                }

                // If condition is true, execute action
                if (condResult.return_count() > 0) {
                    sol::object retVal = condResult;
                    if (retVal.is<bool>() && retVal.as<bool>()) {
                        // Trigger the action
                        auto actionResult = alert.actionFunc();
                        if (!actionResult.valid()) {
                            sol::error err = actionResult;
                            printf("[LuaScriptManager] Alert '%s' action error: %s\n", alert.name.c_str(), err.what());
                        }
                        alert.lastTriggerTime = currentTime;
                    }
                }
            } catch (const std::exception& e) {
                printf("[LuaScriptManager] Exception in alert '%s': %s\n", alert.name.c_str(), e.what());
            }
        }
    }

    // Register a packet callback function
    // packetType: e.g., "IMU", "GPS", "BAT"
    // outputName: Name of the derived signal to create
    // func: Lua function that computes the value
    void registerPacketCallback(const std::string& packetType, const std::string& outputName, sol::protected_function func) {
        packetCallbacks.insert({packetType, {outputName, func}});
        printf("[LuaScriptManager] Registered packet callback for: %s -> %s\n", packetType.c_str(), outputName.c_str());
    }

    // Register a packet parser function
    // parserName: Name of the parser (for debugging)
    // func: Lua function that receives (buffer, length) and returns true if packet was handled
    void registerPacketParser(const std::string& parserName, sol::protected_function func) {
        packetParsers.push_back({parserName, func});
        printf("[LuaScriptManager] Registered packet parser: %s\n", parserName.c_str());
    }
};
