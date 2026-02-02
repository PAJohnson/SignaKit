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
#include <thread>
#include <atomic>
#include <chrono>
#include "types.hpp"
#include "ui_state.hpp"
#include "ImGuiFileDialog.h"

// Tier 5: sockpp for network I/O
#include <sockpp/udp_socket.h>
#include <sockpp/tcp_connector.h>
#include <sockpp/inet_address.h>
#include <sockpp/exception.h>

namespace fs = std::filesystem;

#include "LuaSerialPort.hpp"
#include "LuaCANSocket.hpp"

// For image buffer updates
#include <SDL2/SDL_opengl.h>
#include "stb_image.h"




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

// Tier 5: Robust bridge between C++ and LuaJIT FFI
struct SharedBuffer {
    std::vector<uint8_t> data;
    SharedBuffer(size_t size) : data(size) {}
    
    void* get_ptr() { return data.data(); }
    size_t get_size() const { return data.size(); }
};

// Tier 5: Lua-accessible UDP socket wrapper using sockpp
class LuaUDPSocket {
private:
    sockpp::udp_socket socket;
    bool is_bound = false;

public:
    LuaUDPSocket() = default;

    // Bind socket to local address and port
    bool bind(const std::string& host, int port) {
        try {
            sockpp::inet_address addr(host, static_cast<in_port_t>(port));
            if (!socket.bind(addr)) {
                printf("[LuaUDPSocket] Failed to bind to %s:%d - %s\n",
                       host.c_str(), port, socket.last_error_str().c_str());
                return false;
            }
            is_bound = true;

            // Set non-blocking by default
            socket.set_non_blocking(true);
            
            // Increase receive buffer to handle bursts and GUI stalls (1MB)
            int bufSize = 1024 * 1024;
            socket.set_option(SOL_SOCKET, SO_RCVBUF, bufSize);
            
            printf("[LuaUDPSocket] Bound to %s:%d (RCVBUF set to 1MB)\n", host.c_str(), port);
            return true;
        } catch (const std::exception& e) {
            printf("[LuaUDPSocket] Exception binding: %s\n", e.what());
            return false;
        }
    }

    // Receive data (non-blocking if set_non_blocking was called)
    // Returns: tuple(data_string, error_string)
    std::tuple<std::string, std::string> receive(int max_size) {
        if (!is_bound) {
            return std::make_tuple("", "not bound");
        }

        std::vector<char> buffer(max_size);
        ssize_t n = socket.recv(buffer.data(), buffer.size());

        if (n > 0) {
            return std::make_tuple(std::string(buffer.data(), n), "");
        } else if (n == 0) {
            return std::make_tuple("", "closed");
        } else {
            // n < 0: error occurred
            // For non-blocking sockets, WSAEWOULDBLOCK/EAGAIN means "no data available"
            int err = socket.last_error();
            #ifdef _WIN32
                if (err == WSAEWOULDBLOCK) {
                    return std::make_tuple("", "timeout");
                }
            #else
                if (err == EAGAIN || err == EWOULDBLOCK) {
                    return std::make_tuple("", "timeout");
                }
            #endif
            return std::make_tuple("", socket.last_error_str());
        }
    }

    // Set non-blocking mode
    bool set_non_blocking(bool enable) {
        if (!socket.set_non_blocking(enable)) {
            printf("[LuaUDPSocket] Failed to set non-blocking mode: %s\n",
                   socket.last_error_str().c_str());
            return false;
        }
        return true;
    }

    // Close socket
    void close() {
        socket.close();
        is_bound = false;
    }

    // Check if socket is open
    bool is_open() const {
        return socket.is_open();
    }
    // Receive data directly into a memory buffer (Zero-Copy for FFI)
    // ptr: Pointer to the buffer (handles sol::lightuserdata automatically)
    // max_size: Buffer size
    // Returns: tuple(bytes_received, error_string)
    std::tuple<int, std::string> receive_ptr(void* ptr, int max_size) {
        if (!is_bound) {
            return std::make_tuple(-1, "not bound");
        }
        if (!ptr) {
            return std::make_tuple(-1, "null pointer");
        }

        ssize_t n = socket.recv(ptr, max_size);

        if (n > 0) {
            return std::make_tuple((int)n, "");
        } else if (n == 0) {
            return std::make_tuple(0, "closed");
        } else {
            int err = socket.last_error();
            #ifdef _WIN32
                if (err == WSAEWOULDBLOCK) {
                    return std::make_tuple(0, "timeout"); 
                }
            #else
                if (err == EAGAIN || err == EWOULDBLOCK) {
                    return std::make_tuple(0, "timeout");
                }
            #endif
            return std::make_tuple(-1, socket.last_error_str());
        }
    }
};

// Tier 5: Lua-accessible TCP socket wrapper using sockpp
class LuaTCPSocket {
private:
    sockpp::tcp_connector socket;

public:
    LuaTCPSocket() {
        // TCP sockets are non-blocking by default in this framework
        socket.set_non_blocking(true);
    }

