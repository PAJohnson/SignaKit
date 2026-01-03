#pragma once

#include <sol/sol.hpp>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <filesystem>
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

        // Expose logging API
        lua.set_function("log", [](const std::string& message) {
            printf("[Lua] %s\n", message.c_str());
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
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                loadScript(entry.path().string());
            }
        }
    }

    // Reload all loaded scripts
    void reloadAllScripts() {
        printf("[LuaScriptManager] Reloading all scripts...\n");

        // Clear packet callbacks (they'll be re-registered by scripts)
        packetCallbacks.clear();

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

private:
    sol::state lua;
    std::vector<LuaScript> scripts;

    // Map: PacketType -> vector of (outputSignalName, luaCallback)
    std::multimap<std::string, std::pair<std::string, sol::protected_function>> packetCallbacks;

    // Pointer to signal registry (set during executePacketCallbacks)
    std::map<std::string, Signal>* currentSignalRegistry = nullptr;

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

    // Register a packet callback function
    // packetType: e.g., "IMU", "GPS", "BAT"
    // outputName: Name of the derived signal to create
    // func: Lua function that computes the value
    void registerPacketCallback(const std::string& packetType, const std::string& outputName, sol::protected_function func) {
        packetCallbacks.insert({packetType, {outputName, func}});
        printf("[LuaScriptManager] Registered packet callback for: %s -> %s\n", packetType.c_str(), outputName.c_str());
    }
};
