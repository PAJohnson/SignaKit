# Lua Scripting Guide for Telemetry GUI

## Overview

The Telemetry GUI supports extensibility through Lua scripts via the sol2 library. This allows you to create derived signals, apply custom transformations, and extend the functionality without modifying C++ code.

## Features

- **Signal Transforms**: Create new signals computed from existing telemetry data
- **Full Lua Standard Library**: Access to math, string, table, io, os libraries
- **Hot Reload**: Reload scripts on-the-fly without restarting the application
- **Auto-loading**: Scripts in the `scripts/` directory are loaded at startup
- **Manual Loading**: Load additional scripts via GUI file dialog
- **Performance**: Lua transforms execute efficiently during packet processing (~60 FPS)
- **Stateful Transforms**: Maintain state between transform calls for filters and accumulators

## Architecture

### Data Flow

```
Main GUI Thread (60 FPS)
    ↓
Lua I/O Frame Callback (DataSource.lua) → Receive UDP Packets
    ↓
Lua Parsers → Parse Packets → Signal Registry
    ↓
Lua Transforms → Add Derived Signals
    ↓
GUI Rendering (Render Plots/Controls)
```

### Transform Execution

1. Main thread executes frame callbacks (~60 FPS)
2. Lua I/O callback receives packets and calls parsers
3. Parsers update signals in the signal registry (with mutex lock)
4. **All registered Lua transforms execute** after packet parsing
5. Derived signals are added to the signal registry
6. GUI renders all signals (original + derived)

## Lua API Reference

### Signal Access Functions

#### `get_signal(name)`
Get the latest value of a signal.

**Parameters:**
- `name` (string): Signal name in format "PACKET.field" (e.g., "IMU.accelX")

**Returns:**
- `number`: The latest value, or `nil` if signal doesn't exist or has no data

**Example:**
```lua
local accelX = get_signal("IMU.accelX")
if accelX then
    print("AccelX:", accelX)
end
```

#### `get_signal_history(name, count)`
Get the last N values of a signal.

**Parameters:**
- `name` (string): Signal name
- `count` (number): Number of historical values to retrieve

**Returns:**
- `table`: Array of numbers (most recent values), or `nil` if signal doesn't exist

**Example:**
```lua
local history = get_signal_history("GPS.speed", 10)
if history then
    local sum = 0
    for i, value in ipairs(history) do
        sum = sum + value
    end
    local average = sum / #history
end
```

#### `signal_exists(name)`
Check if a signal exists in the registry.

**Parameters:**
- `name` (string): Signal name

**Returns:**
- `boolean`: `true` if signal exists, `false` otherwise

**Example:**
```lua
if signal_exists("GPS.latitude") then
    local lat = get_signal("GPS.latitude")
end
```

### Transform Registration

#### `register_transform(output_name, function)`
Register a function that computes a derived signal.

**Parameters:**
- `output_name` (string): Name of the new signal to create
- `function` (function): Lua function that returns a number or `nil`

**Notes:**
- Transform functions are called after each packet is parsed
- Returned values are automatically timestamped and added to the signal registry
- Returning `nil` skips adding a value for this cycle
- If the transform function has an error, it's logged and execution continues

**Example:**
```lua
register_transform("IMU.accelMagnitude", function()
    local ax = get_signal("IMU.accelX")
    local ay = get_signal("IMU.accelY")
    local az = get_signal("IMU.accelZ")

    if ax and ay and az then
        return math.sqrt(ax*ax + ay*ay + az*az)
    end

    return nil
end)
```

### Logging

#### `log(message)`
Print a message to the console.

**Parameters:**
- `message` (string): Message to log

**Example:**
```lua
log("Script initialized successfully")
log("Current battery voltage: " .. tostring(get_signal("BAT.voltage")))
```

## Usage

### Auto-loading Scripts

1. Place `.lua` files in the `scripts/` directory
2. Scripts are automatically loaded when the application starts
3. Use `log()` to confirm your script loaded

