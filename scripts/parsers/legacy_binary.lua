-- Auto-generated parser for legacy binary protocol from signals.yaml
-- This parser maintains backward compatibility with the C++ packet parsing
-- It handles the same packet formats that were previously hardcoded in C++

log("Loading legacy binary parser (auto-generated from signals.yaml)")

-- Type mapping helper - converts YAML type names to appropriate read functions
local function readField(buffer, fieldType, offset)
    -- Handle type aliases and normalize type names
    local normalizedType = fieldType:gsub("_t", ""):gsub(" ", "")

    if fieldType == "double" then
        return readDouble(buffer, offset, true)  -- Little-endian
    elseif fieldType == "float" then
        return readFloat(buffer, offset, true)
    elseif fieldType == "int8_t" or fieldType == "int8" or fieldType == "char" then
        return readInt8(buffer, offset)
    elseif fieldType == "int16_t" or fieldType == "int16" or fieldType == "short" then
        return readInt16(buffer, offset, true)
    elseif fieldType == "int32_t" or fieldType == "int32" or fieldType == "int" or fieldType == "long" then
        return readInt32(buffer, offset, true)
    elseif fieldType == "int64_t" or fieldType == "int64" then
        return readInt64(buffer, offset, true)
    elseif fieldType == "uint8_t" or fieldType == "uint8" or fieldType == "unsigned char" then
        return readUInt8(buffer, offset)
    elseif fieldType == "uint16_t" or fieldType == "uint16" or fieldType == "unsigned short" then
        return readUInt16(buffer, offset, true)
    elseif fieldType == "uint32_t" or fieldType == "uint32" or fieldType == "unsigned int" or fieldType == "unsigned long" then
        return readUInt32(buffer, offset, true)
    elseif fieldType == "uint64_t" or fieldType == "uint64" then
        return readUInt64(buffer, offset, true)
    else
        log("Warning: Unknown type '" .. fieldType .. "', defaulting to readDouble")
        return readDouble(buffer, offset, true)
    end
end

