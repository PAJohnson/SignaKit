# Lua Packet Parsing Guide (Tier 2)

## Overview

**Tier 2** of Lua scripting enables **custom packet parsers** written entirely in Lua. This allows you to handle any packet format (binary, JSON, CSV, Protobuf, encrypted, etc.) without recompiling the GUI.

The C++ I/O layer (UDP/TCP/Serial) handles raw socket operations, while Lua scripts parse the received bytes and update signals.

---

## Architecture

```
┌─────────────────────────────────────────┐
│  C++ I/O Layer (UDP/TCP/Serial)         │
│  - Socket management                    │
│  - Raw byte buffer reception            │
│  - Thread safety (stateMutex)           │
└──────────────┬──────────────────────────┘
               │ Raw bytes (buffer, length)
               ▼
┌─────────────────────────────────────────┐
│  Lua Parser Chain                       │
│  - Try each registered parser in order  │
│  - First parser that returns true wins  │
│  - Parse bytes, extract fields           │
└──────────────┬──────────────────────────┘
               │ update_signal(name, time, value)
               ▼
┌─────────────────────────────────────────┐
│  Signal Registry (C++)                  │
│  - Shared state for plotting/analysis   │
└─────────────────────────────────────────┘
```

---

## Lua Parser API

### Buffer Parsing Functions

All functions handle endianness explicitly and perform bounds checking.

#### Integer Readers

```lua
-- Unsigned integers
readUInt8(buffer, offset)                       -- Returns 0-255
readUInt16(buffer, offset, littleEndian=true)   -- Returns 0-65535
readUInt32(buffer, offset, littleEndian=true)   -- Returns 0-4294967295
readUInt64(buffer, offset, littleEndian=true)   -- Returns 0-2^64-1

-- Signed integers
readInt8(buffer, offset)                        -- Returns -128 to 127
readInt16(buffer, offset, littleEndian=true)    -- Returns -32768 to 32767
readInt32(buffer, offset, littleEndian=true)    -- Returns -2^31 to 2^31-1
readInt64(buffer, offset, littleEndian=true)    -- Returns -2^63 to 2^63-1
```

**Parameters:**
- `buffer` (string): Packet buffer (Lua string containing raw bytes)
- `offset` (number): Byte offset (0-indexed)
- `littleEndian` (boolean, optional): `true` for little-endian (default), `false` for big-endian

**Returns:**
- `number` on success, `nil` if offset out of bounds

---

#### Floating Point Readers

```lua
readFloat(buffer, offset, littleEndian=true)    -- IEEE 754 single-precision (32-bit)
readDouble(buffer, offset, littleEndian=true)   -- IEEE 754 double-precision (64-bit)
```

**Example:**
```lua
local temperature = readFloat(buffer, 12, true)  -- Little-endian float at offset 12
local timestamp = readDouble(buffer, 4, false)   -- Big-endian double at offset 4
```

---

#### String Readers

```lua
-- Fixed-length string (can contain null bytes)
readString(buffer, offset, length)

-- Null-terminated C-style string
readCString(buffer, offset)
```

**Example:**
```lua
local header = readString(buffer, 0, 4)          -- Read 4-byte header
local name = readCString(buffer, 20)             -- Read until null terminator
```

---

#### Buffer Utilities

```lua
getBufferLength(buffer)                          -- Returns total buffer size
getBufferByte(buffer, index)                     -- Returns single byte (0-255)
bytesToHex(buffer, offset, length)               -- Returns hex string for debugging
```

**Example:**
```lua
local len = getBufferLength(buffer)
local firstByte = getBufferByte(buffer, 0)
local hexDump = bytesToHex(buffer, 0, 16)        -- "4D 54 52 00 12 34 56 78..."
log("Packet hex: " .. hexDump)
```

---

### Signal Manipulation Functions

```lua
-- Update or create a signal with a new data point
update_signal(signalName, timestamp, value)

-- Pre-create a signal (optional, update_signal creates automatically)
create_signal(signalName)
```

**Example:**
```lua
update_signal("IMU.accelX", 12345.67, 9.81)      -- Add point (12345.67, 9.81)
update_signal("GPS.latitude", time, 37.7749)     -- Create if doesn't exist
```

---

### Tier 1 Integration Function

```lua
-- Trigger Tier 1 on_packet() callbacks for transforms
trigger_packet_callbacks(packetType)
```

**Purpose:** After parsing a packet and updating signals, call this to execute any Tier 1 transform callbacks registered with `on_packet()`. This ensures backward compatibility with existing transform scripts.

