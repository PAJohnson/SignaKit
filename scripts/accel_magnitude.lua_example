-- Example: Compute 3D acceleration magnitude from individual components
-- This creates a new signal "IMU.accelMagnitude" from accelX, accelY, accelZ
-- Uses on_packet to run every time an IMU packet is received

log("Loaded script: accel_magnitude.lua")

on_packet("IMU", "IMU.accelMagnitude", function()
    local ax = get_signal("IMU.accelX")
    local ay = get_signal("IMU.accelY")
    local az = get_signal("IMU.accelZ")

    if ax and ay and az then
        local magnitude = math.sqrt(ax*ax + ay*ay + az*az)
        return magnitude
    end

    return nil
end)
