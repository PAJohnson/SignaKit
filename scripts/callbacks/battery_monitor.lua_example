-- Battery Monitor with Multiple Alert Levels
-- Demonstrates: Multiple alerts, signal existence checks, complex conditions
--
-- This script monitors battery voltage and provides warnings at different levels

-- Critical battery level (< 20%)
on_alert("battery_critical",
    function()
        if not signal_exists("BAT.voltage") then
            return false
        end

        local voltage = get_signal("BAT.voltage")
        if voltage == nil then
            return false
        end

        -- Assume 3S LiPo: 9.0V = ~20% (critical), 12.6V = 100%
        return voltage < 9.5
    end,
    function()
        local voltage = get_signal("BAT.voltage")
        log(string.format("🔴 CRITICAL: Battery critically low! Voltage: %.2fV - LAND IMMEDIATELY", voltage))
    end,
    15.0  -- Alert every 15 seconds when critical
)

-- Low battery warning (< 40%)
on_alert("battery_low",
    function()
        if not signal_exists("BAT.voltage") then
            return false
        end

        local voltage = get_signal("BAT.voltage")
        if voltage == nil then
            return false
        end

        -- 10.2V = ~40% for 3S LiPo
        return voltage < 10.5 and voltage >= 9.5
    end,
    function()
        local voltage = get_signal("BAT.voltage")
        log(string.format("🟡 WARNING: Battery low! Voltage: %.2fV - Consider landing soon", voltage))
    end,
    30.0  -- Alert every 30 seconds when low
)

-- High current draw alert
on_alert("high_current",
    function()
        if not signal_exists("BAT.current") then
            return false
        end

        local current = get_signal("BAT.current")
        if current == nil then
            return false
        end

        -- Alert if drawing more than 30A
        return current > 30.0
    end,
    function()
        local current = get_signal("BAT.current")
        log(string.format("⚡ ALERT: High current draw detected! Current: %.1fA", current))
    end,
    5.0
)

log("Loaded battery_monitor.lua - monitoring battery voltage and current")
