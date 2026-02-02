# Multi-Threading Architecture Migration

## Overview

The telemetry GUI has been refactored from a single-threaded, coroutine-based architecture to a high-performance multi-threaded design. Each Lua script can now spawn dedicated OS threads for data processing, while the main thread handles UI rendering.

## Key Changes

### New Files Created

1. **[LockFreeQueues.hpp](src/LockFreeQueues.hpp)** - Lock-free SPSC queues for signal updates and UI events
   - `SignalQueue`: Lua threads → Main thread (signal updates)
   - `EventQueue`: Main thread → Lua threads (UI events)
   - `SignalIDRegistry`: Thread-safe mapping of signal names to integer IDs

2. **[UIStateSnapshot.hpp](src/UIStateSnapshot.hpp)** - Read-only UI state snapshot for worker threads
   - Double-buffered snapshot with atomic pointer swap
   - Lua threads read UI state without locks
   - Main thread updates snapshot once per frame

3. **[LuaThreadManager.hpp](src/LuaThreadManager.hpp)** - Worker thread management
   - `LuaWorkerThread`: Individual worker thread with isolated Lua state
   - `LuaThreadManager`: Manages all workers, handles queue draining

4. **[LuaThreadBindings.hpp](src/LuaThreadBindings.hpp)** - Lua bindings for worker threads
   - Minimal API for worker threads (no ImGui access)
   - Coroutine support within each thread
   - Signal update and UI event APIs

### Modified Files

1. **[LuaScriptManager.hpp](src/LuaScriptManager.hpp)**
   - Added `LuaThreadManager thread_manager` member
   - Exposed `spawn_thread()` API to Lua
   - Added methods: `drainSignalQueues()`, `processUIEvents()`, `updateUISnapshot()`

2. **[main.cpp](src/main.cpp)**
   - Modified main event loop to:
     1. Drain signal queues from worker threads
     2. Process UI events from worker threads
     3. Update UI snapshot for worker threads

3. **[TelemetryProject.lua](scripts/parsers/TelemetryProject.lua)**
   - Changed `spawn(function() ... end)` to `spawn_thread(function() ... end)`
   - Now runs in dedicated OS thread

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                       MAIN THREAD (GUI)                      │
│                                                               │
│  1. Drain Signal Queues → Update SignalRegistry              │
│  2. Process UI Events → Update UIPlotState                   │
│  3. Update UI Snapshot (atomic swap)                         │
│  4. Execute Frame Callbacks (main thread Lua coroutines)     │
│  5. Render ImGui/ImPlot                                      │
│                                                               │
│  ┌──────────┐        ┌──────────┐        ┌──────────┐      │
│  │ Signal   │        │ Signal   │        │ Signal   │      │
│  │ Queue 1  │◄───────│ Queue 2  │◄───────│ Queue N  │◄─────┤
│  └──────────┘        └──────────┘        └──────────┘      │
│       │                   │                   │              │
│       ▼                   ▼                   ▼              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │           SignalRegistry (shared)                    │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                               │
│  ┌──────────┐        ┌──────────┐        ┌──────────┐      │
│  │ Event    │───────►│ Event    │───────►│ Event    │─────►│
│  │ Queue 1  │        │ Queue 2  │        │ Queue N  │      │
│  └──────────┘        └──────────┘        └──────────┘      │
│                                                               │
│  ┌─────────────────────────────────────────────────────┐    │
│  │      UIStateSnapshot (atomic pointer)                │    │
│  │      ┌─────────────┐      ┌─────────────┐          │    │
│  │      │  Snapshot A │      │  Snapshot B │          │    │
│  │      │  (reading)  │◄────►│  (writing)  │          │    │
│  │      └─────────────┘      └─────────────┘          │    │
│  └─────────────────────────────────────────────────────┘    │
└───────────────────────┬───────────────────┬──────────────────┘
                        │                   │
        ┌───────────────┘                   └─────────────────┐
        │                                                       │
        ▼                                                       ▼
