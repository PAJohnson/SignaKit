-- Example: Simple exponential moving average (EMA) filter
-- This demonstrates how to maintain state between calls
-- Creates "IMU.accelX_filtered" from "IMU.accelX"
-- Uses on_packet to run every time an IMU packet is received

log("Loaded script: simple_filter.lua")

-- State variable for the filter (persists between calls)
local filtered_value = nil
local alpha = 0.1  -- Smoothing factor (0 = no change, 1 = no smoothing)

on_packet("IMU", "IMU.accelX_filtered", function()
    local current = get_signal("IMU.accelX")

    if current then
        if filtered_value == nil then
            -- Initialize filter with first value
            filtered_value = current
        else
            -- Apply exponential moving average
            filtered_value = alpha * current + (1 - alpha) * filtered_value
        end

        return filtered_value
    end

    return nil
end)
