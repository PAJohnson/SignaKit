# Lua I/O Scripts (Tier 5 - Total Luafication)

This directory contains Lua scripts that handle network and data I/O operations, replacing the C++ `NetworkReceiverThread` and `DataSink` classes.

## Overview

With Tier 5 implementation, all network I/O is handled by Lua scripts running in separate threads. This provides ultimate flexibility for different protocols (UDP, TCP, HTTP, Serial, custom) without recompiling the C++ application.

## Architecture

```
┌─────────────────────────────────────────┐
│ C++ Core                                │
│ • Rendering (ImGui/SDL)                 │
│ • Signal registry management            │
│ • Lua script execution                  │
│ • Thread management API                 │
└─────────────────────────────────────────┘
        ↕ (Signal Registry + Mutex)
┌─────────────────────────────────────────┐
│ Lua I/O Thread (scripts/io/*.lua)      │
│ • Socket operations (LuaSocket)         │
│ • Data parsing                          │
│ • Signal updates                        │
│ • Connection management                 │
└─────────────────────────────────────────┘
```

## Available Scripts

### Production Scripts

#### `UDPDataSink.lua`
- **Purpose**: UDP packet reception and parsing
- **Replaces**: C++ `NetworkReceiverThread` + `UDPDataSink` classes
- **Features**:
  - Non-blocking UDP socket reception
  - Configurable via GUI controls (IP, Port)
  - Connect/Disconnect buttons
  - Packet logging (optional)
  - Performance statistics
- **Usage**: Primary script for standard UDP telemetry
- **Status**: Production-ready

#### `default_control_panel.lua`
- **Purpose**: Sets up default UDP control GUI elements
- **Requires**: Tier 4 GUI control elements
- **Creates**: IP/Port text inputs, Connect/Disconnect buttons
- **Status**: Instructions only (programmatic creation pending)

### Example Scripts

#### `TCPDataSink.lua`
- **Purpose**: TCP streaming telemetry reception
- **Features**:
  - TCP connection management
  - Auto-reconnect logic
  - Buffered packet assembly
  - Example: Newline-delimited JSON parsing
- **Usage**: Template for TCP-based protocols

#### `HTTPPoller.lua`
- **Purpose**: REST API polling at intervals
- **Features**:
  - Simple HTTP GET implementation
  - Configurable poll interval
  - Error handling and retry logic
  - Example: JSON API response parsing
- **Usage**: Template for HTTP-based telemetry

### Testing Scripts

#### `socket_test.lua`
- **Purpose**: Validate LuaSocket integration
- **Tests**:
  - Socket module loading
  - UDP socket creation/closure
  - TCP socket creation/closure
  - Method availability
- **Usage**: Run once to verify installation

#### `benchmark.lua`
- **Purpose**: Measure Lua socket performance
- **Target**: 10 MB/s throughput
- **Features**:
  - Packet reception rate measurement
  - Throughput calculation (MB/s)
  - Comparison with C++ implementation
- **Usage**: Run with `mock_device` sending to port 12346

## Lua API Reference

### Threading API

```lua
-- Create a background I/O thread
threadId = create_thread(function()
    while is_app_running() do
        -- I/O operations
        sleep_ms(10)
    end
end)

-- Stop a specific thread
stop_thread(threadId)

-- Sleep for N milliseconds
sleep_ms(100)

-- Check if application is still running
if is_app_running() then
    -- Continue processing
end
```

### Socket API (LuaSocket)

```lua
local socket = require("socket")

-- UDP socket
local udp = socket.udp()
udp:setsockname("127.0.0.1", 12345)
udp:settimeout(0)  -- Non-blocking
local data, err = udp:receive()
udp:close()

-- TCP socket
local tcp = socket.tcp()
tcp:connect("192.168.1.100", 8080)
tcp:settimeout(0)  -- Non-blocking
local data, err = tcp:receive("*a")
tcp:close()
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
```

## Writing Custom I/O Scripts

### Template Structure

```lua
-- 1. Load required modules
local socket = require("socket")

-- 2. Define configuration
local SERVER = "192.168.1.100"
local PORT = 8080

-- 3. Connection management functions
local function connect()
    -- Create socket
    -- Configure socket
    -- Return success/failure
end

local function disconnect()
    -- Close socket
    -- Cleanup resources
end

-- 4. Main I/O loop
local function io_loop()
    while is_app_running() do
        -- Check GUI controls
        -- Receive data
        -- Parse data
        -- Update signals
        -- Sleep to avoid CPU spinning
    end
    disconnect()
end

-- 5. Cleanup handler
on_cleanup(function()
    disconnect()
end)

-- 6. Start I/O thread
create_thread(io_loop)
```

### Best Practices

1. **Non-blocking I/O**: Always set `socket:settimeout(0)` for non-blocking mode
2. **CPU usage**: Call `sleep_ms()` in loops to avoid 100% CPU usage
3. **Error handling**: Use `pcall()` for socket operations and parser calls
4. **Thread safety**: Signal updates are thread-safe (mutex-protected in C++)
5. **Cleanup**: Always register `on_cleanup()` to close sockets on script reload
6. **Reconnection**: Implement auto-reconnect logic for production use

### Performance Considerations

- **Target**: 10 MB/s (100,000 packets/sec @ 100 bytes)
- **Lua overhead**: ~2-5 µs per packet (acceptable for target)
- **LuaJIT**: Can be used for ~10x speedup if needed
- **Sleep tuning**: Balance CPU usage vs latency (1-10ms recommended)

## Troubleshooting

### Socket creation fails
- Check that LuaSocket is properly linked (run `socket_test.lua`)
- Verify `luaopen_socket_core` is registered in C++

### No packets received
- Verify mock_device is sending to correct IP:Port
- Check firewall settings
- Enable packet logging to verify data arrival

### High CPU usage
- Ensure `sleep_ms()` is called in I/O loop
- Check for tight loops without blocking operations

### Thread crashes
- Each thread gets its own Lua state (thread-safe)
- Don't share Lua objects between threads
- Use `pcall()` for error isolation

## Migration from C++ NetworkReceiverThread

To migrate from C++ to Lua I/O:

1. **Phase 1**: Test with `UDPDataSink.lua` running alongside C++ (both active)
2. **Phase 2**: Disable C++ NetworkReceiverThread
3. **Phase 3**: Remove C++ network code entirely
4. **Phase 4**: Customize Lua scripts for your protocol

See `docs/LuaTotalLuafication.md` for complete migration guide.

## Future Enhancements

- Serial port support (requires `luars232` or similar library)
- ZeroMQ support (requires `lzmq` library)
- WebSocket support (requires `lua-websockets` library)
- Multi-protocol support (multiple I/O scripts running concurrently)

## References

- [LuaSocket Documentation](http://w3.impa.br/~diego/software/luasocket/)
- [Tier 5 Implementation Plan](../../docs/Todo.md#tier-5---total-luafication)
- [Complete API Documentation](../../docs/LuaTotalLuafication.md)