    // Connect to remote address and port
    bool connect(const std::string& host, int port) {
        try {
            sockpp::inet_address addr(host, static_cast<in_port_t>(port));
            if (!socket.connect(addr)) {
                int err = socket.last_error();
                #ifdef _WIN32
                    if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS) {
                        return true; // Connection in progress
                    }
                #else
                    if (err == EINPROGRESS) {
                        return true; // Connection in progress
                    }
                #endif
                printf("[LuaTCPSocket] Failed to connect to %s:%d - %s\n",
                       host.c_str(), port, socket.last_error_str().c_str());
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            printf("[LuaTCPSocket] Exception connecting: %s\n", e.what());
            return false;
        }
    }

    // Send data
    std::tuple<int, std::string> send(const std::string& data) {
        ssize_t n = socket.write(data.data(), data.size());
        if (n >= 0) {
            return std::make_tuple((int)n, "");
        } else {
            return std::make_tuple(-1, socket.last_error_str());
        }
    }

    // Receive data
    std::tuple<std::string, std::string> receive(int max_size) {
        std::vector<char> buffer(max_size);
        ssize_t n = socket.read(buffer.data(), buffer.size());

        if (n > 0) {
            return std::make_tuple(std::string(buffer.data(), n), "");
        } else if (n == 0) {
            return std::make_tuple("", "closed");
        } else {
            int err = socket.last_error();
            #ifdef _WIN32
                if (err == WSAEWOULDBLOCK) {
                    return std::make_tuple("", "timeout");
                }
            #else
                if (err == EAGAIN || err == EWOULDBLOCK) {
                    return std::make_tuple("", "timeout");
                }
            #endif
            return std::make_tuple("", socket.last_error_str());
        }
    }

    // Receive data directly into a memory buffer (Zero-Copy for FFI)
    std::tuple<int, std::string> receive_ptr(void* ptr, int max_size) {
        if (!ptr) {
            return std::make_tuple(-1, "null pointer");
        }

        ssize_t n = socket.read(ptr, max_size);

        if (n > 0) {
            return std::make_tuple((int)n, "");
        } else if (n == 0) {
            return std::make_tuple(0, "closed");
        } else {
            int err = socket.last_error();
            #ifdef _WIN32
                if (err == WSAEWOULDBLOCK) {
                    return std::make_tuple(0, "timeout"); 
                }
            #else
                if (err == EAGAIN || err == EWOULDBLOCK) {
                    return std::make_tuple(0, "timeout");
                }
            #endif
            return std::make_tuple(-1, socket.last_error_str());
        }
    }

    // Set non-blocking mode
    bool set_non_blocking(bool enable) {
        if (!socket.set_non_blocking(enable)) {
            printf("[LuaTCPSocket] Failed to set non-blocking mode: %s\n",
                   socket.last_error_str().c_str());
            return false;
        }
        return true;
    }

    // Close socket
    void close() {
        socket.close();
    }

    // Check if socket is open/connected
    bool is_open() const {
        return socket.is_open();
    }
    
    bool is_connected() const {
        // For TCP, we can check if it's connected by trying to get peer address
        // or using sockpp's internal state if available.
        return socket.is_open(); // Basic check
    }
};

class LuaScriptManager {
private:
    // Tier 5: Socket library initializer (must be created before any sockets)
    // static inline sockpp::socket_initializer sockInit;

public:
    LuaScriptManager() {
        sockpp::initialize();
        initializeLuaState();
    }

    // Initialize Lua state and expose API
    void initializeLuaState() {
        lua = sol::state();
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                          sol::lib::table, sol::lib::io, sol::lib::os, sol::lib::package,
                          sol::lib::debug, sol::lib::bit32, sol::lib::jit, sol::lib::ffi,
                          sol::lib::coroutine);

        // ---------------------------------------------------------------------
        // Tier 5: Core Types & I/O (Register FIRST for dependencies)
        // ---------------------------------------------------------------------

        // UDPSocket usertype
        lua.new_usertype<LuaUDPSocket>("UDPSocket",
            sol::constructors<LuaUDPSocket()>(),
            "bind", &LuaUDPSocket::bind,
            "receive", &LuaUDPSocket::receive,
            "receive_ptr", &LuaUDPSocket::receive_ptr,
            "set_non_blocking", &LuaUDPSocket::set_non_blocking,
            "close", &LuaUDPSocket::close,
            "is_open", &LuaUDPSocket::is_open
        );

        // TCPSocket usertype
        lua.new_usertype<LuaTCPSocket>("TCPSocket",
            sol::constructors<LuaTCPSocket()>(),
            "connect", &LuaTCPSocket::connect,
            "send", &LuaTCPSocket::send,
            "receive", &LuaTCPSocket::receive,
            "receive_ptr", &LuaTCPSocket::receive_ptr,
            "set_non_blocking", &LuaTCPSocket::set_non_blocking,
            "close", &LuaTCPSocket::close,
            "is_open", &LuaTCPSocket::is_open,
            "is_connected", &LuaTCPSocket::is_connected
        );