┌───────────────────┐                               ┌───────────────────┐
│ LUA WORKER THREAD │                               │ LUA WORKER THREAD │
│                   │                               │                   │
│ - Isolated Lua VM │                               │ - Isolated Lua VM │
│ - UDP/TCP/Serial  │                               │ - File Parsing    │
│ - Parse Packets   │                               │ - Transformations │
│ - Push Signals    │                               │ - Push Signals    │
│ - Read UI State   │                               │ - Read UI State   │
│ - Push UI Events  │                               │ - Push UI Events  │
│ - Coroutines OK   │                               │ - Coroutines OK   │
└───────────────────┘                               └───────────────────┘
```

## Data Flow

### Signal Updates (Worker → Main)
1. Worker thread calls `update_signal("signal.name", timestamp, value)`
2. Converts signal name to ID (cached lookup)
3. Pushes `SignalUpdate{id, timestamp, value}` to lock-free queue
4. Main thread drains queue at start of frame
5. Updates `SignalRegistry` (already locked by main thread)
6. Plots render from `SignalRegistry`

### UI Events (Main → Worker)
1. Main thread updates UI state snapshot each frame (atomic swap)
2. Worker thread reads UI state: `get_toggle_state("Connect")`
3. Worker thread writes UI state: `set_toggle_state("Connect", true)`
   - Pushes `UIEvent` to lock-free queue
4. Main thread processes events and updates `UIPlotState`

## API Changes

### For Lua Scripts

#### New API
- `spawn_thread(function)` - Spawn a new OS thread with isolated Lua VM
  - Returns thread ID (integer)
  - Function runs in dedicated thread
  - Has its own signal/event queues

#### Unchanged API (works in both main thread and worker threads)
- `spawn(function)` - Spawn coroutine (within current thread)
- `yield([seconds])` - Yield to scheduler
- `sleep(seconds)` - Sleep (yields with timeout)
- `update_signal(name, timestamp, value)` - Push signal update
- `get_signal_id(name)` - Get cached signal ID
- `update_signal_fast(id, timestamp, value)` - Push using ID
- `get_toggle_state(title)` - Read UI state (from snapshot)
- `get_text_input(title)` - Read UI state (from snapshot)
- `get_button_clicked(title)` - Read UI state (from snapshot)
- `set_toggle_state(title, state)` - Write UI state (to event queue)
- `is_app_running()` - Check if app is running
- `get_time_seconds()` - Get current time
- `log(message)` - Print log message

#### Removed from Worker Threads
- `on_frame(callback)` - Only works on main thread
- `on_alert(name, condition, action)` - Only works on main thread
- All ImGui functions - Only accessible from main thread

## Performance Benefits

1. **True Parallelism**: Data sources (UDP, Serial, CAN) run in parallel
2. **No Blocking**: Worker threads can block on I/O without freezing GUI
3. **Lock-Free**: Signal updates use lock-free queues (no contention)
4. **Cache Locality**: Each thread has its own L1/L2 cache working set
5. **Scalability**: Easy to add more data sources (just spawn more threads)

## Testing

### Minimal Test Case

Create a simple test script:

```lua
-- test_threading.lua
print("Test script loaded (main thread)")

-- Spawn a worker thread
spawn_thread(function()
    print("Worker thread started")

    -- Cache signal IDs
    local sig_id = get_signal_id("Test.value")
    print("Cached signal ID: " .. sig_id)

    -- Generate test data
    local t = 0
    while is_app_running() do
        -- Push 1000 updates per frame
        for i = 1, 1000 do
            update_signal_fast(sig_id, t, math.sin(t))
            t = t + 0.001
        end

        -- Check UI state
        if get_toggle_state("Stop") then
            print("Stop button pressed")
            break
        end

        yield()  -- Let other coroutines run
    end

    print("Worker thread exiting")
end)

print("Test script initialized")
```

### Expected Behavior

1. Script loads on main thread (instantaneous)
2. Worker thread spawns (see console message)
3. Worker generates 60,000 points/sec (1000 points * 60 fps)
4. GUI remains responsive
5. No mutex contention

### Monitoring

Check console output for:
- `[LuaThreadManager] Spawned thread N`
- `[LuaWorkerThread N] Thread started`
- `[LuaWorkerThread N] Lua state initialized`
- Signal update rates in TelemetryProject.lua

## Debugging

### Common Issues

1. **No data appearing**: Check signal queue isn't full (65536 limit)
   - Solution: Increase `SIGNAL_QUEUE_SIZE` or reduce update rate

2. **UI events not processed**: Check event queue isn't full (1024 limit)
   - Solution: Increase `EVENT_QUEUE_SIZE`

3. **Crashes on exit**: Ensure `stopAllLuaThreads()` is called
   - Should happen automatically in destructor

4. **Signal ID not found**: Signal name mismatch
   - Check console for `[Parser] ERROR: Signal ID not found`

### Debug Output

Enable verbose logging by uncommenting printf statements in:
- `LuaThreadManager::drainSignalQueues()`
- `LuaThreadManager::processUIEvents()`
- `LuaWorkerThread::pushSignalUpdate()`

## Migration Guide

### For Existing Scripts

To migrate a script to use worker threads:

**Before:**
```lua
spawn(function()
    -- Data processing loop
    while is_app_running() do
        -- Process data
        yield()
    end
end)
```

**After:**
```lua
spawn_thread(function()
    -- Data processing loop
    while is_app_running() do
        -- Process data
        yield()  -- Still works! (within this thread)
    end
end)
```

### When to Use Worker Threads

Use `spawn_thread()` when:
- Doing I/O (UDP, TCP, Serial, CAN, File)
- Heavy computation (parsing, transforms)
- Need parallel processing

Use `spawn()` (coroutines) when:
- UI logic (frame callbacks, alerts)
- Lightweight async tasks
- Need to access ImGui state

## Future Enhancements

1. **Thread Affinity**: Pin threads to specific CPU cores
2. **Priority Scheduling**: Real-time priority for critical data sources
3. **Backpressure**: Slow down sources when GUI can't keep up
4. **Statistics**: Per-thread CPU usage, queue depth monitoring
5. **Hot Reload**: Restart worker threads without restarting app

## Notes

- Worker threads cannot call ImGui functions (not thread-safe)
- Each worker thread has ~2MB overhead (Lua VM + queues)
- Signal ID registry is thread-safe (uses mutex)
- UI snapshot updates are atomic (no mutex needed for reads)
- Coroutines still work within each worker thread

## Backward Compatibility

- Old scripts using `spawn()` still work (run on main thread)
- Frame callbacks and alerts unchanged
- Signal API unchanged (just faster with caching)
- UI state access unchanged (just safer with snapshot)

---

**Questions or issues?** Check console output for detailed error messages.
