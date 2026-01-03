-- Example: Compute battery power (Watts) from voltage and current
-- This creates a new signal "BAT.power" from voltage and current

log("Loaded script: battery_power.lua")

register_transform("Battery.power", function()
    local voltage = get_signal("Battery.voltage")
    local current = get_signal("Battery.current")

    if voltage and current then
        local power = voltage * current
        return power
    end

    return nil
end)
