#pragma once

#include "LuaThreadManager.hpp"
#include "LuaScriptManager.hpp"
#include <chrono>

// -------------------------------------------------------------------------
// Lua Worker Thread Implementation
// -------------------------------------------------------------------------

inline void LuaWorkerThread::initializeLuaState(LuaScriptManager* manager) {
    // Open standard Lua libraries
    lua_state.open_libraries(
        sol::lib::base, sol::lib::math, sol::lib::string,
        sol::lib::table, sol::lib::io, sol::lib::os, sol::lib::package,
        sol::lib::debug, sol::lib::bit32, sol::lib::jit, sol::lib::ffi,
        sol::lib::coroutine
    );

    // Mark this as a worker thread
    lua_state["IS_WORKER_THREAD"] = true;
    lua_state["THREAD_ID"] = thread_id;

    // -------------------------------------------------------------------------
    // Core I/O Types (UDP, TCP, Serial, CAN)
    // -------------------------------------------------------------------------
    // Note: These are thread-safe as each thread has its own sockets

    // Register socket types (copied from LuaScriptManager)
    lua_state.new_usertype<LuaUDPSocket>("UDPSocket",
        sol::constructors<LuaUDPSocket()>(),
        "bind", &LuaUDPSocket::bind,
        "receive", &LuaUDPSocket::receive,
        "receive_ptr", &LuaUDPSocket::receive_ptr,
        "set_non_blocking", &LuaUDPSocket::set_non_blocking,
        "close", &LuaUDPSocket::close,
        "is_open", &LuaUDPSocket::is_open
    );

    lua_state.new_usertype<LuaTCPSocket>("TCPSocket",
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

    lua_state.set_function("create_udp_socket", []() { return std::make_shared<LuaUDPSocket>(); });
    lua_state.set_function("create_tcp_socket", []() { return std::make_shared<LuaTCPSocket>(); });

#ifdef _WIN32
    lua_state.new_usertype<LuaSerialPort>("SerialPort",
        sol::constructors<LuaSerialPort()>(),
        "open", &LuaSerialPort::open,
        "configure", &LuaSerialPort::configure,
        "send", &LuaSerialPort::send,
        "receive", &LuaSerialPort::receive,
        "close", &LuaSerialPort::close,
        "is_open", &LuaSerialPort::is_open
    );
    lua_state.set_function("create_serial_port", []() { return std::make_shared<LuaSerialPort>(); });

    lua_state.new_usertype<LuaCANSocket>("CANSocket",
        sol::constructors<LuaCANSocket()>(),
        "open", &LuaCANSocket::open,
        "send", &LuaCANSocket::send,
        "receive", &LuaCANSocket::receive,
        "close", &LuaCANSocket::close,
        "is_open", &LuaCANSocket::is_open,
        "get_status", &LuaCANSocket::get_status
    );
    lua_state.set_function("create_can_socket", []() { return std::make_shared<LuaCANSocket>(); });
#endif

    // SharedBuffer for FFI (zero-copy)
    lua_state.new_usertype<SharedBuffer>("SharedBuffer",
        sol::constructors<SharedBuffer(size_t)>(),
        "get_ptr", &SharedBuffer::get_ptr,
        "get_size", &SharedBuffer::get_size
    );

    // -------------------------------------------------------------------------
    // Coroutine Support (within this thread)
    // -------------------------------------------------------------------------
    lua_state.set_function("spawn", [this](sol::main_function func, sol::this_main_state L) {
        std::lock_guard<std::mutex> lock(coroutine_mutex);
        sol::thread thr = sol::thread::create(L);
        sol::state_view sv = thr.state();
        spawn_queue.push_back({std::move(thr), sol::coroutine(sv, func), 0.0});
    });

    // spawn_thread() in worker threads just spawns a coroutine instead of an OS thread
    lua_state.set_function("spawn_thread", [this](sol::main_function func, sol::this_main_state L) {
        printf("[LuaWorkerThread %d] spawn_thread() called in worker - spawning coroutine instead\n", thread_id);
        std::lock_guard<std::mutex> lock(coroutine_mutex);
        sol::thread thr = sol::thread::create(L);
        sol::state_view sv = thr.state();
        spawn_queue.push_back({std::move(thr), sol::coroutine(sv, func), 0.0});
    });

    lua_state.set_function("yield", sol::yielding([](sol::variadic_args args) {
        return args;
    }));

    // -------------------------------------------------------------------------
    // Signal Update API (Lock-Free Queue Push)
    // -------------------------------------------------------------------------
    lua_state.set_function("update_signal", [this](const std::string& name, double timestamp, double value) {
        return pushSignalUpdate(name, timestamp, value);
    });

    lua_state.set_function("get_signal_id", [this](const std::string& name) -> int {
        return getSignalID(name);
    });

    lua_state.set_function("update_signal_fast", [this](int signal_id, double timestamp, double value) {
        return pushSignalUpdateFast(signal_id, timestamp, value);
    });

    // -------------------------------------------------------------------------
    // UI State Access (Read from Snapshot, Write to Event Queue)
    // -------------------------------------------------------------------------
    lua_state.set_function("get_button_clicked", [this](const std::string& title) -> bool {
        return getButtonClicked(title);
    });

    lua_state.set_function("get_toggle_state", [this](const std::string& title) -> bool {
        return getToggleState(title);
    });

    lua_state.set_function("get_text_input", [this](const std::string& title) -> std::string {
        return getTextInput(title);
    });

    lua_state.set_function("set_toggle_state", [this](const std::string& title, bool state) {
        return setToggleState(title, state);
    });

    // -------------------------------------------------------------------------
    // Utility Functions
    // -------------------------------------------------------------------------
    lua_state.set_function("log", [this](const std::string& message) {
        printf("[LuaThread %d] %s\n", thread_id, message.c_str());
    });

    lua_state.set_function("is_app_running", [this]() -> bool {
        return isAppRunning();
    });

    lua_state.set_function("get_time_seconds", []() -> double {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    });

    lua_state.set_function("sleep_ms", [](int milliseconds) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    });

    // -------------------------------------------------------------------------
    // Signal Management Functions (Thread-Safe Stubs)
    // -------------------------------------------------------------------------
    // Note: These affect shared state and should be used carefully
    // Worker threads push signals via queues, but these functions provide
    // compatibility with existing scripts

    lua_state.set_function("set_default_signal_mode", [](const std::string& mode) {
        // This is a no-op for worker threads - signals are always in online mode
        // because they're pushed through queues
        printf("[LuaWorkerThread] Warning: set_default_signal_mode() called in worker thread (ignored)\n");
    });

    lua_state.set_function("clear_all_signals", []() {
        // This is a no-op for worker threads - signals are managed by main thread
        printf("[LuaWorkerThread] Warning: clear_all_signals() called in worker thread (ignored)\n");
    });

    // -------------------------------------------------------------------------
    // File Dialog Functions (Not Available in Worker Threads)
    // -------------------------------------------------------------------------
    // ImGui file dialogs must be called from main thread only
    // These are stubs that warn if called from worker threads

    lua_state.set_function("open_file_dialog", [](const std::string& k, const std::string& t, const std::string& f) {
        printf("[LuaWorkerThread] Error: open_file_dialog() cannot be called from worker thread\n");
    });

    lua_state.set_function("is_file_dialog_open", [](const std::string& k) -> bool {
        return false;  // Always return false in worker threads
    });

    lua_state.set_function("get_file_dialog_result", [this](const std::string& k) -> sol::object {
        return sol::lua_nil;  // Always return nil in worker threads
    });

    // -------------------------------------------------------------------------
    // Async Socket Helpers (Pure Lua)
    // -------------------------------------------------------------------------
    auto scriptResult = lua_state.safe_script(R"(
        -- Global sleep helper
        function sleep(s)
            yield(s)
        end

        -- Async UDP helpers
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
    )");

    if (!scriptResult.valid()) {
        sol::error err = scriptResult;
        printf("[LuaWorkerThread %d] Error initializing async helpers: %s\n", thread_id, err.what());
    }

    printf("[LuaWorkerThread %d] Lua state initialized\n", thread_id);
}

