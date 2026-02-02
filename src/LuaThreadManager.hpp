#pragma once

#include <sol/sol.hpp>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <mutex>
#include <string>
#include "LockFreeQueues.hpp"
#include "UIStateSnapshot.hpp"
#include "types.hpp"

// Forward declarations
class LuaScriptManager;

// -------------------------------------------------------------------------
// Lua Worker Thread
// -------------------------------------------------------------------------
// Each Lua thread has:
// - Its own sol::state (Lua VM)
// - A signal queue for pushing updates to main thread
// - An event queue for receiving UI events from main thread
// - Access to a read-only UI state snapshot
// - Per-thread coroutine support (optional, within the thread)
// -------------------------------------------------------------------------

class LuaWorkerThread {
public:
    int thread_id;
    std::thread os_thread;
    sol::state lua_state;

    // Communication channels
    std::unique_ptr<SignalQueue> signal_queue;      // Thread → Main
    std::unique_ptr<EventQueue> event_queue;        // Main → Thread

    // Shared state (read-only from thread perspective)
    const AtomicSnapshotPtr* ui_snapshot_ptr;       // Pointer to main thread's snapshot manager
    SignalIDRegistry* signal_id_registry;            // Shared signal ID registry

    // Thread control
    std::atomic<bool> should_stop{false};
    std::atomic<bool> is_running{false};

    // Per-thread signal ID cache (built at startup)
    std::unordered_map<std::string, int> local_signal_cache;

    // Pointer to global appRunning atomic
    std::atomic<bool>* app_running_ptr{nullptr};

    // Coroutine support (within this thread)
    struct CoroutineEntry {
        sol::thread thread;
        sol::coroutine coro;
        double wakeTime = 0.0;
    };
    std::vector<CoroutineEntry> active_coroutines;
    std::vector<CoroutineEntry> spawn_queue;
    std::mutex coroutine_mutex;

public:
    LuaWorkerThread(int id,
                    const AtomicSnapshotPtr* snapshot_ptr,
                    SignalIDRegistry* id_registry,
                    std::atomic<bool>* app_running)
        : thread_id(id)
        , ui_snapshot_ptr(snapshot_ptr)
        , signal_id_registry(id_registry)
        , app_running_ptr(app_running)
    {
        signal_queue = std::make_unique<SignalQueue>();
        event_queue = std::make_unique<EventQueue>();
    }

    // Initialize Lua state with worker thread bindings
    void initializeLuaState(LuaScriptManager* manager);

    // Thread entry point
    void run(sol::protected_function lua_func);

    // Thread entry point from file
    void runFile(const std::string& filepath);

    // Stop the thread gracefully
    void stop() {
        should_stop.store(true, std::memory_order_release);
        if (os_thread.joinable()) {
            os_thread.join();
        }
    }

    // Exposed to Lua: Push signal update to queue
    bool pushSignalUpdate(const std::string& signal_name, double timestamp, double value) {
        // Get or create signal ID
        int sig_id = -1;
        auto it = local_signal_cache.find(signal_name);
        if (it != local_signal_cache.end()) {
            sig_id = it->second;
        } else {
            // Cache miss - get from global registry and cache locally
            sig_id = signal_id_registry->get_or_create(signal_name);
            local_signal_cache[signal_name] = sig_id;
        }

        if (sig_id < 0) {
            return false;
        }

        SignalUpdate update(sig_id, timestamp, value);
        return signal_queue->push(update);
    }

    // Exposed to Lua: Push signal update using pre-cached ID
    bool pushSignalUpdateFast(int signal_id, double timestamp, double value) {
        SignalUpdate update(signal_id, timestamp, value);
        return signal_queue->push(update);
    }

    // Exposed to Lua: Get or cache signal ID
    int getSignalID(const std::string& signal_name) {
        auto it = local_signal_cache.find(signal_name);
        if (it != local_signal_cache.end()) {
            return it->second;
        }

        int sig_id = signal_id_registry->get_or_create(signal_name);
        local_signal_cache[signal_name] = sig_id;
        return sig_id;
    }

    // Exposed to Lua: Get UI state (read from snapshot)
    bool getButtonClicked(const std::string& title) {
        const UIStateSnapshot* snapshot = ui_snapshot_ptr->getReadSnapshot();
        return snapshot ? snapshot->getButtonClicked(title) : false;
    }

    bool getToggleState(const std::string& title) {
        const UIStateSnapshot* snapshot = ui_snapshot_ptr->getReadSnapshot();
        bool result = snapshot ? snapshot->getToggleState(title) : false;

        // Debug output
        static int call_count = 0;
        if (++call_count % 1000 == 0) {
            printf("[LuaWorkerThread %d] getToggleState('%s') = %s (snapshot=%p)\n",
                   thread_id, title.c_str(), result ? "true" : "false", (void*)snapshot);
        }

        return result;
    }

