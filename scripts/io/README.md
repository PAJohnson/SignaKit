# Lua I/O Scripts (Tier 5 - Total Luafication)

This directory contains Lua scripts that handle network and data I/O operations, replacing the legacy C++ networking classes.

## Overview

With Tier 5 implementation, all network I/O is handled by Lua scripts running in the **Main GUI Thread** via frame callbacks. This provides flexibility for different protocols (UDP, custom) without recompiling the C++ application, while ensuring thread safety with the GUI.

## Architecture

```
┌───────────────────────────────────────────────┐
│  Main GUI Thread (60 FPS)                     │
│                                               │
│  ┌─────────────────────────────────────────┐  │
│  │  Lua I/O Script (e.g. DataSource.lua)   │  │
│  │  - Runs via on_frame() callback         │  │
│  │  - Manages non-blocking sockets         │  │
│  │  - Receives raw bytes                   │  │
│  │  - Calls parse_packet()                 │  │
│  └─────────────────────────────────────────┘  │
└───────────────────────────────────────────────┘
```

## Available Scripts

### Production Scripts

#### `UDPDataSink.lua` / `DataSource.lua`
- **Purpose**: UDP packet reception and parsing
- **Features**:
  - Non-blocking UDP socket reception using `LuaUDPSocket`
  - Configurable via GUI controls (IP, Port)
  - Connect/Disconnect logic
  - Packet logging
- **Usage**: Primary script for standard UDP telemetry
- **Status**: Production-ready

### Testing Scripts

#### `benchmark.lua`
- **Purpose**: Measure Lua socket performance
- **Features**:
  - Packet reception rate measurement
  - Throughput calculation (MB/s)

## Lua API Reference

### Socket API (LuaUDPSocket)

The system uses a built-in `LuaUDPSocket` class (wrapper around `sockpp`) instead of the external `socket` library.

```lua
-- Create a UDP socket
local udp = create_udp_socket()

-- Bind to port (required for receiving)
if udp:bind("0.0.0.0", 12345) then
    -- Set non-blocking mode (CRITICAL for main thread)
    udp:set_non_blocking(true)
end

-- Receive data
local data, err = udp:receive(1024)
if data then
    -- Process data...
end

-- Close socket
udp:close()

-- Check status
if udp:is_open() then ... end
```

### Signal Manipulation

```lua
-- Update a signal (creates if doesn't exist)
update_signal("signal_name", timestamp, value)

-- Create a signal
create_signal("signal_name")
```

### GUI Control Access (Tier 4)

```lua
-- Read text input value
local ip = get_text_input("UDP IP") or "127.0.0.1"

-- Check if button was clicked
if get_button_clicked("UDP Connect") then
    -- Handle connect
end

-- Read toggle state
local enabled = get_toggle_state("Enable Logging")
```

### Timing Utilities

```lua
-- Get current time in seconds (high-resolution)
local currentTime = get_time_seconds()

-- Get delta time from last frame
local dt = get_delta_time()
```

## Writing Custom I/O Scripts

### Template Structure

```lua
-- 1. State variables
local udp_socket = nil
local active = false

-- 2. Configuration strings
local BIND_IP = "0.0.0.0"
local BIND_PORT = 12345

-- 3. Initialization
local function init_network()
    if udp_socket then udp_socket:close() end
    udp_socket = create_udp_socket()
    
    if udp_socket:bind(BIND_IP, BIND_PORT) then
        udp_socket:set_non_blocking(true)
        active = true
        log("Network initialized on " .. BIND_PORT)
    else
        log("Failed to bind port " .. BIND_PORT)
        active = false
    end
end

-- 4. Cleanup handler
on_cleanup(function()
    if udp_socket then
        udp_socket:close()
        udp_socket = nil
    end
end)

-- 5. Main Frame Loop
on_frame(function()
    if not active then return end

    -- Drain socket (read until empty or limit reached)
    local packets_read = 0
    while packets_read < 100 do
        local data, err = udp_socket:receive(65536)
        if data and #data > 0 then
            -- Parse data
            parse_packet(data, #data)
            packets_read = packets_read + 1
        else
            break -- No more data
        end
    end
end)

-- 6. Start
init_network()
```

### Best Practices

1.  **Non-blocking I/O**: Always call `set_non_blocking(true)`. Blocking calls will freeze the entire GUI.
2.  **Socket Draining**: In `on_frame`, read multiple packets (loop) until `receive` returns no data, to avoid building up a backlog.
3.  **Cleanup**: Always register `on_cleanup()` to close sockets on script reload.
4.  **Error Handling**: Handle binding failures gracefully (e.g. port already in use).

## Troubleshooting

### Socket creation fails
- Check if another application (or previous script instance) is holding the port.
- Verify `create_udp_socket` returns a valid object.

### No packets received
- Verify sender IP/Port.
- Check firewall settings.
- Ensure `bind` was successful.

### GUI Freezes
- **Cause**: Socket is blocking.
- **Fix**: Ensure `udp:set_non_blocking(true)` is called immediately after creation/binding.

## Future Enhancements

- Serial port support
- Multi-threaded processing (Tier 6)
