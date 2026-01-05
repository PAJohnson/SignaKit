# Lua Frame Callbacks & Monitoring (Tier 3)

This document describes the **Tier 3** Lua scripting features: frame-based callbacks and event monitoring.

---

## Table of Contents
1. [Overview](#overview)
2. [Frame Callbacks](#frame-callbacks)
3. [Alert System](#alert-system)
4. [API Reference](#api-reference)
5. [Examples](#examples)
6. [Performance Guidelines](#performance-guidelines)
7. [Architecture](#architecture)

---

## Overview

**Tier 3** adds real-time monitoring and frame-based execution to the Lua scripting system:

- **Frame Callbacks** - Execute Lua code every GUI render frame (~60 FPS)
- **Alert System** - Monitor signal conditions and trigger actions
- **Statistics** - Accumulate metrics over time
- **GUI State Access** - Query frame timing and plot count

### Use Cases
- Real-time statistics and logging
- Threshold-based alerts and warnings
- Performance monitoring (FPS, frame time)
- Custom data analysis every frame
- Heartbeat/watchdog timers
- Event correlation across signals

---

## Frame Callbacks

Frame callbacks execute **every GUI render frame** (typically 60 FPS).

### Registration

```lua
on_frame(function()
    -- Your code executes every frame
    local frameNum = get_frame_number()
    local deltaTime = get_delta_time()
    local plotCount = get_plot_count()

    -- Access signals
    if signal_exists("IMU.accelX") then
        local accel = get_signal("IMU.accelX")
        -- Process data...
    end
end)
```

### Frame Context API

| Function | Return Type | Description |
|----------|-------------|-------------|
| `get_frame_number()` | `uint64` | Current frame number (increments each frame) |
| `get_delta_time()` | `double` | Time since last frame in seconds (typically ~0.016s at 60 FPS) |
| `get_plot_count()` | `int` | Total number of active plot windows |

### Example: FPS Counter

```lua
local frameCount = 0
local timeAccum = 0.0

on_frame(function()
    frameCount = frameCount + 1
    timeAccum = timeAccum + get_delta_time()

    if timeAccum >= 1.0 then
        local fps = frameCount / timeAccum
        log(string.format("FPS: %.1f", fps))

        frameCount = 0
        timeAccum = 0.0
    end
end)
```

### Example: Periodic Logger

```lua
local logInterval = 5.0  -- Log every 5 seconds
local timeSinceLog = 0.0

on_frame(function()
    timeSinceLog = timeSinceLog + get_delta_time()

    if timeSinceLog >= logInterval then
        if signal_exists("BAT.voltage") then
            local voltage = get_signal("BAT.voltage")
            log(string.format("Battery: %.2fV", voltage))
        end
        timeSinceLog = 0.0
    end
end)
```

---

## Alert System

Alerts monitor **conditions** and execute **actions** when triggered.

### Registration

```lua
on_alert(
    "alert_name",           -- Unique identifier
    conditionFunction,      -- Returns true to trigger
    actionFunction,         -- Executed when triggered
    cooldownSeconds         -- Optional: minimum time between triggers (default: 0)
)
```

### Alert Lifecycle

1. **Condition Evaluation** - Runs every frame
2. **Cooldown Check** - Has enough time passed since last trigger?
3. **Action Execution** - If condition is true and cooldown expired
4. **Cooldown Reset** - Timestamp updated after action

### Example: High Temperature Alert

```lua
on_alert("high_temp",
    -- Condition: Check if temperature exceeds threshold
    function()
        if not signal_exists("Sensor.temperature") then
            return false
        end

        local temp = get_signal("Sensor.temperature")
        return temp ~= nil and temp > 80.0
    end,

    -- Action: Log warning
    function()
        local temp = get_signal("Sensor.temperature")
        log(string.format("⚠️  WARNING: Temperature critical! %.1f°C", temp))
    end,

    10.0  -- Cooldown: Don't spam - alert at most once per 10 seconds
)
```

### Example: Multi-Level Battery Alert

```lua
-- Critical level
on_alert("battery_critical",
    function()
        if not signal_exists("BAT.voltage") then return false end
        local v = get_signal("BAT.voltage")
        return v ~= nil and v < 9.5  -- 3S LiPo critical
    end,
    function()
        log("🔴 CRITICAL: Battery critically low! LAND IMMEDIATELY")
    end,
    15.0
)

-- Warning level
on_alert("battery_low",
    function()
        if not signal_exists("BAT.voltage") then return false end
        local v = get_signal("BAT.voltage")
        return v ~= nil and v < 10.5 and v >= 9.5
    end,
    function()
        log("🟡 WARNING: Battery low! Consider landing soon")
    end,
    30.0
)
```

### Example: Signal Watchdog

```lua
-- Alert if no data received for 5 seconds
local lastSignalTime = 0.0

on_frame(function()
    if signal_exists("IMU.accelX") then
        -- Signal exists, update timestamp (use frame time as proxy)
        lastSignalTime = get_frame_number() * get_delta_time()
    end
end)

on_alert("signal_timeout",
    function()
        local currentTime = get_frame_number() * get_delta_time()
        return (currentTime - lastSignalTime) > 5.0
    end,
    function()
        log("⚠️  ALERT: No signal received for 5 seconds!")
    end,
    5.0
)
```

---

## API Reference

### Registration Functions

#### `on_frame(callback)`
Register a callback to execute every GUI render frame.

**Parameters:**
- `callback` (function) - Function to execute each frame

**Returns:** None

**Example:**
```lua
on_frame(function()
    log("Frame " .. get_frame_number())
end)
```

---

#### `on_alert(name, conditionFunc, actionFunc, [cooldownSeconds])`
Register a condition monitor with optional cooldown.

**Parameters:**
- `name` (string) - Unique alert identifier
- `conditionFunc` (function) - Returns `true` when alert should trigger
- `actionFunc` (function) - Executed when alert triggers
- `cooldownSeconds` (number, optional) - Minimum seconds between triggers (default: 0)

**Returns:** None

**Example:**
```lua
on_alert("overspeed",
    function() return get_signal("GPS.speed") > 50.0 end,
    function() log("Speed limit exceeded!") end,
    5.0
)
```

---

### Frame Context Functions

#### `get_frame_number()`
Get the current frame number (increments each frame).

**Returns:** `uint64` - Frame number (starts at 1)

**Example:**
```lua
local frame = get_frame_number()
if frame % 60 == 0 then
    log("60 frames elapsed")
end
```

---

#### `get_delta_time()`
Get time elapsed since the last frame.

**Returns:** `double` - Delta time in seconds (typically ~0.016s at 60 FPS)

**Example:**
```lua
local dt = get_delta_time()
log(string.format("Frame time: %.2f ms", dt * 1000.0))
```

---

#### `get_plot_count()`
Get the total number of active plot windows.

**Returns:** `int` - Number of plots (time plots + readouts + XY plots + histograms + FFT + spectrograms)

**Example:**
```lua
local plotCount = get_plot_count()
if plotCount > 10 then
    log("Warning: Many plots open - may affect performance")
end
```

---

### Signal Access Functions

See [LuaScripting.md](LuaScripting.md#signal-access-api) for complete signal API reference:

- `signal_exists(name)` - Check if signal exists
- `get_signal(name)` - Get latest value
- `get_signal_history(name, count)` - Get N latest values

---

## Examples

### Example 1: Frame Statistics Tracker

```lua
local frameCount = 0
local timeAccum = 0.0
local minDt = math.huge
local maxDt = 0.0

on_frame(function()
    local dt = get_delta_time()

    frameCount = frameCount + 1
    timeAccum = timeAccum + dt

    if dt < minDt then minDt = dt end
    if dt > maxDt then maxDt = dt end

    if timeAccum >= 1.0 then
        local avgFPS = frameCount / timeAccum
        log(string.format("FPS: %.1f | Min: %.2fms | Max: %.2fms | Plots: %d",
            avgFPS, minDt * 1000, maxDt * 1000, get_plot_count()))

        frameCount = 0
        timeAccum = 0.0
        minDt = math.huge
        maxDt = 0.0
    end
end)
```

---

### Example 2: Vibration Detector

```lua
local checkInterval = 1.0
local timeSinceCheck = 0.0

on_frame(function()
    timeSinceCheck = timeSinceCheck + get_delta_time()

    if timeSinceCheck >= checkInterval then
        if signal_exists("IMU.accelX") then
            local history = get_signal_history("IMU.accelX", 60)

            if history and #history >= 20 then
                -- Calculate variance
                local sum = 0.0
                for i = 1, #history do
                    sum = sum + history[i]
                end
                local mean = sum / #history

                local varSum = 0.0
                for i = 1, #history do
                    local diff = history[i] - mean
                    varSum = varSum + (diff * diff)
                end
                local variance = varSum / #history

                if variance > 2.0 then
                    log(string.format("⚠️  High vibration detected! Variance: %.2f", variance))
                end
            end
        end

        timeSinceCheck = 0.0
    end
end)
```

---

### Example 3: Multi-Signal Correlation

```lua
on_alert("engine_fault",
    function()
        -- Check for RPM drop + high temperature
        if not (signal_exists("Engine.RPM") and signal_exists("Engine.temp")) then
            return false
        end

        local rpm = get_signal("Engine.RPM")
        local temp = get_signal("Engine.temp")

        if rpm == nil or temp == nil then
            return false
        end

        -- Fault: Low RPM + High temp (possible seizure)
        return rpm < 1000.0 and temp > 90.0
    end,
    function()
        local rpm = get_signal("Engine.RPM")
        local temp = get_signal("Engine.temp")
        log(string.format("🔴 ENGINE FAULT: Low RPM (%.0f) + High temp (%.1f°C)", rpm, temp))
    end,
    5.0
)
```

---

### Example 4: Custom Data Logger

```lua
local logFile = nil
local logInterval = 1.0
local timeSinceLog = 0.0

-- Open log file once
if logFile == nil then
    logFile = io.open("telemetry_log.csv", "a")
    if logFile then
        logFile:write("Frame,Time,AccelX,AccelY,AccelZ,Voltage\n")
    end
end

on_frame(function()
    timeSinceLog = timeSinceLog + get_delta_time()

    if timeSinceLog >= logInterval and logFile then
        local accelX = signal_exists("IMU.accelX") and get_signal("IMU.accelX") or 0
        local accelY = signal_exists("IMU.accelY") and get_signal("IMU.accelY") or 0
        local accelZ = signal_exists("IMU.accelZ") and get_signal("IMU.accelZ") or 0
        local voltage = signal_exists("BAT.voltage") and get_signal("BAT.voltage") or 0

        logFile:write(string.format("%d,%.3f,%.3f,%.3f,%.3f,%.2f\n",
            get_frame_number(),
            get_frame_number() * get_delta_time(),
            accelX, accelY, accelZ, voltage))

        logFile:flush()
        timeSinceLog = 0.0
    end
end)
```

---

## Performance Guidelines

### Frame Callback Performance

Frame callbacks execute **every frame** (~60 times per second):

✅ **Good Practices:**
- Use time accumulators for periodic tasks (see Example 1)
- Return early if signal doesn't exist
- Cache computations when possible
- Keep callbacks under **1ms** total execution time

❌ **Bad Practices:**
- Heavy computation every frame
- File I/O every frame (use intervals)
- Creating signals every frame (do it once)
- Expensive string operations

### Alert Performance

Alert conditions also evaluate every frame:

✅ **Efficient Conditions:**
```lua
function()
    if not signal_exists("Signal.name") then
        return false  -- Early return
    end
    return get_signal("Signal.name") > threshold
end
```

❌ **Inefficient Conditions:**
```lua
function()
    -- Don't do heavy work in conditions
    local history = get_signal_history("Signal.name", 1000)
    -- Expensive analysis...
    return result
end
```

### Cooldown Usage

Use cooldowns to prevent alert spam:

```lua
-- Without cooldown: Logs every frame while condition is true (60/sec!)
on_alert("test", condition, action, 0.0)

-- With cooldown: Logs at most once per 5 seconds
on_alert("test", condition, action, 5.0)
```

### Performance Monitoring

Monitor your callback performance:

```lua
local callbackStart = 0.0
local callbackTime = 0.0

on_frame(function()
    callbackStart = os.clock()

    -- Your expensive code here...

    callbackTime = os.clock() - callbackStart

    if callbackTime > 0.001 then
        log(string.format("⚠️  Slow callback: %.2f ms", callbackTime * 1000))
    end
end)
```

---

## Architecture

### Execution Flow

```
Main Render Loop (60 FPS)
  ↓
[ImGui::NewFrame()]
  ↓
[Lock stateMutex] ←─────────── Thread safety
  ↓
[Execute Frame Callbacks]
  ├─ Set frame context (frameNumber, deltaTime, plotCount)
  ├─ Execute all on_frame() callbacks
  ├─ Execute all alert conditions
  │   └─ If condition true + cooldown expired → execute action
  └─ Clear frame context
  ↓
[Render GUI]
  ↓
[Unlock stateMutex]
  ↓
[Swap buffers]
```

### Thread Safety

- **Execution Thread:** Main render thread
- **Mutex:** `stateMutex` is held during callback execution
- **Signal Access:** Safe - registry locked during frame callbacks
- **Lua Parsers:** Cannot modify signals while frame callbacks run (both use stateMutex)

### Timing Behavior

| Metric | Value | Notes |
|--------|-------|-------|
| Frame rate | ~60 FPS | VSync enabled |
| Frame period | ~16.67 ms | 1/60 second |
| Callback frequency | Every frame | ~60 calls/second |
| Alert evaluation | Every frame | Condition checked 60x/sec |
| Cooldown precision | Frame-based | ±16ms accuracy |

### Memory Management

- **Callback Storage:** `std::vector<sol::protected_function>`
- **Alert Storage:** `std::vector<Alert>` (struct with name, funcs, cooldown, lastTrigger)
- **Registration:** Callbacks persist until script reload
- **Cleanup:** `reloadAllScripts()` clears all callbacks and alerts

---

## Integration with Other Tiers

### Tier 1: Signal Transforms
Frame callbacks can access signals created by transforms:

```lua
-- Tier 1: Create derived signal
on_packet("IMU", "accel_magnitude", function()
    local x = get_signal("IMU.accelX")
    local y = get_signal("IMU.accelY")
    local z = get_signal("IMU.accelZ")
    return math.sqrt(x*x + y*y + z*z)
end)

-- Tier 3: Monitor derived signal
on_alert("high_accel",
    function()
        return signal_exists("accel_magnitude") and
               get_signal("accel_magnitude") > 20.0
    end,
    function()
        log("High acceleration detected!")
    end,
    2.0
)
```

### Tier 2: Packet Parsers
Frame callbacks see signals created by Lua parsers:

```lua
-- Tier 2: Parse custom packets
register_parser("custom", function(buffer, length)
    local value = readUInt32(buffer, 0, true)
    update_signal("Custom.value", os.time(), value)
    return true
end)

-- Tier 3: Monitor parsed signals
on_frame(function()
    if signal_exists("Custom.value") then
        local val = get_signal("Custom.value")
        log("Custom value: " .. val)
    end
end)
```

---

## Troubleshooting

### Callbacks Not Executing
**Symptom:** `on_frame()` or `on_alert()` not being called

**Solutions:**
1. Check script loaded successfully (no syntax errors)
2. Verify script in `scripts/` directory
3. Use **Scripts > Reload All Scripts** menu
4. Check console for Lua errors

### Alert Not Triggering
**Symptom:** Condition true but action not executing

**Solutions:**
1. Check cooldown - may be too long
2. Verify condition returns boolean `true`, not truthy value
3. Add debug logging to condition:
```lua
function()
    local result = (condition)
    log("Alert condition: " .. tostring(result))
    return result
end
```

### Poor Performance
**Symptom:** Low FPS, laggy UI

**Solutions:**
1. Profile callback time (see Performance Monitoring above)
2. Use time accumulators instead of every-frame execution
3. Reduce `get_signal_history()` sample count
4. Add early returns for non-existent signals
5. Increase alert cooldowns

### Signal Not Found
**Symptom:** `get_signal()` returns `nil`

**Solutions:**
1. Always check `signal_exists()` first
2. Verify signal name (case-sensitive)
3. Check signal created by network/parser
4. Ensure data received before callback runs

---

## See Also

- [LuaScripting.md](LuaScripting.md) - Complete Lua API reference (Tier 1)
- [LuaPacketParsing.md](LuaPacketParsing.md) - Packet parsing (Tier 2)
- [scripts/callbacks/README.md](../scripts/callbacks/README.md) - Example scripts
- [Todo.md](Todo.md) - Tier 3 specification