    std::string getTextInput(const std::string& title) {
        const UIStateSnapshot* snapshot = ui_snapshot_ptr->getReadSnapshot();
        return snapshot ? snapshot->getTextInput(title) : std::string();
    }

    // Exposed to Lua: Set UI state (push to event queue)
    bool setToggleState(const std::string& title, bool state) {
        UIEvent event;
        event.type = UIEventType::SET_TOGGLE_STATE;
        strncpy(event.title, title.c_str(), sizeof(event.title) - 1);
        event.title[sizeof(event.title) - 1] = '\0';
        event.boolValue = state;
        return event_queue->push(event);
    }

    bool setTextInput(const std::string& title, const std::string& text) {
        UIEvent event;
        event.type = UIEventType::SET_TEXT_INPUT;
        strncpy(event.title, title.c_str(), sizeof(event.title) - 1);
        event.title[sizeof(event.title) - 1] = '\0';
        strncpy(event.textValue, text.c_str(), sizeof(event.textValue) - 1);
        event.textValue[sizeof(event.textValue) - 1] = '\0';
        return event_queue->push(event);
    }

    // Check if app is still running
    bool isAppRunning() const {
        return app_running_ptr ? app_running_ptr->load(std::memory_order_acquire) : false;
    }

    // Coroutine scheduler (runs within this thread)
    void runCoroutineScheduler(double deltaTime) {
        double currentTime = std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();

        // Process spawn queue
        {
            std::lock_guard<std::mutex> lock(coroutine_mutex);
            if (!spawn_queue.empty()) {
                active_coroutines.insert(active_coroutines.end(),
                    std::make_move_iterator(spawn_queue.begin()),
                    std::make_move_iterator(spawn_queue.end()));
                spawn_queue.clear();
            }
        }

        // Run active coroutines
        for (size_t i = 0; i < active_coroutines.size(); ) {
            auto& entry = active_coroutines[i];

            // Check if sleeping
            if (currentTime < entry.wakeTime) {
                i++;
                continue;
            }

            // Resume coroutine
            auto result = entry.coro();

            if (!result.valid()) {
                sol::error err = result;
                printf("[LuaWorkerThread %d] Coroutine error: %s\n", thread_id, err.what());
                active_coroutines.erase(active_coroutines.begin() + i);
                continue;
            }

            // Check status
            if (entry.coro.status() == sol::call_status::yielded) {
                // If yielded with sleep value
                if (result.return_count() > 0) {
                    sol::object ret = result[0];
                    if (ret.is<double>()) {
                        entry.wakeTime = currentTime + ret.as<double>();
                    } else {
                        entry.wakeTime = 0.0;
                    }
                } else {
                    entry.wakeTime = 0.0;
                }
                i++;
            } else {
                // Finished
                active_coroutines.erase(active_coroutines.begin() + i);
            }
        }
    }
};

// -------------------------------------------------------------------------
// Lua Thread Manager
// -------------------------------------------------------------------------
// Manages all worker threads and provides the spawn_thread() API
// -------------------------------------------------------------------------

class LuaThreadManager {
private:
    std::vector<std::unique_ptr<LuaWorkerThread>> worker_threads;
    mutable std::mutex threads_mutex;  // mutable to allow locking in const methods
    std::atomic<int> next_thread_id{0};

    // Shared resources
    AtomicSnapshotPtr ui_snapshot;
    SignalIDRegistry signal_id_registry;
    std::atomic<bool>* app_running_ptr{nullptr};

    LuaScriptManager* script_manager{nullptr};

public:
    LuaThreadManager() = default;

    void setAppRunningPtr(std::atomic<bool>* ptr) {
        app_running_ptr = ptr;
    }

    void setScriptManager(LuaScriptManager* manager) {
        script_manager = manager;
    }

    // Spawn a new Lua worker thread
    int spawnThread(sol::protected_function lua_func) {
        int thread_id = next_thread_id.fetch_add(1);

        auto worker = std::make_unique<LuaWorkerThread>(
            thread_id,
            &ui_snapshot,
            &signal_id_registry,
            app_running_ptr
        );

        // Initialize Lua state for this thread
        worker->initializeLuaState(script_manager);

        // Start the OS thread
        worker->is_running.store(true, std::memory_order_release);
        worker->os_thread = std::thread([worker_ptr = worker.get(), lua_func]() mutable {
            worker_ptr->run(lua_func);
        });

        printf("[LuaThreadManager] Spawned thread %d\n", thread_id);

        std::lock_guard<std::mutex> lock(threads_mutex);
        worker_threads.push_back(std::move(worker));

        return thread_id;
    }