### GUI Controls

- **Scripts Menu** → **Reload All Scripts**: Reloads all loaded scripts (useful during development)
- **Scripts Menu** → **Load Script...**: Open file dialog to load additional scripts
- **Scripts Menu** → Script List: View loaded scripts, toggle enable/disable, see errors

### Error Handling

If a script has errors:
- An "ERROR" indicator appears next to the script name in the Scripts menu
- Hover over the error indicator to see the error message
- Fix the script and use "Reload All Scripts" to retry

## Examples

### Example 1: Vector Magnitude

Compute 3D magnitude from components:

```lua
-- accel_magnitude.lua
log("Loaded script: accel_magnitude.lua")

register_transform("IMU.accelMagnitude", function()
    local ax = get_signal("IMU.accelX")
    local ay = get_signal("IMU.accelY")
    local az = get_signal("IMU.accelZ")

    if ax and ay and az then
        return math.sqrt(ax*ax + ay*ay + az*az)
    end

    return nil
end)
```

### Example 2: Unit Conversion

Convert GPS speed from m/s to km/h:

```lua
-- gps_speed_kmh.lua
register_transform("GPS.speedKmh", function()
    local speed_ms = get_signal("GPS.speed")

    if speed_ms then
        return speed_ms * 3.6
    end

    return nil
end)
```

### Example 3: Power Calculation

Compute battery power from voltage and current:

```lua
-- battery_power.lua
register_transform("BAT.power", function()
    local voltage = get_signal("BAT.voltage")
    local current = get_signal("BAT.current")

    if voltage and current then
        return voltage * current
    end

    return nil
end)
```

### Example 4: Stateful Filter

Exponential moving average (EMA) filter:

```lua
-- simple_filter.lua
local filtered_value = nil
local alpha = 0.1  -- Smoothing factor

register_transform("IMU.accelX_filtered", function()
    local current = get_signal("IMU.accelX")

    if current then
        if filtered_value == nil then
            -- Initialize with first value
            filtered_value = current
        else
            -- Apply EMA formula
            filtered_value = alpha * current + (1 - alpha) * filtered_value
        end

        return filtered_value
    end

    return nil
end)
```

### Example 5: Threshold Detection

Detect when a signal exceeds a threshold:

```lua
-- high_g_detector.lua
local threshold = 15.0  -- G-force threshold

register_transform("IMU.highGDetected", function()
    local accelX = get_signal("IMU.accelX")
    local accelY = get_signal("IMU.accelY")
    local accelZ = get_signal("IMU.accelZ")

    if accelX and accelY and accelZ then
        local magnitude = math.sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ)

        if magnitude > threshold then
            log("HIGH G-FORCE DETECTED: " .. tostring(magnitude) .. " G")
            return 1.0  -- Boolean as number
        else
            return 0.0
        end
    end

    return nil
end)
```

### Example 6: Running Average

Compute a moving average using history:

```lua
-- moving_average.lua
local window_size = 10

register_transform("GPS.speed_smoothed", function()
    local history = get_signal_history("GPS.speed", window_size)

    if history and #history > 0 then
        local sum = 0
        for i, value in ipairs(history) do
            sum = sum + value
        end
        return sum / #history
    end

    return nil
end)
```

## Performance Considerations

### Execution Frequency
- Transforms execute during packet processing (triggered by Lua parsers)
- Keep transform functions lightweight (< 1ms execution time)
- Avoid expensive operations like file I/O in transforms

### Best Practices
1. **Check for nil**: Always verify signals exist before using them
2. **Early returns**: Return `nil` early if required signals aren't available
3. **Minimize allocations**: Reuse variables when possible for filters
4. **Use local variables**: Local variables are faster than globals
5. **Cache constants**: Define constants outside the transform function

### Example: Optimized Transform