        lua.set_function("create_udp_socket", []() { return std::make_shared<LuaUDPSocket>(); });
        lua.set_function("create_tcp_socket", []() { return std::make_shared<LuaTCPSocket>(); });

#ifdef _WIN32
        // SerialPort usertype (Windows only)
        lua.new_usertype<LuaSerialPort>("SerialPort",
            sol::constructors<LuaSerialPort()>(),
            "open", &LuaSerialPort::open,
            "configure", &LuaSerialPort::configure,
            "send", &LuaSerialPort::send,
            "receive", &LuaSerialPort::receive,
            "close", &LuaSerialPort::close,
            "is_open", &LuaSerialPort::is_open
        );

        lua.set_function("create_serial_port", []() { return std::make_shared<LuaSerialPort>(); });

        // CANSocket usertype (Windows only, requires PCAN drivers)
        lua.new_usertype<LuaCANSocket>("CANSocket",
            sol::constructors<LuaCANSocket()>(),
            "open", &LuaCANSocket::open,
            "send", &LuaCANSocket::send,
            "receive", &LuaCANSocket::receive,
            "close", &LuaCANSocket::close,
            "is_open", &LuaCANSocket::is_open,
            "get_status", &LuaCANSocket::get_status
        );

        lua.set_function("create_can_socket", []() { return std::make_shared<LuaCANSocket>(); });
#endif


        // SharedBuffer usertype (FFI support)
        lua.new_usertype<SharedBuffer>("SharedBuffer",
            sol::constructors<SharedBuffer(size_t)>(),
            "get_ptr", &SharedBuffer::get_ptr,
            "get_size", &SharedBuffer::get_size
        );
        
        // spawn(function) - starts a new coroutine
        lua.set_function("spawn", [this](sol::main_function func, sol::this_main_state L) {
            std::lock_guard<std::mutex> lock(activeCoroutinesMut);
            sol::thread thr = sol::thread::create(L);
            sol::state_view sv = thr.state();
            spawnQueue.push_back({std::move(thr), sol::coroutine(sv, func), 0.0});
        });

        // yield(...) - yields control back to C++ with optional values
        lua.set_function("yield", sol::yielding([](sol::variadic_args args) {
            return args;
        }));

        // Expose the signal registry API
        exposeSignalAPI();

        // Expose packet parser registration API
        lua.set_function("register_parser", [this](const std::string& parserName, sol::main_protected_function func) {
            registerPacketParser(parserName, func);
        });

        // Optimization: Check if a signal is active in the UI
        lua.set_function("is_signal_active", [this](const std::string& name) -> bool {
            if (currentUIPlotState == nullptr) return true;
            return currentUIPlotState->isSignalActive(name);
        });

        // Optimization: Check if a packet type has any Lua callbacks registered
        lua.set_function("has_packet_callback", [this](const std::string& packetType) -> bool {
            sol::object callbacks = lua["_packet_callbacks"][packetType];
            return callbacks.valid() && callbacks.get_type() == sol::type::table && callbacks.as<sol::table>().size() > 0;
        });

        // Expose logging API
        lua.set_function("log", [](const std::string& message) {
            printf("[Lua] %s\n", message.c_str());
        });

