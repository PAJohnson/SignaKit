# Lua Cleanup Callbacks (Tier 4.5)

## Overview

Cleanup callbacks allow Lua scripts to properly release resources (sockets, file handles, timers, etc.) when scripts are reloaded or unloaded. This prevents resource leaks and ensures graceful shutdown of persistent connections.

## When Cleanup Callbacks Are Executed

Cleanup callbacks are automatically called when:
- **Reload All Scripts** is triggered from the Scripts menu
- Scripts are being reloaded programmatically

Cleanup happens **before** the Lua state is cleared and **before** new scripts are loaded.

## API

### `on_cleanup(function)`

Registers a function to be called during script cleanup.

**Parameters**:
- `function` - Lua function to execute during cleanup (takes no parameters)

**Example**:
```lua
on_cleanup(function()
    log("Cleaning up resources...")
    -- Close sockets, files, etc.
end)
```

**Important Notes**:
- You can register multiple cleanup callbacks in a single script
- All cleanup callbacks are executed in the order they were registered
- Cleanup callbacks should complete quickly (avoid long-running operations)
- Errors in cleanup callbacks are logged but don't prevent other cleanups from running

---

## Complete Examples

### Example 1: Network Socket Cleanup

```lua
-- File: scripts/callbacks/network_client.lua
-- Manages a persistent network connection with proper cleanup

local socket = require("socket")
local tcp_socket = nil
local connected = false

-- Initialize connection
local function connect_to_server()
    tcp_socket = socket.tcp()
    tcp_socket:settimeout(0)  -- Non-blocking

    local success, err = tcp_socket:connect("127.0.0.1", 5000)
    if success then
        connected = true
        log("Connected to server at 127.0.0.1:5000")
    else
        connected = false
        log("Failed to connect: " .. tostring(err))
    end
end

-- Register cleanup callback
on_cleanup(function()
    log("[Cleanup] Closing network socket...")
    if tcp_socket then
        tcp_socket:close()
        tcp_socket = nil
        connected = false
    end
end)

-- Initialize on load
connect_to_server()

-- Frame callback to send data
on_frame(function()
    if connected and get_button_clicked("Send Data") then
        local data = get_text_input("Data Input")
        if data then
            local bytes, err = tcp_socket:send(data .. "\n")
            if err then
                log("Send error: " .. err)
                connected = false
            else
                log("Sent: " .. data)
            end
        end
    end

    -- Try to reconnect if disconnected
    if not connected and get_button_clicked("Reconnect") then
        connect_to_server()
    end
end)
```

### Example 2: File Handle Cleanup

```lua
-- File: scripts/callbacks/data_logger.lua
-- Logs telemetry data to a file with proper cleanup

local log_file = nil
local logging_enabled = false

-- Open log file
local function open_log_file()
    log_file = io.open("telemetry_log.csv", "a")
    if log_file then
        logging_enabled = true
        log("Log file opened")
        -- Write CSV header if file is new
        log_file:write("timestamp,altitude,velocity\n")
        log_file:flush()
    else
        log("Failed to open log file")
    end
end

-- Cleanup: Close file handle
on_cleanup(function()
    log("[Cleanup] Closing log file...")
    if log_file then
        log_file:flush()
        log_file:close()
        log_file = nil
        logging_enabled = false
    end
end)

-- Initialize
open_log_file()

-- Frame callback to write data
on_frame(function()
    if logging_enabled and get_toggle_state("Enable Logging") then
        local alt = get_signal("GPS.altitude")
        local vel = get_signal("GPS.velocity")

        if alt and vel then
            local timestamp = get_frame_number() * get_delta_time()
            log_file:write(string.format("%.3f,%.2f,%.2f\n", timestamp, alt, vel))

            -- Flush every 10 frames to avoid data loss
            if get_frame_number() % 10 == 0 then
                log_file:flush()
            end
        end
    end
end)
```

### Example 3: Multiple Resource Cleanup

```lua
-- File: scripts/callbacks/multi_resource.lua
-- Demonstrates managing multiple resources with separate cleanup callbacks

local socket = require("socket")
local udp_socket = nil
local log_file = nil
local timer_id = 12345  -- Hypothetical timer ID

-- Initialize UDP socket
local function init_udp()
    udp_socket = socket.udp()
    udp_socket:settimeout(0)
    udp_socket:setsockname("*", 0)
    log("UDP socket initialized")
end

-- Initialize log file
local function init_log()
    log_file = io.open("commands.log", "a")
    log("Command log opened")
end

-- Register separate cleanup for each resource
on_cleanup(function()
    log("[Cleanup] Closing UDP socket...")
    if udp_socket then
        udp_socket:close()
        udp_socket = nil
    end
end)

on_cleanup(function()
    log("[Cleanup] Closing log file...")
    if log_file then
        log_file:close()
        log_file = nil
    end
end)

on_cleanup(function()
    log("[Cleanup] Stopping timer " .. timer_id)
    -- Hypothetical timer cleanup
    -- stop_timer(timer_id)
end)

-- Initialize all resources
init_udp()
init_log()

on_frame(function()
    -- Use resources here
    if get_button_clicked("Send Command") then
        local cmd = get_text_input("Command")
        if udp_socket and cmd then
            udp_socket:sendto(cmd, "192.168.1.100", 5000)
            if log_file then
                log_file:write(os.date() .. ": " .. cmd .. "\n")
                log_file:flush()
            end
        end
    end
end)
```

### Example 4: State Persistence Between Reloads

