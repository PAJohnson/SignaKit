-- Altitude Alert Monitor
-- Demonstrates: Alert monitoring with conditions and cooldown
--
-- This script monitors altitude and triggers warnings when thresholds are exceeded

-- Alert when altitude exceeds 100 meters
on_alert("high_altitude",
    -- Condition function: returns true when alert should trigger
    function()
        if not signal_exists("GPS.altitude") then
            return false
        end

        local altitude = get_signal("GPS.altitude")
        if altitude == nil then
            return false
        end

        return altitude > 100.0
    end,
    -- Action function: what to do when alert triggers
    function()
        local altitude = get_signal("GPS.altitude")
        log(string.format("⚠️  ALERT: High altitude detected! Current: %.1f meters", altitude))
    end,
    5.0  -- Cooldown: 5 seconds between alerts
)

-- Alert when altitude is negative (below sea level or sensor error)
on_alert("negative_altitude",
    function()
        if not signal_exists("GPS.altitude") then
            return false
        end

        local altitude = get_signal("GPS.altitude")
        if altitude == nil then
            return false
        end

        return altitude < 0.0
    end,
    function()
        local altitude = get_signal("GPS.altitude")
        log(string.format("⚠️  ALERT: Negative altitude detected! Current: %.1f meters (sensor error?)", altitude))
    end,
    10.0  -- Cooldown: 10 seconds
)

log("Loaded altitude_alert.lua - monitoring GPS altitude thresholds")