**Parameters:**
- `packetType` (string): Packet type identifier (e.g., "IMU", "GPS", "Battery")

**Example:**
```lua
register_parser("my_parser", function(buffer, length)
    -- Parse packet and update signals
    local time = readDouble(buffer, 4, true)
    local value = readFloat(buffer, 12, true)
    update_signal("MyData.value", time, value)

    -- Trigger any on_packet("MyData", ...) transform callbacks
    trigger_packet_callbacks("MyData")

    return true
end)
```

**When to use:** Call this at the end of your parser (after updating all signals) if you want Tier 1 transforms to run. The `legacy_binary.lua` parser calls this for all packet types to maintain full backward compatibility.

---

### Parser Registration

```lua
register_parser(parserName, parserFunction)
```

**Parameters:**
- `parserName` (string): Name for debugging (shown in console logs)
- `parserFunction` (function): Parser function with signature `function(buffer, length) -> bool`

**Parser Function Contract:**
- Receives `buffer` (Lua string) and `length` (number of bytes)
- Returns `true` if packet was successfully parsed, `false` otherwise
- First parser to return `true` stops the chain (others are not tried)
- Should perform bounds checking and handle errors gracefully

**Example:**
```lua
register_parser("my_protocol", function(buffer, length)
    -- Bounds check
    if length < 16 then
        return false
    end

    -- Check header
    local header = readString(buffer, 0, 3)
    if header ~= "XYZ" then
        return false  -- Not our packet
    end

    -- Parse fields
    local time = readDouble(buffer, 4, true)
    local value = readFloat(buffer, 12, true)

    -- Update signals
    update_signal("MyProtocol.value", time, value)

    return true  -- Packet handled
end)
```

---

## Complete Example: Custom Binary Protocol

```lua
-- Custom telemetry protocol parser
log("Loading custom telemetry parser")

register_parser("custom_telemetry", function(buffer, length)
    -- Packet format:
    -- [0-3]   Header: "CUST" (4 bytes)
    -- [4-11]  Timestamp: double (8 bytes)
    -- [12]    Packet type: uint8 (1 byte)
    -- [13+]   Payload (variable)

    -- Validate minimum size
    if length < 13 then
        return false
    end

    -- Check header
    local header = readString(buffer, 0, 4)
    if header ~= "CUST" then
        return false
    end

    -- Read common fields
    local timestamp = readDouble(buffer, 4, true)
    local packetType = readUInt8(buffer, 12)

    if not timestamp or not packetType then
        return false
    end

    -- Parse based on packet type
    if packetType == 1 then
        -- IMU data (requires 25 bytes total)
        if length < 25 then return false end

        local accelX = readFloat(buffer, 13, true)
        local accelY = readFloat(buffer, 17, true)
        local accelZ = readFloat(buffer, 21, true)

        if accelX and accelY and accelZ then
            update_signal("Custom.IMU.accelX", timestamp, accelX)
            update_signal("Custom.IMU.accelY", timestamp, accelY)
            update_signal("Custom.IMU.accelZ", timestamp, accelZ)
            return true
        end

    elseif packetType == 2 then
        -- GPS data (requires 37 bytes total)
        if length < 37 then return false end

        local lat = readDouble(buffer, 13, true)
        local lon = readDouble(buffer, 21, true)
        local alt = readFloat(buffer, 29, true)
        local speed = readFloat(buffer, 33, true)

        if lat and lon and alt and speed then
            update_signal("Custom.GPS.latitude", timestamp, lat)
            update_signal("Custom.GPS.longitude", timestamp, lon)
            update_signal("Custom.GPS.altitude", timestamp, alt)
            update_signal("Custom.GPS.speed", timestamp, speed)
            return true
        end

    elseif packetType == 3 then
        -- Battery data (requires 21 bytes total)
        if length < 21 then return false end

        local voltage = readFloat(buffer, 13, true)
        local current = readFloat(buffer, 17, true)

        if voltage and current then
            update_signal("Custom.Battery.voltage", timestamp, voltage)
            update_signal("Custom.Battery.current", timestamp, current)
            update_signal("Custom.Battery.power", timestamp, voltage * current)
            return true
        end
    end

    -- Unknown packet type
    log("Warning: Unknown packet type " .. tostring(packetType))
    return false
end)

log("Custom telemetry parser registered")
```

---

## Example: Encrypted Data

