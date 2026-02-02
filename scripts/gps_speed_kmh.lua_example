-- Example: Convert GPS speed from m/s to km/h
-- This creates a new signal "GPS.speedKmh" from the GPS speed in m/s
-- Uses on_packet to run every time a GPS packet is received

log("Loaded script: gps_speed_kmh.lua")

on_packet("GPS", "GPS.speedKmh", function()
    local speed_ms = get_signal("GPS.speed")

    if speed_ms then
        -- Convert m/s to km/h (multiply by 3.6)
        local speed_kmh = speed_ms * 3.6
        return speed_kmh
    end

    return nil
end)
