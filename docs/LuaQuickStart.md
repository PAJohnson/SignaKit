# Lua Scripting Quick Start

## 5-Minute Introduction to Lua Scripting in SignaKit

### What Can You Do?

Create **derived signals** from your telemetry data without writing C++ code:
- Compute vector magnitudes (e.g., total acceleration from X, Y, Z)
- Apply filters and smoothing
- Unit conversions (m/s → km/h, radians → degrees)
- Calculate power, energy, derivatives, integrals
- Detect thresholds and events

### Your First Script

Create a file `scripts/my_first_script.lua`:

```lua
-- Compute total acceleration magnitude
log("My first Lua script loaded!")

on_packet("IMU", "IMU.totalAccel", function()
    local ax = get_signal("IMU.accelX")
    local ay = get_signal("IMU.accelY")
    local az = get_signal("IMU.accelZ")

    if ax and ay and az then
        return math.sqrt(ax*ax + ay*ay + az*az)
    end

    return nil
end)
```

**That's it!** Start the application and you'll see a new signal `IMU.totalAccel` in your signal list.

### How It Works

1. Scripts in `scripts/` are automatically loaded at startup
2. `on_packet(packet_name, name, function)` creates a new signal
3. Your function is called ~100 times per second (every time packets arrive)
4. `get_signal(name)` reads the latest value of any signal
5. Return a number to add a data point, or `nil` to skip

### Key Functions

| Function | Purpose | Example |
|----------|---------|---------|
| `get_signal(name)` | Read latest value | `get_signal("GPS.speed")` |
| `on_packet(packet, name, func)` | Create derived signal | `on_packet("GPS", "My.Signal", function() ... end)` |
| `log(message)` | Print to console | `log("Script loaded")` |
| `signal_exists(name)` | Check if signal exists | `if signal_exists("GPS.latitude") then ...` |

### Common Patterns

#### Vector Magnitude
```lua
```lua
on_packet("IMU", "IMU.gyroMagnitude", function()
    local gx = get_signal("IMU.gyroX")
    local gy = get_signal("IMU.gyroY")
    local gz = get_signal("IMU.gyroZ")

    if gx and gy and gz then
        return math.sqrt(gx*gx + gy*gy + gz*gz)
    end
    return nil
end)
```

#### Unit Conversion
```lua
```lua
on_packet("GPS", "GPS.speedMph", function()
    local speed_ms = get_signal("GPS.speed")
    if speed_ms then
        return speed_ms * 2.237  -- m/s to mph
    end
    return nil
end)
```

#### Simple Filter
```lua
local filtered = nil
local alpha = 0.1

on_packet("BAT", "Battery.voltageFiltered", function()
    local voltage = get_signal("BAT.voltage")
    if voltage then
        if filtered == nil then
            filtered = voltage
        else
            filtered = alpha * voltage + (1 - alpha) * filtered
        end
        return filtered
    end
    return nil
end)
```

#### Threshold Detection
```lua
on_packet("BAT", "Battery.lowVoltage", function()
    local voltage = get_signal("BAT.voltage")
    if voltage then
        if voltage < 3.3 then
            log("WARNING: Low battery voltage!")
            return 1.0  -- Boolean as number
        end
        return 0.0
    end
    return nil
end)
```

### Signal Names

Signal names use the format: `PACKET.field`

**Available signals** (depend on your Lua parsers in `scripts/parsers/`):
- `IMU.accelX`, `IMU.accelY`, `IMU.accelZ` - Acceleration (m/s²)
- `IMU.gyroX`, `IMU.gyroY`, `IMU.gyroZ` - Angular velocity (rad/s)
- `GPS.latitude`, `GPS.longitude`, `GPS.altitude`, `GPS.speed`
- `BAT.voltage`, `BAT.current`, `BAT.temp`, `BAT.percentage`
- `MOTOR.rpm`, `MOTOR.torque`, `MOTOR.power`

### GUI Controls

**Scripts Menu** → **Reload All Scripts**: Hot-reload all scripts without restarting

**Scripts Menu** → **Load Script...**: Load additional scripts from anywhere

**Scripts Menu** → Script name: Toggle script on/off (hover for error messages)

### Best Practices

1. **Always check for nil**: Signals might not exist or have data yet
   ```lua
   local value = get_signal("Some.Signal")
   if not value then
       return nil  -- Exit early
   end
   ```

2. **Use descriptive names**: `"IMU.accelMagnitude"` is better than `"Signal1"`

3. **Add logging**: Help debug your scripts
   ```lua
   log("Loaded accel_magnitude.lua")
   ```

4. **Keep it fast**: Your function runs ~100 times/second, keep it simple

5. **Use local variables**: Faster than globals
   ```lua
   local THRESHOLD = 10.0  -- Good
   THRESHOLD = 10.0        -- Avoid
   ```

### Next Steps

- Read [docs/LuaScripting.md](LuaScripting.md) for complete API reference
- Check out [scripts/README.md](../scripts/README.md) for more examples
- Experiment with the example scripts in `scripts/`
- Try creating a filter, unit conversion, or threshold detector

### Troubleshooting

**Script not loading?**
- Check console output for errors
- Verify filename ends with `.lua`
- Check for syntax errors

**Transform not working?**
- Make sure you're returning a number (not nil)
- Verify input signals exist: `signal_exists("IMU.accelX")`
- Check console for runtime errors

**Performance issues?**
- Simplify your transform function
- Avoid creating large tables or strings
- Use `get_signal()` instead of `get_signal_history()` when possible

### Example: Complete Script

Here's a complete script that demonstrates best practices:

```lua
-- battery_monitor.lua
-- Computes battery power and detects low voltage conditions

log("Loaded battery_monitor.lua")

-- Constants
local LOW_VOLTAGE_THRESHOLD = 3.3
local CRITICAL_VOLTAGE_THRESHOLD = 3.0

-- Compute battery power (Watts)
on_packet("BAT", "BAT.power", function()
    local voltage = get_signal("BAT.voltage")
    local current = get_signal("BAT.current")

    if voltage and current then
        return voltage * current
    end

    return nil
end)

-- Low voltage detector
on_packet("BAT", "BAT.lowVoltageWarning", function()
    local voltage = get_signal("BAT.voltage")

    if not voltage then
        return nil
    end

    if voltage < CRITICAL_VOLTAGE_THRESHOLD then
        log("CRITICAL: Battery voltage critically low: " .. tostring(voltage) .. "V")
        return 2.0  -- Critical state
    elseif voltage < LOW_VOLTAGE_THRESHOLD then
        log("WARNING: Battery voltage low: " .. tostring(voltage) .. "V")
        return 1.0  -- Warning state
    end

    return 0.0  -- Normal state
end)

-- Battery health indicator (0-100%)
on_packet("BAT", "BAT.healthPercent", function()
    local health = get_signal("BAT.health")

    if health then
        -- Assuming health is 0-100
        return health
    end

    return nil
end)

log("Battery monitoring scripts registered")
```

Save this as `scripts/battery_monitor.lua` and restart the application. You'll now have three new signals:
- `BAT.power` - Battery power in Watts
- `BAT.lowVoltageWarning` - Warning level (0=OK, 1=Low, 2=Critical)
- `BAT.healthPercent` - Battery health percentage

Happy scripting! 🚀