```lua
-- Simple XOR encryption example
register_parser("encrypted_telemetry", function(buffer, length)
    -- Check header (unencrypted)
    local header = readString(buffer, 0, 4)
    if header ~= "ENCR" then
        return false
    end

    -- XOR key (simple example - use proper crypto in production!)
    local key = 0x5A

    -- Decrypt payload (bytes 4 onwards)
    local decrypted = {}
    for i = 4, length - 1 do
        local byte = getBufferByte(buffer, i)
        if byte then
            decrypted[i - 4] = string.char(bit.bxor(byte, key))
        end
    end

    local decryptedBuffer = table.concat(decrypted)

    -- Now parse decrypted buffer
    -- (In real implementation, you'd call another parser or parse directly)

    return true
end)
```

---

## Endianness Handling

Network protocols often use **big-endian** (network byte order), while most systems are **little-endian**.

**Examples:**
```lua
-- Network protocol (big-endian)
local value = readUInt32(buffer, 0, false)  -- false = big-endian

-- Local binary file (little-endian, typical for x86/ARM)
local value = readUInt32(buffer, 0, true)   -- true = little-endian (default)

-- Auto-detect based on magic number
local magic = readUInt32(buffer, 0, true)
if magic == 0x12345678 then
    -- Little-endian
elseif magic == 0x78563412 then
    -- Big-endian, swap all future reads
end
```

---

## Error Handling Best Practices

### 1. Bounds Checking
```lua
register_parser("safe_parser", function(buffer, length)
    -- Check minimum size BEFORE reading
    if length < 16 then
        return false
    end

    -- All read functions return nil if out of bounds
    local value = readFloat(buffer, 12, true)
    if not value then
        log("Error: Failed to read float at offset 12")
        return false
    end

    return true
end)
```

### 2. Use `pcall` for Complex Parsing
```lua
register_parser("robust_parser", function(buffer, length)
    local success, result = pcall(function()
        -- Complex parsing logic here
        local header = readString(buffer, 0, 4)
        assert(header == "DATA", "Invalid header")

        local time = readDouble(buffer, 4, true)
        assert(time ~= nil, "Failed to read timestamp")

        update_signal("Data.value", time, 123.45)
        return true
    end)

    if not success then
        log("Parser error: " .. tostring(result))
        return false
    end

    return result
end)
```

### 3. Validate Data Ranges
```lua
local temp = readFloat(buffer, 12, true)

-- Sanity check (temperature in °C)
if temp and temp >= -50 and temp <= 100 then
    update_signal("Sensor.temperature", time, temp)
else
    log("Warning: Temperature out of range: " .. tostring(temp))
end
```

---

## Performance Considerations

### Execution Context
- Parsers run in the **main thread during frame callbacks** under `stateMutex` lock
- Called for **every received packet** (~100Hz typical, up to 100kHz possible)
- Keep parser execution time **< 10 µs** to avoid backlog

### Optimization Tips

**1. Early Return**
```lua
-- GOOD: Reject early
if length < 16 then return false end

-- BAD: Unnecessary work
local header = readString(buffer, 0, 4)  -- Wastes time if length check fails
```

**2. Minimize Allocations**
```lua
-- GOOD: Reuse local variables (defined outside parser)
local header_buffer = {}

register_parser("optimized", function(buffer, length)
    -- Reuse existing table
    -- ...
end)

-- BAD: Create new table every packet
register_parser("slow", function(buffer, length)
    local temp = {}  -- Allocated every call!
end)
```

**3. Cache Constants**
```lua
-- GOOD: Define once outside function
local HEADER_MAGIC = "FAST"
local MIN_PACKET_SIZE = 24

register_parser("fast", function(buffer, length)
    if length < MIN_PACKET_SIZE then return false end
    -- ...
end)
```

**4. Tune Lua GC**
```lua
-- In your parser initialization script
collectgarbage("setpause", 200)    -- Less frequent GC
collectgarbage("setstepmul", 400)  -- Larger GC steps
```

---

## Migration from C++ Parsing (signals.yaml)

### Automatic Migration

The `legacy_binary.lua` parser is **auto-generated** from your `signals.yaml` and provides backward compatibility.

**How it works:**
1. On startup, Lua scripts are loaded from `scripts/` (including `scripts/parsers/`)
2. `legacy_binary.lua` registers a parser for all packet types in `signals.yaml`
3. If no Lua parser handles a packet, C++ falls back to `signals.yaml` parsing

### Gradual Migration Strategy

**Option 1: Keep Both (Recommended)**
- Lua parsers run first
- C++ parsing acts as fallback
- Migrate one packet type at a time

**Option 2: Disable C++ Parsing**
- Empty your `signals.yaml` packets section
- All parsing handled by Lua

**Option 3: Hybrid**
- Use Lua for complex/custom formats
- Use C++ for simple, high-frequency packets

---

## Debugging Parsers