        // Pure-Lua packet callback system & async helpers
        auto scriptResult = lua.safe_script(R"(
            -- Global registry of packet callbacks (stored in Lua, not C++)
            _packet_callbacks = {}

            -- Register a packet callback
            function on_packet(packetType, outputName, func)
                if not _packet_callbacks[packetType] then
                    _packet_callbacks[packetType] = {}
                end
                table.insert(_packet_callbacks[packetType], {
                    outputName = outputName,
                    callback = func
                })
                log("Registered packet callback: " .. packetType .. " -> " .. outputName)
            end

            -- Execute packet callbacks
            function trigger_packet_callbacks(packetType, timestamp)
                local callbacks = _packet_callbacks[packetType]
                if not callbacks then return end

                for _, cb in ipairs(callbacks) do
                    local success, result = pcall(cb.callback)
                    if success and type(result) == "number" then
                        update_signal(cb.outputName, timestamp, result)
                    elseif not success then
                        log("Error in packet callback " .. cb.outputName .. ": " .. tostring(result))
                    end
                end
            end

            -- Global sleep helper
            function sleep(s)
                yield(s)
            end

            -- Async Socket helper
            function UDPSocket:receive_async(max_size)
                while true do
                    local data, err = self:receive(max_size or 65536)
                    if data and #data > 0 then
                        return data, err
                    end
                    if err ~= "timeout" then
                        return nil, err
                    end
                    yield()
                end
            end

            -- Async Socket helper for pointers
            function UDPSocket:receive_ptr_async(ptr, max_size)
                while true do
                    local len, err = self:receive_ptr(ptr, max_size or 65536)
                    if len > 0 then
                        return len, err
                    end
                    if err ~= "timeout" then
                        return -1, err
                    end
                    yield()
                end
            end

            -- Async TCP helpers
            function TCPSocket:connect_async(host, port)
                local success = self:connect(host, port)
                if not success then return false, "failed to initiate" end
                
                while not self:is_connected() do
                    yield()
                end
                return true
            end

            function TCPSocket:send_async(data)
                while true do
                    local n, err = self:send(data)
                    if n >= 0 then return n, err end
                    -- For TCP, wait and retry if needed (though usually send is non-blocking with buffer)
                    yield()
                end
            end

            function TCPSocket:receive_async(max_size)
                while true do
                    local data, err = self:receive(max_size or 65536)
                    if data and #data > 0 then
                        return data, err
                    end
                    if err == "closed" then
                        return nil, "closed"
                    end
                    if err ~= "timeout" then
                        return nil, err
                    end
                    yield()
                end
            end

            function TCPSocket:receive_ptr_async(ptr, max_size)
                while true do
                    local len, err = self:receive_ptr(ptr, max_size or 65536)
                    if len > 0 then
                        return len, err
                    end
                    if err == "closed" then
                        return 0, "closed"
                    end
                    if err ~= "timeout" then
                        return -1, err
                    end
                    yield()
                end
            end
            
            -- Async SerialPort helpers (Windows only)
            function SerialPort:receive_async(max_size)
                while true do
                    local data, err = self:receive(max_size or 65536)
                    if data and #data > 0 then
                        return data, err
                    end
                    if err ~= "timeout" then
                        return nil, err
                    end
                    yield()
                end
            end

            function SerialPort:send_async(data)
                while true do
                    local n, err = self:send(data)
                    if n >= 0 then return n, err end
                    yield()
                end
            end
            
            -- Async CANSocket helpers (Windows only)
            function CANSocket:receive_async()
                while true do
                    local timestamp, id, data, ext, rtr, err = self:receive()
                    if data and #data > 0 then
                        return timestamp, id, data, ext, rtr
                    end
                    if err ~= "timeout" then
                        return nil, nil, nil, nil, nil, err
                    end
                    yield()
                end
            end
        )");

        if (!scriptResult.valid()) {
            sol::error err = scriptResult;
            printf("[LuaScriptManager] Error initializing internal Lua API: %s\n", err.what());
        }

        // Tier 3: Core registration APIs
        lua.set_function("on_frame", [this](sol::main_protected_function func) {
            registerFrameCallback(func);
        });

        lua.set_function("on_alert", [this](const std::string& alertName,
                                            sol::main_protected_function conditionFunc,
                                            sol::main_protected_function actionFunc,
                                            sol::optional<double> cooldownSeconds) {
            registerAlert(alertName, conditionFunc, actionFunc, cooldownSeconds.value_or(0.0));
        });

        lua.set_function("get_frame_number", [this]() -> uint64_t { return currentFrameNumber; });
        lua.set_function("get_delta_time", [this]() -> double { return currentDeltaTime; });
        lua.set_function("get_plot_count", [this]() -> int { return currentPlotCount; });

        // Tier 4: UI state access
        lua.set_function("get_button_clicked", [this](const std::string& title) -> bool {
            if (currentUIPlotState == nullptr) return false;
            for (const auto& b : currentUIPlotState->activeButtons) if (b.title == title) return b.clicked;
            return false;
        });

        lua.set_function("get_toggle_state", [this](const std::string& title) -> bool {
            if (currentUIPlotState == nullptr) return false;
            for (const auto& t : currentUIPlotState->activeToggles) if (t.title == title) return t.state;
            return false;
        });

        lua.set_function("set_toggle_state", [this](const std::string& title, bool state) {
            if (currentUIPlotState == nullptr) return;
            for (auto& t : currentUIPlotState->activeToggles) if (t.title == title) { t.state = state; return; }
        });

        lua.set_function("get_text_input", [this](const std::string& title) -> sol::optional<std::string> {
            if (currentUIPlotState == nullptr) return sol::nullopt;
            for (const auto& i : currentUIPlotState->activeTextInputs) if (i.title == title) return std::string(i.textBuffer);
            return sol::nullopt;
        });

        lua.set_function("on_cleanup", [this](sol::main_protected_function func) {
            cleanupCallbacks.push_back(func);
        });

        // Tier 5: Remaining System APIs
        lua.set_function("parse_packet", [this](const std::string& b, size_t l) -> bool {
            if (currentSignalRegistry == nullptr) return false;
            return parsePacket(b.c_str(), l, *currentSignalRegistry);
        });

        lua.set_function("parse_packet_ptr", [this](void* ptr, size_t length) -> bool {
            if (ptr == nullptr) return false;
            auto* registry = currentSignalRegistry ? currentSignalRegistry : defaultSignalRegistry;
            if (registry == nullptr) return false;
            return parsePacket(ptr, length, *registry, "fast_binary");
        });

        lua.set_function("sleep_ms", [this](int ms) { sleepMs(ms); });
        lua.set_function("is_app_running", [this]() -> bool { return isAppRunning(); });
        