```lua
-- Good: Constants defined once
local GRAVITY = 9.81
local THRESHOLD = 2.5 * GRAVITY

register_transform("IMU.exceedsThreshold", function()
    local accel = get_signal("IMU.accelZ")

    -- Early return if signal unavailable
    if not accel then
        return nil
    end

    -- Simple comparison
    return (accel > THRESHOLD) and 1.0 or 0.0
end)
```

## Available Signals

Signals follow the format: `PACKET.field`

### Common Packet Types

See `signals.yaml` for the complete, up-to-date list of available signals.

**IMU** (Inertial Measurement Unit):
- accelX, accelY, accelZ (m/s²)
- gyroX, gyroY, gyroZ (rad/s)

**GPS**:
- latitude, longitude (degrees)
- altitude (meters)
- speed (m/s)

**BAT** (Battery):
- voltage (V)
- current (A)
- temp (°C)
- percentage (%)
- health (0-100)
- cycleCount

**STATE** (System State):
- mode, armed, status
- errorCode, uptime
- cpuUsage, memoryUsage

**MOTOR**:
- rpm, torque, power
- temperature, throttle
- faults, rotations

## Troubleshooting

### Script Not Loading
- Check console output for error messages
- Verify the script is in the `scripts/` directory
- Ensure the file has a `.lua` extension
- Check for syntax errors

### Transform Not Creating Signal
- Verify the transform is registered (check console for registration message)
- Ensure the transform function is returning a number (not `nil`)
- Check that input signals exist and have data
- Look for runtime errors in console output

### Performance Issues
- Reduce complexity of transform functions
- Avoid creating large tables or strings
- Use `get_signal()` instead of `get_signal_history()` when possible
- Profile using `log()` with timestamps

## Future Extensions

### ✅ Tier 1: Signal Transforms (COMPLETE)
Current implementation - transform existing signals into derived signals.

### ✅ Tier 2: Lua Packet Parsing (COMPLETE - 2026-01-03)
**Custom packet parsers written entirely in Lua!**

Parse any packet format (binary, JSON, CSV, Protobuf, encrypted, etc.) without recompiling the GUI.

**New Features:**
- Byte-manipulation API: `readUInt8/16/32/64`, `readInt8/16/32/64`, `readFloat/Double`
- String readers: `readString`, `readCString`
- Buffer utilities: `getBufferLength`, `getBufferByte`, `bytesToHex`
- Signal manipulation: `update_signal`, `create_signal`
- Parser registration: `register_parser(name, function)`
- Endianness handling (little/big-endian)
- Auto-generated backward compatibility with signals.yaml

**Documentation:** See `docs/LuaPacketParsing.md` for complete API reference and examples.

**Example parsers:**
- `scripts/parsers/legacy_binary.lua` - Auto-generated from signals.yaml
- `scripts/parsers/json_example.lua` - JSON parsing example
- `scripts/parsers/csv_example.lua` - CSV parsing example

### 🔄 Tier 3: Frame Callbacks & Monitoring (TODO)
- Execute code every GUI frame for monitoring and alerting
- Event-based alerting when signals meet conditions
- Statistics accumulation and custom logging

### 🔄 Tier 4: GUI Control Elements (TODO)
- Dynamic UI element creation from Lua
- Button callbacks with custom actions
- Text input boxes for configuration

### 🔄 Tier 5: Timers & Async Operations (TODO)
- Timer registration (one-shot and cyclic)
- Heartbeat/keep-alive signal support
- Integration with network transmission

## Technical Details

- **Lua Version**: 5.4.7
- **C++ Bindings**: sol2 v3.3.1
- **Thread Safety**: Transforms execute with mutex lock on signal registry
- **Memory**: Signal history is limited to 2000 samples in online mode
- **Sandboxing**: Full standard library access (no restrictions)

## Contributing

To contribute example scripts:
1. Create a new `.lua` file in the `scripts/` directory
2. Add clear comments explaining what the script does
3. Test with both online and offline modes
4. Update `scripts/README.md` if adding a new category of transforms
