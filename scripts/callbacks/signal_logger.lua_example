-- Periodic Signal Logger
-- Demonstrates: Frame callbacks with timing control, signal history access
--
-- This script logs specific signals at regular intervals

local logInterval = 5.0  -- Log every 5 seconds
local timeSinceLastLog = 0.0

-- List of signals to log
local signalsToLog = {
    "IMU.accelX",
    "IMU.accelY",
    "IMU.accelZ",
    "GPS.latitude",
    "GPS.longitude",
    "GPS.altitude",
    "BAT.voltage",
    "BAT.current"
}

on_frame(function()
    local dt = get_delta_time()
    timeSinceLastLog = timeSinceLastLog + dt

    if timeSinceLastLog >= logInterval then
        -- Build log message
        local logMsg = string.format("Signal Log [Frame %d]:", get_frame_number())

        for _, signalName in ipairs(signalsToLog) do
            if signal_exists(signalName) then
                local value = get_signal(signalName)
                if value ~= nil then
                    logMsg = logMsg .. string.format(" | %s=%.3f", signalName, value)
                end
            end
        end

        log(logMsg)
        timeSinceLastLog = 0.0
    end
end)

log("Loaded signal_logger.lua - logging signals every " .. logInterval .. " seconds")