        lua.set_function("get_time_seconds", []() -> double {
            return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        });

        lua.set_function("set_signal_mode", [this](const std::string& name, const std::string& mode) {
            if (currentSignalRegistry == nullptr) return;
            PlaybackMode m = (mode == "offline") ? PlaybackMode::OFFLINE : PlaybackMode::ONLINE;
            auto it = currentSignalRegistry->find(name);
            if (it != currentSignalRegistry->end()) it->second.SetMode(m);
            else (*currentSignalRegistry)[name] = Signal(name, 10000, m);
        });

        lua.set_function("clear_all_signals", [this]() {
            auto* r = currentSignalRegistry ? currentSignalRegistry : defaultSignalRegistry;
            if (r) for (auto& p : *r) p.second.Clear();
        });

        lua.set_function("set_default_signal_mode", [this](const std::string& mode) {
            defaultSignalMode = (mode == "offline") ? PlaybackMode::OFFLINE : PlaybackMode::ONLINE;
        });

        lua.set_function("open_file_dialog", [](const std::string& k, const std::string& t, const std::string& f) {
            IGFD::FileDialogConfig config; config.path = ".";
            ImGuiFileDialog::Instance()->OpenDialog(k.c_str(), t.c_str(), f.c_str(), config);
        });

        lua.set_function("is_file_dialog_open", [](const std::string& k) -> bool {
            return ImGuiFileDialog::Instance()->Display(k.c_str(), ImGuiWindowFlags_None, ImVec2(800, 600));
        });