-- Register the legacy binary parser
register_parser("legacy_binary", function(buffer, length)
    -- Try to match packet header (first 3-4 bytes)
    if length < 4 then
        return false  -- Packet too small
    end

    local header = readString(buffer, 0, 3)
    if not header then
        return false
    end

    -- IMU Packet
    if header == "IMU" and length >= 36 then
        local time = readDouble(buffer, 4, true)
        local accelX = readField(buffer, "float", 12)
        local accelY = readField(buffer, "float", 16)
        local accelZ = readField(buffer, "float", 20)
        local gyroX = readField(buffer, "float", 24)
        local gyroY = readField(buffer, "float", 28)
        local gyroZ = readField(buffer, "float", 32)

        if time and accelX and accelY and accelZ and gyroX and gyroY and gyroZ then
            update_signal("IMU.time", time, time)
            update_signal("IMU.accelX", time, accelX)
            update_signal("IMU.accelY", time, accelY)
            update_signal("IMU.accelZ", time, accelZ)
            update_signal("IMU.gyroX", time, gyroX)
            update_signal("IMU.gyroY", time, gyroY)
            update_signal("IMU.gyroZ", time, gyroZ)

            -- Trigger Tier 1 on_packet() callbacks for transforms
            trigger_packet_callbacks("IMU")

            return true
        end
    end

    -- GPS Packet
    if header == "GPS" and length >= 36 then
        local time = readDouble(buffer, 4, true)
        local latitude = readField(buffer, "double", 12)
        local longitude = readField(buffer, "double", 20)
        local altitude = readField(buffer, "float", 28)
        local speed = readField(buffer, "float", 32)

        if time and latitude and longitude and altitude and speed then
            update_signal("GPS.time", time, time)
            update_signal("GPS.latitude", time, latitude)
            update_signal("GPS.longitude", time, longitude)
            update_signal("GPS.altitude", time, altitude)
            update_signal("GPS.speed", time, speed)

            trigger_packet_callbacks("GPS")
            return true
        end
    end

    -- Battery Packet
    if header == "BAT" and length >= 28 then
        local time = readDouble(buffer, 4, true)
        local voltage = readField(buffer, "float", 12)
        local current = readField(buffer, "float", 16)
        local temperature = readField(buffer, "float", 20)
        local percentage = readField(buffer, "uint8_t", 24)
        local health = readField(buffer, "uint8_t", 25)
        local cycleCount = readField(buffer, "uint16_t", 26)

        if time and voltage and current and temperature and percentage and health and cycleCount then
            update_signal("Battery.time", time, time)
            update_signal("Battery.voltage", time, voltage)
            update_signal("Battery.current", time, current)
            update_signal("Battery.temperature", time, temperature)
            update_signal("Battery.percentage", time, percentage)
            update_signal("Battery.health", time, health)
            update_signal("Battery.cycleCount", time, cycleCount)

            trigger_packet_callbacks("Battery")
            return true
        end
    end

    -- LIDAR Packet
    if header == "LID" and length >= 26 then
        local time = readDouble(buffer, 4, true)
        local range = readField(buffer, "float", 12)
        local intensity = readField(buffer, "float", 16)
        local angleX = readField(buffer, "int16_t", 20)
        local angleY = readField(buffer, "int16_t", 22)
        local quality = readField(buffer, "uint8_t", 24)
        local flags = readField(buffer, "uint8_t", 25)

        if time and range and intensity and angleX and angleY and quality and flags then
            update_signal("LIDAR.time", time, time)
            update_signal("LIDAR.range", time, range)
            update_signal("LIDAR.intensity", time, intensity)
            update_signal("LIDAR.angleX", time, angleX)
            update_signal("LIDAR.angleY", time, angleY)
            update_signal("LIDAR.quality", time, quality)
            update_signal("LIDAR.flags", time, flags)

            trigger_packet_callbacks("LIDAR")
            return true
        end
    end

    -- RADAR Packet
    if header == "RAD" and length >= 32 then
        local time = readDouble(buffer, 4, true)
        local range = readField(buffer, "float", 12)
        local velocity = readField(buffer, "float", 16)
        local azimuth = readField(buffer, "float", 20)
        local elevation = readField(buffer, "float", 24)
        local signalStrength = readField(buffer, "int16_t", 28)
        local targetCount = readField(buffer, "uint8_t", 30)
        local trackID = readField(buffer, "uint8_t", 31)

        if time and range and velocity and azimuth and elevation and signalStrength and targetCount and trackID then
            update_signal("RADAR.time", time, time)
            update_signal("RADAR.range", time, range)
            update_signal("RADAR.velocity", time, velocity)
            update_signal("RADAR.azimuth", time, azimuth)
            update_signal("RADAR.elevation", time, elevation)
            update_signal("RADAR.signalStrength", time, signalStrength)
            update_signal("RADAR.targetCount", time, targetCount)
            update_signal("RADAR.trackID", time, trackID)

            trigger_packet_callbacks("RADAR")
            return true
        end
    end

    -- State Packet
    if header == "STA" and length >= 32 then
        local time = readDouble(buffer, 4, true)
        local systemMode = readField(buffer, "uint8_t", 12)
        local armed = readField(buffer, "uint8_t", 13)
        local statusFlags = readField(buffer, "uint16_t", 14)
        local errorCode = readField(buffer, "int32_t", 16)
        local uptime = readField(buffer, "uint32_t", 20)
        local cpuUsage = readField(buffer, "float", 24)
        local memoryUsage = readField(buffer, "float", 28)

        if time and systemMode and armed and statusFlags and errorCode and uptime and cpuUsage and memoryUsage then
            update_signal("State.time", time, time)
            update_signal("State.systemMode", time, systemMode)
            update_signal("State.armed", time, armed)
            update_signal("State.statusFlags", time, statusFlags)
            update_signal("State.errorCode", time, errorCode)
            update_signal("State.uptime", time, uptime)
            update_signal("State.cpuUsage", time, cpuUsage)
            update_signal("State.memoryUsage", time, memoryUsage)

            trigger_packet_callbacks("State")
            return true
        end
    end

    -- Debug Packet
    if header == "DBG" and length >= 34 then
        local time = readDouble(buffer, 4, true)
        local counter = readField(buffer, "int64_t", 12)
        local eventID = readField(buffer, "uint32_t", 20)
        local priority = readField(buffer, "int8_t", 24)
        local subsystem = readField(buffer, "uint8_t", 25)
        local value1 = readField(buffer, "int16_t", 26)
        local value2 = readField(buffer, "int16_t", 28)
        local metric = readField(buffer, "float", 30)

        if time and counter and eventID and priority and subsystem and value1 and value2 and metric then
            update_signal("Debug.time", time, time)
            update_signal("Debug.counter", time, counter)
            update_signal("Debug.eventID", time, eventID)
            update_signal("Debug.priority", time, priority)
            update_signal("Debug.subsystem", time, subsystem)
            update_signal("Debug.value1", time, value1)
            update_signal("Debug.value2", time, value2)
            update_signal("Debug.metric", time, metric)

            trigger_packet_callbacks("Debug")
            return true
        end
    end

    -- Motor Packet
    if header == "MTR" and length >= 30 then
        local time = readDouble(buffer, 4, true)
        local rpm = readField(buffer, "int16_t", 12)
        local torque = readField(buffer, "float", 14)
        local power = readField(buffer, "float", 18)
        local temperature = readField(buffer, "int8_t", 22)
        local throttle = readField(buffer, "uint8_t", 23)
        local faults = readField(buffer, "uint16_t", 24)
        local totalRotations = readField(buffer, "uint32_t", 26)

        if time and rpm and torque and power and temperature and throttle and faults and totalRotations then
            update_signal("Motor.time", time, time)
            update_signal("Motor.rpm", time, rpm)
            update_signal("Motor.torque", time, torque)
            update_signal("Motor.power", time, power)
            update_signal("Motor.temperature", time, temperature)
            update_signal("Motor.throttle", time, throttle)
            update_signal("Motor.faults", time, faults)
            update_signal("Motor.totalRotations", time, totalRotations)

            trigger_packet_callbacks("Motor")
            return true
        end
    end

    -- No packet matched
    return false
end)

log("Legacy binary parser registered successfully")