### 1. Enable Logging
```lua
log("Parser started, packet length: " .. tostring(length))

local header = readString(buffer, 0, 4)
log("Header: " .. tostring(header))
```

### 2. Hex Dump Packets
```lua
local hexDump = bytesToHex(buffer, 0, math.min(32, length))
log("Packet hex: " .. hexDump)
```

### 3. Check Parser Registration
Look for console output:
```
[LuaScriptManager] Registered packet parser: my_parser
```

### 4. Test with Known Packets
Create a test script to send known packets via UDP and verify parsing.

---

## File Structure

```
scripts/
  parsers/
    legacy_binary.lua       # Auto-generated from signals.yaml (backward compat)
    my_protocol.lua         # Your custom parser
    json_example.lua        # Example: JSON parsing (disabled by default)
    csv_example.lua         # Example: CSV parsing (disabled by default)
  transforms/
    accel_magnitude.lua     # Tier 1 transforms (deprecated in Tier 2)
```

**Note:** Parsers load recursively from `scripts/parsers/`. Organize as needed:
```
scripts/parsers/
  production/
    imu_parser.lua
    gps_parser.lua
  experimental/
    test_parser.lua
```

---

## API Reference Summary

| Function | Description | Returns |
|----------|-------------|---------|
| `readUInt8(buf, off)` | Read unsigned 8-bit | `0-255` or `nil` |
| `readUInt16(buf, off, le)` | Read unsigned 16-bit | `0-65535` or `nil` |
| `readUInt32(buf, off, le)` | Read unsigned 32-bit | `0-2^32-1` or `nil` |
| `readUInt64(buf, off, le)` | Read unsigned 64-bit | `0-2^64-1` or `nil` |
| `readInt8(buf, off)` | Read signed 8-bit | `-128-127` or `nil` |
| `readInt16(buf, off, le)` | Read signed 16-bit | `-32768-32767` or `nil` |
| `readInt32(buf, off, le)` | Read signed 32-bit | `-2^31-2^31-1` or `nil` |
| `readInt64(buf, off, le)` | Read signed 64-bit | `-2^63-2^63-1` or `nil` |
| `readFloat(buf, off, le)` | Read 32-bit float | `number` or `nil` |
| `readDouble(buf, off, le)` | Read 64-bit double | `number` or `nil` |
| `readString(buf, off, len)` | Read fixed-length string | `string` or `nil` |
| `readCString(buf, off)` | Read null-terminated string | `string` or `nil` |
| `getBufferLength(buf)` | Get buffer size | `number` |
| `getBufferByte(buf, idx)` | Get single byte | `0-255` or `nil` |
| `bytesToHex(buf, off, len)` | Hex dump for debugging | `string` or `nil` |
| `update_signal(name, time, val)` | Update signal | `void` |
| `create_signal(name)` | Pre-create signal | `void` |
| `trigger_packet_callbacks(type)` | Trigger Tier 1 transforms | `void` |
| `register_parser(name, func)` | Register parser | `void` |

---

## Troubleshooting

### Parser Not Being Called
- Check console for `[LuaScriptManager] Registered packet parser: ...`
- Verify script is in `scripts/parsers/` directory
- Check for Lua syntax errors in console

### Signals Not Updating
- Ensure parser returns `true` when successful
- Verify `update_signal()` is being called
- Check signal names match your expectations
- Look for nil values from read functions

### Tier 1 Transforms Not Running
- Ensure your parser calls `trigger_packet_callbacks(packetType)` after updating signals
- Verify the packet type string matches what's used in `on_packet()` callbacks
- Check that Tier 1 transform scripts are loaded (look for registration messages in console)
- The `legacy_binary.lua` parser includes these calls for all packet types

### Performance Issues
- Profile with `log()` and timestamps
- Check Lua GC settings (`collectgarbage("count")`)
- Reduce allocations in hot path
- Consider moving simple packets to C++ parsing

### Compilation Errors
- Ensure LuaScriptManager includes are correct
- Check that `LuaScriptManager*` is passed to DataSink
- Verify sol2 library is properly linked

---

## Next Steps

- **Tier 3**: Frame callbacks, GUI monitoring, event-based alerting
- **Tier 4**: Dynamic GUI elements, button callbacks, network transmission from Lua
- **Tier 5**: Timers, async operations, heartbeat signals

---

For more examples, see:
- `scripts/parsers/legacy_binary.lua` - Complete example of all packet types
- `scripts/parsers/json_example.lua` - JSON parsing
- `scripts/parsers/csv_example.lua` - CSV parsing
- `docs/LuaScripting.md` - Tier 1 API reference
