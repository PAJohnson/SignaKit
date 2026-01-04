# Frame Callbacks & Monitoring Examples

This directory contains example Lua scripts demonstrating **Tier 3** features: frame callbacks and event-based monitoring.

## Available Scripts

### 1. `frame_stats.lua` - Frame Rate Statistics
**Demonstrates:** Basic frame callbacks, timing, statistics accumulation

Tracks and logs FPS, average frame time, min/max frame times every second.

**Key APIs:**
- `on_frame(function)` - Register frame callback
- `get_frame_number()` - Get current frame number
- `get_delta_time()` - Time since last frame
- `get_plot_count()` - Number of active plots

**Output Example:**
```
Frame Stats - FPS: 59.8 | Avg: 16.72ms | Min: 15.21ms | Max: 18.45ms | Plots: 3
```

---

### 2. `altitude_alert.lua` - Altitude Monitoring
**Demonstrates:** Alert conditions, cooldown timers, signal monitoring

Monitors GPS altitude and triggers alerts when thresholds are exceeded.

**Key APIs:**
- `on_alert(name, conditionFunc, actionFunc, cooldownSeconds)` - Register alert
- `signal_exists(name)` - Check if signal exists
- `get_signal(name)` - Get latest signal value

**Alerts:**
- High altitude (>100m) - 5 second cooldown
- Negative altitude (<0m) - 10 second cooldown

---

### 3. `battery_monitor.lua` - Battery Warning System
**Demonstrates:** Multiple alert levels, complex conditions

Monitors battery voltage and current with tiered warning system.

**Alert Levels:**
- 🔴 Critical (<9.5V) - Every 15 seconds
- 🟡 Low (<10.5V) - Every 30 seconds
- ⚡ High current (>30A) - Every 5 seconds

**Use Case:** Battery management for drones/vehicles

---

### 4. `signal_logger.lua` - Periodic Data Logger
**Demonstrates:** Frame timing control, multiple signal access

Logs specified signals at regular intervals (default: 5 seconds).

**Key Features:**
- Configurable log interval
- Customizable signal list
- Frame-based timing

**Output Example:**
```
Signal Log [Frame 300]: | IMU.accelX=0.152 | IMU.accelY=-0.034 | GPS.altitude=42.300
```

---

### 5. `vibration_detector.lua` - Statistical Analysis
**Demonstrates:** Signal history analysis, variance calculation, pattern detection

Detects high-frequency vibrations by analyzing acceleration variance over time.

**Key APIs:**
- `get_signal_history(name, count)` - Get N latest values
- Statistical analysis on signal buffers

**Detection:** Alerts when total variance exceeds 5.0 across all axes

---

## Usage

### Loading Scripts
Scripts are automatically loaded from `scripts/` directory at startup (including subdirectories).

### Manual Reload
Use the **Scripts > Reload All Scripts** menu to reload all Lua scripts without restarting.

### Creating Your Own Callbacks

#### Frame Callback Template
```lua
on_frame(function()
    local frameNum = get_frame_number()
    local dt = get_delta_time()
    local plotCount = get_plot_count()

    -- Your code here
end)
```

#### Alert Template
```lua
on_alert("my_alert",
    -- Condition: return true to trigger
    function()
        if not signal_exists("My.Signal") then
            return false
        end
        return get_signal("My.Signal") > 100.0
    end,
    -- Action: what to do when triggered
    function()
        log("Alert triggered!")
    end,
    5.0  -- Cooldown in seconds
)
```

---

## API Reference

### Frame Callback Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `on_frame(func)` | - | Register callback executed every GUI frame (~60 FPS) |
| `get_frame_number()` | uint64 | Current frame number (increments each frame) |
| `get_delta_time()` | double | Time since last frame (seconds) |
| `get_plot_count()` | int | Total number of active plot windows |

### Alert Functions

| Function | Parameters | Description |
|----------|------------|-------------|
| `on_alert()` | name, conditionFunc, actionFunc, [cooldown] | Register condition monitor |

**Parameters:**
- `name` (string) - Alert identifier
- `conditionFunc` - Function returning `true` to trigger alert
- `actionFunc` - Function to execute when alert triggers
- `cooldown` (optional, default=0) - Minimum seconds between triggers

### Signal Access Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `signal_exists(name)` | bool | Check if signal exists in registry |
| `get_signal(name)` | double or nil | Get latest value of signal |
| `get_signal_history(name, count)` | table or nil | Get N latest values as array |

### Utility Functions

| Function | Parameters | Description |
|----------|------------|-------------|
| `log(message)` | string | Print message to console |

---

## Performance Considerations

### Frame Callbacks
- Execute **every frame** (~60 FPS = 60 calls/second)
- Keep callbacks **fast** (<1ms recommended)
- Avoid heavy computation in every frame
- Use timers/accumulators for periodic tasks (see `signal_logger.lua`)

### Alerts
- Condition functions execute **every frame**
- Use `signal_exists()` checks to avoid errors
- Set appropriate cooldowns to prevent spam
- Cooldown prevents action spam but condition still evaluates

### Best Practices
✅ **DO:**
- Check signal existence before access
- Use accumulators for periodic operations
- Set reasonable cooldowns on alerts
- Return early from callbacks when possible

❌ **DON'T:**
- Perform heavy I/O in frame callbacks
- Create signals in every frame (do it once)
- Log excessively (use intervals)
- Access non-existent signals without checks

---

## Thread Safety

All Lua callbacks execute in the **main render thread** with `stateMutex` held, ensuring:
- ✅ Safe access to signal registry
- ✅ No race conditions with network thread
- ✅ Consistent frame timing

**Note:** Callbacks block rendering - keep them fast!

---

## See Also
- [LuaScripting.md](../../docs/LuaScripting.md) - Complete Lua API reference
- [LuaPacketParsing.md](../../docs/LuaPacketParsing.md) - Tier 2 packet parsing
- [Todo.md](../../docs/Todo.md) - Tier 3 specification
