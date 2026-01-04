-- Vibration Detector
-- Demonstrates: Signal history analysis, statistical calculations, frame callbacks
--
-- This script detects high-frequency vibrations by analyzing acceleration variance

local checkInterval = 1.0  -- Check every second
local timeSinceLastCheck = 0.0

on_frame(function()
    local dt = get_delta_time()
    timeSinceLastCheck = timeSinceLastCheck + dt

    if timeSinceLastCheck >= checkInterval then
        -- Get acceleration history (last 60 samples)
        if signal_exists("IMU.accelX") and signal_exists("IMU.accelY") and signal_exists("IMU.accelZ") then
            local histX = get_signal_history("IMU.accelX", 60)
            local histY = get_signal_history("IMU.accelY", 60)
            local histZ = get_signal_history("IMU.accelZ", 60)

            if histX and histY and histZ and #histX >= 20 then
                -- Calculate variance for each axis
                local function variance(data)
                    local sum = 0.0
                    for i = 1, #data do
                        sum = sum + data[i]
                    end
                    local mean = sum / #data

                    local varSum = 0.0
                    for i = 1, #data do
                        local diff = data[i] - mean
                        varSum = varSum + (diff * diff)
                    end
                    return varSum / #data
                end

                local varX = variance(histX)
                local varY = variance(histY)
                local varZ = variance(histZ)

                local totalVariance = varX + varY + varZ

                -- Alert if total variance exceeds threshold (indicates vibration)
                if totalVariance > 5.0 then
                    log(string.format("⚠️  VIBRATION DETECTED: Total variance = %.2f (X:%.2f, Y:%.2f, Z:%.2f)",
                        totalVariance, varX, varY, varZ))
                end
            end
        end

        timeSinceLastCheck = 0.0
    end
end)

log("Loaded vibration_detector.lua - monitoring for high-frequency vibrations")
