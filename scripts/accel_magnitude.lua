-- Example: Compute 3D acceleration magnitude from individual components
-- This creates a new signal "IMU.accelMagnitude" from accelX, accelY, accelZ

log("Loaded script: accel_magnitude.lua")

register_transform("IMU.accelMagnitude", function()
    local ax = get_signal("IMU.accelX")
    local ay = get_signal("IMU.accelY")
    local az = get_signal("IMU.accelZ")

    if ax and ay and az then
        local magnitude = math.sqrt(ax*ax + ay*ay + az*az)
        return magnitude
    end

    return nil
end)
