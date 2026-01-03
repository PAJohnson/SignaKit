# Lua Scripts for Telemetry GUI

This directory contains Lua scripts that extend the functionality of the Telemetry GUI through signal transforms.

## Available API Functions

### Signal Access
- `get_signal(name)` - Get the latest value of a signal (returns `nil` if signal doesn't exist or has no data)
- `get_signal_history(name, count)` - Get the last N values of a signal as an array
- `signal_exists(name)` - Check if a signal exists (returns boolean)

### Transform Registration
- `register_transform(output_name, function)` - Register a function that computes a derived signal
  - `output_name`: Name of the new signal to create (e.g., "IMU.accelMagnitude")
  - `function`: Lua function that returns a number or `nil`

### Logging
- `log(message)` - Print a message to the console

## Example Scripts

### accel_magnitude.lua
Computes 3D acceleration magnitude from individual X, Y, Z components:
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

### gyro_magnitude.lua
Computes 3D gyroscope magnitude from individual X, Y, Z components.

### battery_power.lua
Calculates power (Watts) from voltage and current:
```lua
register_transform("BAT.power", function()
    local voltage = get_signal("BAT.voltage")
    local current = get_signal("BAT.current")
    if voltage and current then
        return voltage * current
    end
    return nil
end)
```

### gps_speed_kmh.lua
Converts GPS speed from m/s to km/h.

### simple_filter.lua
Demonstrates stateful transforms with an exponential moving average filter:
```lua
local filtered_value = nil
local alpha = 0.1

register_transform("IMU.accelX_filtered", function()
    local current = get_signal("IMU.accelX")
    if current then
        if filtered_value == nil then
            filtered_value = current
        else
            filtered_value = alpha * current + (1 - alpha) * filtered_value
        end
        return filtered_value
    end
    return nil
end)
```

## Writing Your Own Scripts

1. Create a `.lua` file in this directory
2. Use `register_transform()` to create derived signals
3. Access existing signals with `get_signal()`
4. Return a number or `nil` from your transform function
5. Scripts are automatically loaded at startup
6. Use the "Scripts" menu to reload scripts or load additional ones

## Tips

- **Performance**: Transforms run once per network packet burst (every ~10ms in online mode)
- **Error Handling**: Always check if `get_signal()` returns `nil` before using values
- **State**: Variables defined outside the transform function persist between calls
- **Math Library**: Full Lua math library is available (`math.sqrt`, `math.sin`, etc.)
- **Debugging**: Use `log()` to print debug messages to the console

## Signal Names

Signal names follow the pattern: `PACKET.field`

Common packets and their signals:
- **IMU**: accelX, accelY, accelZ, gyroX, gyroY, gyroZ
- **GPS**: latitude, longitude, altitude, speed
- **BAT**: voltage, current, temp, percentage, health, cycleCount
- **STATE**: mode, armed, status, errorCode, uptime, cpuUsage, memoryUsage
- **MOTOR**: rpm, torque, power, temperature, throttle, faults, rotations

Check the `signals.yaml` file for the complete list of available signals.