inline void LuaWorkerThread::run(sol::protected_function lua_func) {
    printf("[LuaWorkerThread %d] Thread started\n", thread_id);

    // Execute the Lua function
    auto result = lua_func();
    if (!result.valid()) {
        sol::error err = result;
        printf("[LuaWorkerThread %d] Error executing function: %s\n", thread_id, err.what());
    }

    // Main loop: Run coroutine scheduler until stopped
    auto last_frame_time = std::chrono::high_resolution_clock::now();

    while (!should_stop.load(std::memory_order_acquire) && isAppRunning()) {
        auto current_frame_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = current_frame_time - last_frame_time;
        double deltaTime = elapsed.count();
        last_frame_time = current_frame_time;

        // Run coroutine scheduler
        runCoroutineScheduler(deltaTime);

        // Small sleep to avoid spinning (can be removed if coroutines always yield)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    is_running.store(false, std::memory_order_release);
    printf("[LuaWorkerThread %d] Thread stopped\n", thread_id);
}

// -------------------------------------------------------------------------
// Run Lua file (loads and executes in worker thread's Lua state)
// -------------------------------------------------------------------------
inline void LuaWorkerThread::runFile(const std::string& filepath) {
    printf("[LuaWorkerThread %d] Thread started (file mode): %s\n", thread_id, filepath.c_str());

    // Load and execute the Lua file in this thread's Lua state
    // This will re-execute the script, defining all FFI structs, functions, etc.
    // When it hits spawn_thread(), it will create a coroutine instead of a new OS thread
    auto result = lua_state.safe_script_file(filepath);
    if (!result.valid()) {
        sol::error err = result;
        printf("[LuaWorkerThread %d] Error loading file: %s\n", thread_id, err.what());
    }

    // Main loop: Run coroutine scheduler until stopped
    auto last_frame_time = std::chrono::high_resolution_clock::now();

    while (!should_stop.load(std::memory_order_acquire) && isAppRunning()) {
        auto current_frame_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = current_frame_time - last_frame_time;
        double deltaTime = elapsed.count();
        last_frame_time = current_frame_time;

        // Run coroutine scheduler
        runCoroutineScheduler(deltaTime);

        // Small sleep to avoid spinning (can be removed if coroutines always yield)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    is_running.store(false, std::memory_order_release);
    printf("[LuaWorkerThread %d] Thread stopped\n", thread_id);
}
