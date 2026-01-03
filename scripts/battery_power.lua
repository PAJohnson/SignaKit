-- Example: Compute battery power (Watts) from voltage and current
-- This creates a new signal "BAT.power" from voltage and current
-- Uses on_packet to run every time a BAT packet is received

log("Loaded script: battery_power.lua")

on_packet("BAT", "BAT.power", function()
    local voltage = get_signal("BAT.voltage")
    local current = get_signal("BAT.current")

    if voltage and current then
        local power = voltage * current
        return power
    end

    return nil
end)