```lua
-- File: scripts/callbacks/persistent_counter.lua
-- Demonstrates saving state before reload and restoring it

local counter = counter or 0  -- Persist across reloads using global
local state_file = "counter_state.txt"

-- Load previous state on startup
local function load_state()
    local file = io.open(state_file, "r")
    if file then
        local saved_count = file:read("*n")
        if saved_count then
            counter = saved_count
            log("Restored counter: " .. counter)
        end
        file:close()
    end
end

-- Save state before cleanup
on_cleanup(function()
    log("[Cleanup] Saving counter state: " .. counter)
    local file = io.open(state_file, "w")
    if file then
        file:write(tostring(counter))
        file:close()
    end
end)

-- Load state on first run
load_state()

on_frame(function()
    if get_button_clicked("Increment") then
        counter = counter + 1
        log("Counter: " .. counter)
    end

    if get_button_clicked("Reset") then
        counter = 0
        log("Counter reset")
    end
end)
```

### Example 5: Graceful Connection Shutdown

```lua
-- File: scripts/callbacks/graceful_shutdown.lua
-- Sends a disconnect message before closing the connection

local socket = require("socket")
local tcp_socket = nil
local connected = false

-- Connect to server
local function connect()
    tcp_socket = socket.tcp()
    tcp_socket:settimeout(0)

    if tcp_socket:connect("127.0.0.1", 5000) then
        connected = true
        -- Send hello message
        tcp_socket:send("HELLO\n")
        log("Connected and sent HELLO")
    end
end

-- Cleanup with graceful disconnect
on_cleanup(function()
    log("[Cleanup] Disconnecting gracefully...")
    if tcp_socket and connected then
        -- Send goodbye message before closing
        tcp_socket:settimeout(1)  -- Brief blocking for final send
        tcp_socket:send("GOODBYE\n")

        -- Give server time to receive
        socket.sleep(0.1)

        tcp_socket:close()
        log("Disconnected")
    end
    tcp_socket = nil
    connected = false
end)

connect()

on_frame(function()
    if connected then
        -- Heartbeat every 60 frames
        if get_frame_number() % 60 == 0 then
            tcp_socket:send("PING\n")
        end
    end
end)
```

---

## Best Practices

### 1. Always Register Cleanup for Persistent Resources

If your script creates any of these, register a cleanup callback:
- Network sockets (TCP, UDP)
- File handles
- Timers or scheduled tasks
- External processes
- Shared memory or locks

```lua
-- ✅ GOOD: Register cleanup
local file = io.open("data.log", "w")
on_cleanup(function()
    if file then file:close() end
end)

-- ❌ BAD: No cleanup - file handle leaked on reload
local file = io.open("data.log", "w")
```

### 2. Check for nil Before Cleanup

Always check if resources exist before cleaning them up:

```lua
on_cleanup(function()
    if tcp_socket then
        tcp_socket:close()
        tcp_socket = nil
    end
end)
```

### 3. Set Resources to nil After Cleanup

Prevent double-cleanup by setting variables to nil:

```lua
on_cleanup(function()
    if file then
        file:close()
        file = nil  -- Prevent double-close
    end
end)
```

### 4. Cleanup Should Be Fast

Avoid long-running operations in cleanup callbacks:

```lua
-- ✅ GOOD: Quick cleanup
on_cleanup(function()
    socket:close()
end)

-- ❌ BAD: Long-running operation
on_cleanup(function()
    -- Don't do this - blocks script reload
    for i = 1, 1000000 do
        process_item(i)
    end
end)
```

### 5. Multiple Cleanup Callbacks Are OK

You can register multiple cleanup callbacks for different resources:

```lua
on_cleanup(function()
    -- Clean up network
    if socket then socket:close() end
end)

on_cleanup(function()
    -- Clean up file
    if file then file:close() end
end)

on_cleanup(function()
    -- Save state
    save_config()
end)
```

### 6. Error Handling in Cleanup

Wrap cleanup operations in pcall if they might fail:

```lua
on_cleanup(function()
    local success, err = pcall(function()
        tcp_socket:send("GOODBYE\n")
        tcp_socket:close()
    end)

    if not success then
        log("Cleanup error: " .. tostring(err))
    end

    tcp_socket = nil
end)
```

---

## Common Use Cases

### 1. Network Communication
- Close TCP/UDP sockets
- Send disconnect messages
- Clean up connection pools

### 2. File I/O
- Flush and close log files
- Close data recorders
- Save configuration

### 3. State Management
- Save persistent state to disk
- Update checkpoint files
- Write session summaries

### 4. External Resources
- Stop timers
- Terminate child processes
- Release hardware resources

---

## Troubleshooting

### Cleanup callback not called

**Problem**: Resources not being cleaned up on reload

**Solution**:
- Ensure you're calling `on_cleanup()` at the script's top level, not inside other functions
- Check the console for cleanup messages when reloading

### Resources still leaking

**Problem**: Files/sockets still open after reload

**Solution**:
- Verify your cleanup function is actually closing resources
- Check that resource variables are in scope (use module-level locals, not function locals)
- Add log statements to confirm cleanup is running

### Cleanup takes too long

**Problem**: Script reload is slow

**Solution**:
- Remove long-running operations from cleanup
- Reduce network timeouts in cleanup code
- Move heavy operations to separate background tasks

---

## See Also

- [LuaFrameCallbacks.md](LuaFrameCallbacks.md) - Tier 3: Frame Callbacks & Alerts
- [LuaControlElements.md](LuaControlElements.md) - Tier 4: GUI Control Elements
- [LuaScripting.md](LuaScripting.md) - Tier 1: Signal Transforms
