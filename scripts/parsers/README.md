# Lua Packet Parsers

This directory contains Lua scripts that parse raw packet data from network/serial sources.

## How It Works

1. **C++ I/O Layer** receives raw bytes from UDP/TCP/Serial
2. **Lua Parser Chain** is invoked with `(buffer, length)`
3. **First parser to return `true`** handles the packet
4. **Signal Registry** is updated with parsed values

## Files in This Directory

### Production Parsers

- **`legacy_binary.lua`** - Binary packet parser for the legacy telemetry protocol
  - Handles all packet types: IMU, GPS, Battery, LIDAR, RADAR, State, Debug, Motor
  - Enabled by default
  - Demonstrates the Lua packet parsing API

### Example Parsers (Disabled by Default)

- **`json_example.lua`** - JSON packet parsing example
  - Format: `{"type":"sensor","timestamp":123.45,"temperature":23.5}`
  - Uncomment to enable

- **`csv_example.lua`** - CSV packet parsing example
  - Format: `timestamp,sensor_name,value` or `timestamp,temp,humidity,pressure`
  - Uncomment to enable

## Creating Your Own Parser

### Basic Template

```lua
-- my_parser.lua
log("Loading my custom parser")

register_parser("my_parser_name", function(buffer, length)
    -- 1. Validate packet size
    if length < 12 then
        return false
    end

    -- 2. Check header or magic bytes
    local header = readString(buffer, 0, 4)
    if header ~= "MYPR" then
        return false  -- Not our packet
    end

    -- 3. Parse fields
    local timestamp = readDouble(buffer, 4, true)  -- Little-endian double
    local value = readFloat(buffer, 12, true)      -- Little-endian float

    -- 4. Validate data (optional)
    if not timestamp or not value then
        return false
    end

    -- 5. Update signals
    update_signal("MyProtocol.value", timestamp, value)

    -- 6. Return true to indicate success
    return true
end)

log("My custom parser registered")
```

### Available API Functions

**Integer Readers:**
- `readUInt8(buffer, offset)` → `0-255` or `nil`
- `readUInt16(buffer, offset, littleEndian)` → `0-65535` or `nil`
- `readUInt32(buffer, offset, littleEndian)` → `0-4294967295` or `nil`
- `readUInt64(buffer, offset, littleEndian)` → `0-2^64-1` or `nil`
- `readInt8/16/32/64(...)` - Signed versions

**Floating Point:**
- `readFloat(buffer, offset, littleEndian)` → IEEE 754 single (32-bit)
- `readDouble(buffer, offset, littleEndian)` → IEEE 754 double (64-bit)

**Strings:**
- `readString(buffer, offset, length)` → Fixed-length string
- `readCString(buffer, offset)` → Null-terminated string

**Utilities:**
- `getBufferLength(buffer)` → Total buffer size
- `getBufferByte(buffer, index)` → Single byte (0-255)
- `bytesToHex(buffer, offset, length)` → Hex dump string

**Signal Functions:**
- `update_signal(name, timestamp, value)` → Create/update signal
- `create_signal(name)` → Pre-create signal (optional)

**Default Endianness:** `littleEndian = true` (most common for binary protocols)

## Parser Execution Order

Parsers are executed in the order they are registered (which depends on filesystem order). The **first parser to return `true` wins** and stops the chain.

**Best Practice:** Make header checks specific to avoid false positives.

## Performance Tips

- **Early return:** Check packet size and header immediately
- **Minimize allocations:** Define reusable variables outside the parser function
- **Cache constants:** Define magic numbers, offsets, etc. once
- **Bounds checking:** All read functions return `nil` if out of bounds
- **Target:** Keep execution time < 10 µs per packet

## Debugging

### Enable Logging
```lua
log("Parser called with length: " .. tostring(length))
local hexDump = bytesToHex(buffer, 0, math.min(16, length))
log("First 16 bytes: " .. hexDump)
```

### Check Registration
Look for console output:
```
[LuaScriptManager] Registered packet parser: my_parser_name
```

### Test with Known Data
Send test packets via UDP and verify console logs show successful parsing.

## File Organization

You can organize parsers into subdirectories:
```
scripts/parsers/
  production/
    imu.lua
    gps.lua
  experimental/
    test_parser.lua
  deprecated/
    old_format.lua
```

All `.lua` files are loaded recursively.

## Migration from signals.yaml

### Option 1: Gradual Migration (Recommended)
- Keep `legacy_binary.lua` enabled
- Add custom parsers for new formats
- Lua parsers are tried first, C++ fallback remains

### Option 2: Full Lua Migration
- Rely entirely on `legacy_binary.lua`
- Empty the `packets:` section in `signals.yaml`
- All parsing handled by Lua

### Option 3: Hybrid Approach
- Use Lua for complex/variable formats (JSON, Protobuf, encrypted)
- Keep C++ for simple, high-frequency packets

## Documentation

- **Full API Reference:** `docs/LuaPacketParsing.md`
- **Tier 1 Transforms:** `docs/LuaScripting.md`
- **Implementation Plan:** `docs/Todo.md` (Tier 2 section)

## Examples

See the example parsers in this directory:
- `legacy_binary.lua` - Complete implementation of all packet types
- `json_example.lua` - Parsing JSON-formatted telemetry
- `csv_example.lua` - Parsing CSV-formatted telemetry

## Troubleshooting

**Parser not being called?**
- Check for syntax errors in console
- Verify script is in `scripts/parsers/` or subdirectory
- Ensure script calls `register_parser(...)`

**Signals not updating?**
- Parser must return `true` on success
- Check that `update_signal()` is called
- Verify nil checks on all read functions

**Performance issues?**
- Profile with `log()` and timestamps
- Reduce string allocations
- Check Lua GC settings (`collectgarbage("count")`)

---

**Questions?** See the full documentation in `docs/LuaPacketParsing.md`