    // Spawn a new Lua worker thread from file
    int spawnThreadFile(const std::string& filepath) {
        int thread_id = next_thread_id.fetch_add(1);

        auto worker = std::make_unique<LuaWorkerThread>(
            thread_id,
            &ui_snapshot,
            &signal_id_registry,
            app_running_ptr
        );

        // Initialize Lua state for this thread
        worker->initializeLuaState(script_manager);

        // Start the OS thread with file path
        worker->is_running.store(true, std::memory_order_release);
        worker->os_thread = std::thread([worker_ptr = worker.get(), filepath]() mutable {
            worker_ptr->runFile(filepath);
        });

        printf("[LuaThreadManager] Spawned thread %d from file: %s\n", thread_id, filepath.c_str());

        std::lock_guard<std::mutex> lock(threads_mutex);
        worker_threads.push_back(std::move(worker));

        return thread_id;
    }

    // Update UI snapshot (called by main thread each frame)
    void updateUISnapshot(const UIPlotState& ui_state) {
        UIStateSnapshot* write_snap = ui_snapshot.getWriteSnapshot();

        // Clear and rebuild snapshot
        write_snap->buttons.clear();
        write_snap->toggles.clear();
        write_snap->textInputs.clear();

        // Copy button states
        for (const auto& btn : ui_state.activeButtons) {
            UIStateSnapshot::ButtonState bs;
            bs.title = btn.title;
            bs.clicked = btn.clicked;
            write_snap->buttons.push_back(bs);
        }

        // Copy toggle states
        for (const auto& tog : ui_state.activeToggles) {
            UIStateSnapshot::ToggleState ts;
            ts.title = tog.title;
            ts.state = tog.state;
            write_snap->toggles.push_back(ts);
        }

        // Copy text input states
        for (const auto& txt : ui_state.activeTextInputs) {
            UIStateSnapshot::TextInputState tis;
            tis.title = txt.title;
            tis.text = std::string(txt.textBuffer);
            write_snap->textInputs.push_back(tis);
        }

        // Build lookup maps
        write_snap->buildMaps();

        // Debug: Print snapshot contents occasionally
        static int debug_counter = 0;
        if (debug_counter++ % 300 == 0) {  // Print every ~5 seconds at 60fps
            printf("[LuaThreadManager] UI Snapshot: %zu buttons, %zu toggles, %zu text inputs\n",
                   write_snap->buttons.size(), write_snap->toggles.size(), write_snap->textInputs.size());
            for (const auto& tog : write_snap->toggles) {
                printf("  Toggle: '%s' = %s\n", tog.title.c_str(), tog.state ? "true" : "false");
            }
        }

        // Publish snapshot (atomic swap)
        ui_snapshot.publish();
    }

    // Drain all signal queues and update signal registry
    void drainSignalQueues(std::map<std::string, Signal>& signal_registry, PlaybackMode default_mode) {
        std::lock_guard<std::mutex> lock(threads_mutex);

        for (auto& worker : worker_threads) {
            if (!worker->is_running.load(std::memory_order_acquire)) {
                continue;
            }

            SignalUpdate update;
            while (worker->signal_queue->pop(update)) {
                // Get signal name from ID
                std::string signal_name = signal_id_registry.get_name(update.signal_id);
                if (signal_name.empty()) {
                    continue;
                }

                // Get or create signal
                auto it = signal_registry.find(signal_name);
                if (it == signal_registry.end()) {
                    signal_registry[signal_name] = Signal(signal_name, 10000, default_mode);
                }

                // Add point
                signal_registry[signal_name].AddPoint(update.timestamp, update.value);
            }
        }
    }

    // Process UI events from all threads
    void processUIEvents(UIPlotState& ui_state) {
        std::lock_guard<std::mutex> lock(threads_mutex);

        for (auto& worker : worker_threads) {
            if (!worker->is_running.load(std::memory_order_acquire)) {
                continue;
            }

            UIEvent event;
            while (worker->event_queue->pop(event)) {
                switch (event.type) {
                    case UIEventType::SET_TOGGLE_STATE: {
                        std::string title(event.title);
                        for (auto& tog : ui_state.activeToggles) {
                            if (tog.title == title) {
                                tog.state = event.boolValue;
                                break;
                            }
                        }
                        break;
                    }
                    case UIEventType::SET_TEXT_INPUT: {
                        std::string title(event.title);
                        std::string text(event.textValue);
                        for (auto& txt : ui_state.activeTextInputs) {
                            if (txt.title == title) {
                                strncpy(txt.textBuffer, text.c_str(), sizeof(txt.textBuffer) - 1);
                                txt.textBuffer[sizeof(txt.textBuffer) - 1] = '\0';
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    // Stop all threads
    void stopAllThreads() {
        printf("[LuaThreadManager] Stopping all worker threads...\n");
        std::lock_guard<std::mutex> lock(threads_mutex);

        for (auto& worker : worker_threads) {
            worker->stop();
        }

        worker_threads.clear();
        printf("[LuaThreadManager] All worker threads stopped\n");
    }

    // Get thread count
    size_t getThreadCount() const {
        std::lock_guard<std::mutex> lock(threads_mutex);
        return worker_threads.size();
    }

    // Clear signal ID registry
    void clearSignalRegistry() {
        signal_id_registry.clear();
    }
};
