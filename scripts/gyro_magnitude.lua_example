-- Example: Compute 3D gyroscope magnitude from individual components
-- This creates a new signal "IMU.gyroMagnitude" from gyroX, gyroY, gyroZ
-- Uses on_packet to run every time an IMU packet is received

log("Loaded script: gyro_magnitude.lua")

on_packet("IMU", "IMU.gyroMagnitude", function()
    local gx = get_signal("IMU.gyroX")
    local gy = get_signal("IMU.gyroY")
    local gz = get_signal("IMU.gyroZ")

    if gx and gy and gz then
        local magnitude = math.sqrt(gx*gx + gy*gy + gz*gz)
        return magnitude
    end

    return nil
end)