        lua.set_function("get_file_dialog_result", [this](const std::string& k) -> sol::object {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string res = ImGuiFileDialog::Instance()->GetFilePathName();
                ImGuiFileDialog::Instance()->Close();
                return sol::make_object(lua, res);
            }
            ImGuiFileDialog::Instance()->Close();
            return sol::lua_nil;
        });

        // Image window content control API (find windows by title)
        lua.set_function("set_image_file", [this](const std::string& window_title, const std::string& file_path) -> bool {
            if (currentUIPlotState == nullptr) return false;
            
            // Find window by title
            for (auto& img : currentUIPlotState->activeImageWindows) {
                if (img.title == window_title) {
                    // Clear old texture if exists
                    if (img.textureID != 0) {
                        extern void DeleteTexture(unsigned int);
                        DeleteTexture(img.textureID);
                        img.textureID = 0;
                    }
                    
                    // Load new image
                    img.filePath = file_path;
                    extern unsigned int LoadImageToTexture(const std::string&, int*, int*);
                    img.textureID = LoadImageToTexture(img.filePath, &img.width, &img.height);
                    
                    return (img.textureID != 0);
                }
            }
            return false; // Window not found
        });

        lua.set_function("update_image_buffer", [this](const std::string& window_title, 
                                                        const std::string& image_data,
                                                        int width, int height,
                                                        sol::optional<std::string> format) -> bool {
            if (currentUIPlotState == nullptr) return false;
            
            // Find window by title
            for (auto& img : currentUIPlotState->activeImageWindows) {
                if (img.title == window_title) {
                    // Decode image data (JPEG or other format) into RGBA using stb_image
                    int decoded_width, decoded_height, channels;
                    unsigned char* rgba_data = stbi_load_from_memory(
                        (const unsigned char*)image_data.data(),
                        image_data.size(),
                        &decoded_width, &decoded_height, &channels,
                        4  // Force RGBA
                    );
                    
                    if (!rgba_data) {
                        printf("[ImageWindow] Failed to decode image data\\n");
                        return false;
                    }
                    
                    // Use OpenGL functions (GL constants already defined in SDL_opengl.h)
                    // Create or update texture
                    if (img.textureID == 0) {
                        // Create new texture
                        unsigned int textureID;
                        glGenTextures(1, &textureID);
                        glBindTexture(GL_TEXTURE_2D, textureID);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, decoded_width, decoded_height, 
                                    0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
                        img.textureID = textureID;
                    } else {
                        // Update existing texture
                        glBindTexture(GL_TEXTURE_2D, img.textureID);
                        
                        // If dimensions changed, reallocate
                        if (decoded_width != img.width || decoded_height != img.height) {
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, decoded_width, decoded_height,
                                        0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
                        } else {
                            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, decoded_width, decoded_height,
                                           GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
                        }
                    }
                    
                    img.width = decoded_width;
                    img.height = decoded_height;
                    img.filePath = ""; // Clear file path since this is from buffer
                    
                    stbi_image_free(rgba_data);
                    return true;
                }
            }
            return false; // Window not found
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

        // Execute cleanup callbacks before clearing state
        executeCleanupCallbacks();

        // Clear all callbacks and parsers (they'll be re-registered by scripts)
        packetParsers.clear();
        frameCallbacks.clear();
        alerts.clear();
        cleanupCallbacks.clear();
        
        {
            std::lock_guard<std::mutex> lock(activeCoroutinesMut);
            activeCoroutines.clear();
            spawnQueue.clear();
        }

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


    // Tier 3: Execute frame callbacks every GUI render frame
    // NOTE: Caller must hold stateMutex lock before calling this function
    // frameNumber: current frame number
    // deltaTime: time since last frame in seconds
    // plotCount: number of active plots
    // uiPlotState: UI state for accessing control elements (Tier 4)
    void executeFrameCallbacks(std::map<std::string, Signal>& signalRegistry,
                              uint64_t frameNumber,
                              double deltaTime,
                              int plotCount,
                              UIPlotState* uiPlotState = nullptr) {
        // Set frame context
        currentSignalRegistry = &signalRegistry;
        currentFrameNumber = frameNumber;
        currentDeltaTime = deltaTime;
        currentPlotCount = plotCount;
        currentUIPlotState = uiPlotState;

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

        // Execute all active coroutines (Scheduler)
        runCoroutineScheduler(deltaTime);

        // Execute alert monitoring
        executeAlerts();

        // Clear context
        currentSignalRegistry = nullptr;
        currentUIPlotState = nullptr;
    }

    // Tick the coroutine scheduler
    void runCoroutineScheduler(double deltaTime) {
        double currentTime = std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();

        // 1. Process Spawn Queue (Move new coroutines into the active list)
        {
            std::lock_guard<std::mutex> lock(activeCoroutinesMut);
            if (!spawnQueue.empty()) {
                activeCoroutines.insert(activeCoroutines.end(),
                    std::make_move_iterator(spawnQueue.begin()),
                    std::make_move_iterator(spawnQueue.end()));
                spawnQueue.clear();
            }
        }

        // 2. Safely iterate over active coroutines
        // Use index-based loop in case the vector reallocates (though it shouldn't now with the queue)
        for (size_t i = 0; i < activeCoroutines.size(); ) {
            auto& entry = activeCoroutines[i];

            // Check if it's sleeping
            if (currentTime < entry.wakeTime) {
                i++;
                continue;
            }

            // Resume the coroutine
            auto result = entry.coro();
            
            if (!result.valid()) {
                sol::error err = result;
                printf("[LuaScriptManager] Coroutine error: %s\n", err.what());
                activeCoroutines.erase(activeCoroutines.begin() + i);
                continue;
            }

            // Check status
            if (entry.coro.status() == sol::call_status::yielded) {
                // If it yielded with a sleep value
                if (result.return_count() > 0) {
                    sol::object ret = result[0];
                    if (ret.is<double>()) {
                        entry.wakeTime = currentTime + ret.as<double>();
                    } else {
                        // Normal yield, wake up next frame
                        entry.wakeTime = 0.0;
                    }
                } else {
                    entry.wakeTime = 0.0;
                }
                i++;
            } else {
                // Finished
                activeCoroutines.erase(activeCoroutines.begin() + i);
            }
        }
    }

    // Tier 4.5: Execute cleanup callbacks (called before script reload/unload)
    void executeCleanupCallbacks() {
        printf("[LuaScriptManager] Executing %zu cleanup callback(s)...\n", cleanupCallbacks.size());
        for (auto& func : cleanupCallbacks) {
            try {
                auto result = func();
                if (!result.valid()) {
                    sol::error err = result;
                    printf("[LuaScriptManager] Cleanup callback error: %s\n", err.what());
                }
            } catch (const std::exception& e) {
                printf("[LuaScriptManager] Exception in cleanup callback: %s\n", e.what());
            }
        }
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
    // All registered parsers are tried until one handles the packet
    bool parsePacket(const char* buffer, size_t length, std::map<std::string, Signal>& signalRegistry, PlaybackMode mode = PlaybackMode::ONLINE, const std::string& selectedParser = "") {
        // Set the registry so Lua functions can access it
        currentSignalRegistry = &signalRegistry;

        // Copy buffer into Lua string for safe memory handling
        std::string bufferStr(buffer, length);

        bool handled = false;

        // Try each registered parser in order until one handles it
        for (const auto& [parserName, parserFunc] : packetParsers) {
            // Removed parser selection filtering - all parsers run until one handles the packet

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
        // currentSignalRegistry = nullptr;

        return handled;
    }

    // Tier 5: Zero-Copy Packet Parsing (FFI Support)
    // Returns true if handled.
    bool parsePacket(const void* data, size_t length, std::map<std::string, Signal>& signalRegistry, const std::string& selectedParser = "") {
        currentSignalRegistry = &signalRegistry;
        bool handled = false;

        // Try each registered parser (ignore selectedParser - let Lua scripts handle everything)
        for (const auto& [parserName, parserFunc] : packetParsers) {
            // Removed parser selection filtering - all parsers run until one handles the packet

            try {
                // Pass pointer directly to Lua as lightuserdata
                auto result = parserFunc(data, length);
                
                if (result.valid() && result.return_count() > 0) {
                    sol::object retVal = result;
                    if (retVal.is<bool>() && retVal.as<bool>()) {
                        handled = true;
                        break;
                    }
                } else if (!result.valid()) {
                    sol::error err = result;
                    printf("[LuaScriptManager] FFI parser '%s' error: %s\n", parserName.c_str(), err.what());
                }
            } catch (const std::exception& e) {
                printf("[LuaScriptManager] Exception in FFI parser '%s': %s\n", parserName.c_str(), e.what());
            }
        }
        
        static int parseFailCounter = 0;
        if (!handled && length > 0) {
            if (parseFailCounter++ % 1000 == 0) {
                printf("[LuaScriptManager] Warning: No parser handled packet of length %zu (total unhandled: %d)\n", length, parseFailCounter);
            }
        }

        return handled;
    }

    void setAppRunningPtr(std::atomic<bool>* ptr) {
        appRunningPtr = ptr;
    }

    // Set a default signal registry to be used when no frame/packet context is active (e.g. at script load)
    void setSignalRegistry(std::map<std::string, Signal>* registry) {
        defaultSignalRegistry = registry;
    }

    void stopAllLuaThreads() {
        printf("[LuaScriptManager] Stopping all Lua threads...\n");
        threadsRunning = false;

        std::lock_guard<std::mutex> lock(threadVectorMutex);
        for (auto& luaThread : luaThreads) {
            if (luaThread->thread.joinable()) {
                luaThread->thread.join();
            }
        }
        luaThreads.clear();
        printf("[LuaScriptManager] All Lua threads stopped\n");
    }

    // Clear the fast access cache (call when registry is cleared or pointers might be invalid)
    void clearSignalCache() {
        signalCache.clear();
        signalNameCache.clear();
    }

private:
    // Fast access signal cache
    std::vector<Signal*> signalCache;
    std::unordered_map<std::string, int> signalNameCache;
    sol::state lua;
    std::vector<LuaScript> scripts;

    // Coroutine tracking
    struct CoroutineEntry {
        sol::thread thread;
        sol::coroutine coro;
        double wakeTime = 0.0;
    };
    std::vector<CoroutineEntry> activeCoroutines;
    std::vector<CoroutineEntry> spawnQueue;
    std::mutex activeCoroutinesMut;

    // Vector of registered packet parsers: (parserName, parserFunction)
    std::vector<std::pair<std::string, sol::main_protected_function>> packetParsers;

    // Tier 3: Frame callbacks
    std::vector<sol::main_protected_function> frameCallbacks;

    // Tier 4.5: Cleanup callbacks (called before script reload/unload)
    std::vector<sol::main_protected_function> cleanupCallbacks;

    // Tier 3: Alert monitoring
    struct Alert {
        std::string name;
        sol::main_protected_function conditionFunc;
        sol::main_protected_function actionFunc;
        double cooldownSeconds;
        double lastTriggerTime;

        Alert(const std::string& n, sol::main_protected_function cond, sol::main_protected_function act, double cooldown)
            : name(n), conditionFunc(cond), actionFunc(act), cooldownSeconds(cooldown), lastTriggerTime(-1e9) {}
    };
    std::vector<Alert> alerts;

    // Pointer to signal registry (set during executeFrameCallbacks or parsePacket)
    std::map<std::string, Signal>* currentSignalRegistry = nullptr;

    // Default pointer to signal registry (fallback when currentSignalRegistry is null)
    std::map<std::string, Signal>* defaultSignalRegistry = nullptr;

    // Default playback mode for new signals (changed by set_default_signal_mode)
    PlaybackMode defaultSignalMode = PlaybackMode::ONLINE;

    // Frame context (set during executeFrameCallbacks)
    uint64_t currentFrameNumber = 0;
    double currentDeltaTime = 0.0;
    int currentPlotCount = 0;

    // Tier 4: Pointer to UI state for control element access (set during executeFrameCallbacks)
    UIPlotState* currentUIPlotState = nullptr;

    // Tier 5: Lua thread management
    struct LuaThread {
        int id;
        std::thread thread;
        sol::state luaState;  // Each thread gets its own Lua state
        std::string funcBytecode;  // Store function as bytecode to transfer between states

        LuaThread(int threadId, const std::string& bytecode)
            : id(threadId), funcBytecode(bytecode) {}
    };
    std::vector<std::unique_ptr<LuaThread>> luaThreads;
    std::atomic<int> nextThreadId{0};
    std::atomic<bool> threadsRunning{true};
    std::mutex threadVectorMutex;

    // Pointer to global appRunning atomic (set from main.cpp)
    std::atomic<bool>* appRunningPtr = nullptr;

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
            std::map<std::string, Signal>* registry = currentSignalRegistry ? currentSignalRegistry : defaultSignalRegistry;
            
            if (registry == nullptr) {
                printf("[Lua] Warning: Cannot update signal '%s' - no active signal registry\n", name.c_str());
                return;
            }

            // Get or create the signal (using default mode)
            if (registry->find(name) == registry->end()) {
                (*registry)[name] = Signal(name, 10000, defaultSignalMode);
            }

            Signal& sig = (*registry)[name];
            sig.AddPoint(timestamp, value);
        });

        lua.set_function("create_signal", [this](const std::string& name) {
            std::map<std::string, Signal>* registry = currentSignalRegistry ? currentSignalRegistry : defaultSignalRegistry;

            if (registry == nullptr) {
                printf("[Lua] Warning: Cannot create signal '%s' - no active signal registry\n", name.c_str());
                return;
            }

            if (registry->find(name) == registry->end()) {
                (*registry)[name] = Signal(name, 10000, PlaybackMode::ONLINE);
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

        // Optimization: Fast Signal Access API
        // Get integer ID for a signal name (creates signal if needed)
        lua.set_function("get_signal_id", [this](const std::string& name) -> int {
            // Check cache first
            auto it = signalNameCache.find(name);
            if (it != signalNameCache.end()) {
                return it->second;
            }

            // Not in cache, get from registry
            std::map<std::string, Signal>* registry = currentSignalRegistry ? currentSignalRegistry : defaultSignalRegistry;
            if (!registry) {
                printf("[Lua] Error: get_signal_id('%s') failed - no registry found\n", name.c_str());
                return -1;
            }

            // Find or create in registry
            if (registry->find(name) == registry->end()) {
                 (*registry)[name] = Signal(name, 10000, defaultSignalMode);
                 // printf("[Lua] Registered new signal in registry: %s\n", name.c_str());
            }

            // Add to cache
            Signal* sigPtr = &(*registry)[name];
            int id = static_cast<int>(signalCache.size());
            signalCache.push_back(sigPtr);
            signalNameCache[name] = id;
            
            // printf("[Lua] Cached signal ID: %s -> %d\n", name.c_str(), id);
            return id;
        });

        // Update signal using fast ID access (O(1))
        lua.set_function("update_signal_fast", [this](int id, double timestamp, double value) {
            if (id >= 0 && id < (int)signalCache.size()) {
                Signal* sig = signalCache[id];
                
                // Debug: Log the first update for this signal to confirm pipeline is working
                static std::unordered_set<int> firstUpdateLogged;
                if (firstUpdateLogged.find(id) == firstUpdateLogged.end()) {
                    printf("[Lua] Initial update for signal ID %d (%s): t=%f, v=%f\n", 
                           id, sig->name.c_str(), timestamp, value);
                    firstUpdateLogged.insert(id);
                }
                
                sig->AddPoint(timestamp, value);
            } else if (id != -1) {
                // Only log if not -1 (which is returned on error) to avoid spam
                static int spamCounter = 0;
                if (spamCounter++ % 1000 == 0) {
                    printf("[Lua] Warning: update_signal_fast received invalid ID %d (cache size %zu)\n", 
                           id, signalCache.size());
                }
            }
        });
    }

    // Tier 3: Register a frame callback function
    void registerFrameCallback(sol::main_protected_function func) {
        frameCallbacks.push_back(func);
        printf("[LuaScriptManager] Registered frame callback (total: %zu)\n", frameCallbacks.size());
    }

    // Tier 3: Register an alert with condition monitoring
    void registerAlert(const std::string& alertName,
                      sol::main_protected_function conditionFunc,
                      sol::main_protected_function actionFunc,
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

    // Register a packet parser function
    // parserName: Name of the parser (for debugging)
    // func: Lua function that receives (buffer, length) and returns true if packet was handled
    void registerPacketParser(const std::string& parserName, sol::main_protected_function func) {
        packetParsers.push_back({parserName, func});
        printf("[LuaScriptManager] Registered packet parser: %s\n", parserName.c_str());
    }

    void sleepMs(int milliseconds) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }

    bool isAppRunning() {
        return appRunningPtr ? appRunningPtr->load() : false;
    }

    // Helper to expose buffer parsing API to a specific Lua state
    void exposeBufferParsingAPIForState(sol::state& luaState) {
        // Simplified version - just expose the essential buffer reading functions
        luaState.set_function("readUInt8", [](const std::string& buffer, size_t offset) -> sol::optional<uint8_t> {
            if (offset >= buffer.size()) return sol::nullopt;
            return static_cast<uint8_t>(buffer[offset]);
        });

        luaState.set_function("readUInt16", [this](const std::string& buffer, size_t offset, sol::optional<bool> littleEndian) -> sol::optional<uint16_t> {
            return readNumeric<uint16_t>(buffer, offset, littleEndian.value_or(true));
        });

        luaState.set_function("readDouble", [this](const std::string& buffer, size_t offset, sol::optional<bool> littleEndian) -> sol::optional<double> {
            if (offset + sizeof(double) > buffer.size()) return sol::nullopt;
            uint64_t bits = readNumeric<uint64_t>(buffer, offset, littleEndian.value_or(true)).value_or(0);
            double value;
            std::memcpy(&value, &bits, sizeof(double));
            return value;
        });

        // Add other essential buffer functions as needed
    }
};
