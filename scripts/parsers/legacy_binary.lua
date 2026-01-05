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

    -- IMU Packet (164 bytes with expanded fields)
    if header == "IMU" and length >= 164 then
        local time = readDouble(buffer, 4, true)
        local accelX = readField(buffer, "float", 12)
        local accelY = readField(buffer, "float", 16)
        local accelZ = readField(buffer, "float", 20)
        local gyroX = readField(buffer, "float", 24)
        local gyroY = readField(buffer, "float", 28)
        local gyroZ = readField(buffer, "float", 32)
        local magX = readField(buffer, "float", 36)
        local magY = readField(buffer, "float", 40)
        local magZ = readField(buffer, "float", 44)
        local temperature = readField(buffer, "float", 48)

        if time and accelX and accelY and accelZ and gyroX and gyroY and gyroZ then
            update_signal("IMU.time", time, time)
            update_signal("IMU.accelX", time, accelX)
            update_signal("IMU.accelY", time, accelY)
            update_signal("IMU.accelZ", time, accelZ)
            update_signal("IMU.gyroX", time, gyroX)
            update_signal("IMU.gyroY", time, gyroY)
            update_signal("IMU.gyroZ", time, gyroZ)
            update_signal("IMU.magX", time, magX)
            update_signal("IMU.magY", time, magY)
            update_signal("IMU.magZ", time, magZ)
            update_signal("IMU.temperature", time, temperature)

            -- Covariance matrices (9 elements each)
            for i = 0, 8 do
                local accelCov = readField(buffer, "float", 52 + i * 4)
                local gyroCov = readField(buffer, "float", 88 + i * 4)
                local magCov = readField(buffer, "float", 124 + i * 4)
                update_signal("IMU.accelCov[" .. i .. "]", time, accelCov)
                update_signal("IMU.gyroCov[" .. i .. "]", time, gyroCov)
                update_signal("IMU.magCov[" .. i .. "]", time, magCov)
            end

            -- Calibration status
            local accelCalibStatus = readField(buffer, "uint8_t", 160)
            local gyroCalibStatus = readField(buffer, "uint8_t", 161)
            local magCalibStatus = readField(buffer, "uint8_t", 162)
            update_signal("IMU.accelCalibStatus", time, accelCalibStatus)
            update_signal("IMU.gyroCalibStatus", time, gyroCalibStatus)
            update_signal("IMU.magCalibStatus", time, magCalibStatus)

            -- Trigger Tier 1 on_packet() callbacks for transforms
            trigger_packet_callbacks("IMU", time)

            return true
        end
    end

    -- GPS Packet (174 bytes with expanded fields)
    if header == "GPS" and length >= 174 then
        local time = readDouble(buffer, 4, true)
        local latitude = readField(buffer, "double", 12)
        local longitude = readField(buffer, "double", 20)
        local altitude = readField(buffer, "float", 28)
        local speed = readField(buffer, "float", 32)
        local heading = readField(buffer, "float", 36)
        local verticalSpeed = readField(buffer, "float", 40)

        if time and latitude and longitude and altitude and speed then
            update_signal("GPS.time", time, time)
            update_signal("GPS.latitude", time, latitude)
            update_signal("GPS.longitude", time, longitude)
            update_signal("GPS.altitude", time, altitude)
            update_signal("GPS.speed", time, speed)
            update_signal("GPS.heading", time, heading)
            update_signal("GPS.verticalSpeed", time, verticalSpeed)

            -- Position and velocity covariance (9 elements each)
            for i = 0, 8 do
                local posCov = readField(buffer, "float", 44 + i * 4)
                local velCov = readField(buffer, "float", 80 + i * 4)
                update_signal("GPS.posCov[" .. i .. "]", time, posCov)
                update_signal("GPS.velCov[" .. i .. "]", time, velCov)
            end

            -- GPS quality metrics
            local numSatellites = readField(buffer, "uint8_t", 116)
            local fixType = readField(buffer, "uint8_t", 117)
            local hdop = readField(buffer, "float", 118)
            local vdop = readField(buffer, "float", 122)
            update_signal("GPS.numSatellites", time, numSatellites)
            update_signal("GPS.fixType", time, fixType)
            update_signal("GPS.hdop", time, hdop)
            update_signal("GPS.vdop", time, vdop)

            -- Satellite info (12 satellites, 4 bytes each)
            for i = 0, 11 do
                local offset = 126 + i * 4
                local prn = readField(buffer, "uint8_t", offset)
                local snr = readField(buffer, "uint8_t", offset + 1)
                local elevation = readField(buffer, "uint8_t", offset + 2)
                local azimuth = readField(buffer, "uint8_t", offset + 3)
                update_signal("GPS.satellites[" .. i .. "].prn", time, prn)
                update_signal("GPS.satellites[" .. i .. "].snr", time, snr)
                update_signal("GPS.satellites[" .. i .. "].elevation", time, elevation)
                update_signal("GPS.satellites[" .. i .. "].azimuth", time, azimuth)
            end

            trigger_packet_callbacks("GPS", time)
            return true
        end
    end

    -- Battery Packet (268 bytes with expanded fields)
    if header == "BAT" and length >= 268 then
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

            -- Cell-level diagnostics (16 cells, 10 bytes each: float voltage, float temp, uint8 health, uint8 balancing, uint16 resistance)
            for i = 0, 15 do
                local offset = 28 + i * 10
                local cellVoltage = readField(buffer, "float", offset)
                local cellTemperature = readField(buffer, "float", offset + 4)
                local cellHealth = readField(buffer, "uint8_t", offset + 8)
                local cellBalancing = readField(buffer, "uint8_t", offset + 9)
                local cellResistance = readField(buffer, "uint16_t", offset + 10)
                update_signal("Battery.cells[" .. i .. "].voltage", time, cellVoltage)
                update_signal("Battery.cells[" .. i .. "].temperature", time, cellTemperature)
                update_signal("Battery.cells[" .. i .. "].health", time, cellHealth)
                update_signal("Battery.cells[" .. i .. "].balancing", time, cellBalancing)
                update_signal("Battery.cells[" .. i .. "].resistance", time, cellResistance)
            end

            -- Extended battery pack status (starts at offset 188 after 16 cells)
            local maxCellVoltage = readField(buffer, "float", 188)
            local minCellVoltage = readField(buffer, "float", 192)
            local avgCellVoltage = readField(buffer, "float", 196)
            local maxCellTemp = readField(buffer, "float", 200)
            local minCellTemp = readField(buffer, "float", 204)
            local avgCellTemp = readField(buffer, "float", 208)
            local powerOut = readField(buffer, "float", 212)
            local energyConsumed = readField(buffer, "float", 216)
            local energyRemaining = readField(buffer, "float", 220)
            local timeToEmpty = readField(buffer, "uint32_t", 224)
            local timeToFull = readField(buffer, "uint32_t", 228)
            local chargeState = readField(buffer, "uint8_t", 232)
            local faultFlags = readField(buffer, "uint8_t", 233)

            update_signal("Battery.maxCellVoltage", time, maxCellVoltage)
            update_signal("Battery.minCellVoltage", time, minCellVoltage)
            update_signal("Battery.avgCellVoltage", time, avgCellVoltage)
            update_signal("Battery.maxCellTemp", time, maxCellTemp)
            update_signal("Battery.minCellTemp", time, minCellTemp)
            update_signal("Battery.avgCellTemp", time, avgCellTemp)
            update_signal("Battery.powerOut", time, powerOut)
            update_signal("Battery.energyConsumed", time, energyConsumed)
            update_signal("Battery.energyRemaining", time, energyRemaining)
            update_signal("Battery.timeToEmpty", time, timeToEmpty)
            update_signal("Battery.timeToFull", time, timeToFull)
            update_signal("Battery.chargeState", time, chargeState)
            update_signal("Battery.faultFlags", time, faultFlags)

            trigger_packet_callbacks("Battery", time)
            return true
        end
    end

    -- LIDAR Packet (798 bytes with expanded fields)
    if header == "LID" and length >= 798 then
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

            -- Track data (32 tracks, 24 bytes each: 5 floats + uint8 trackID + uint8 confidence + uint16 age)
            for i = 0, 31 do
                local offset = 26 + i * 24
                local trackRange = readField(buffer, "float", offset)
                local azimuth = readField(buffer, "float", offset + 4)
                local elevation = readField(buffer, "float", offset + 8)
                local velocity = readField(buffer, "float", offset + 12)
                local rcs = readField(buffer, "float", offset + 16)
                local trackID = readField(buffer, "uint8_t", offset + 20)
                local confidence = readField(buffer, "uint8_t", offset + 21)
                local age = readField(buffer, "uint16_t", offset + 22)
                update_signal("LIDAR.tracks[" .. i .. "].range", time, trackRange)
                update_signal("LIDAR.tracks[" .. i .. "].azimuth", time, azimuth)
                update_signal("LIDAR.tracks[" .. i .. "].elevation", time, elevation)
                update_signal("LIDAR.tracks[" .. i .. "].velocity", time, velocity)
                update_signal("LIDAR.tracks[" .. i .. "].rcs", time, rcs)
                update_signal("LIDAR.tracks[" .. i .. "].trackID", time, trackID)
                update_signal("LIDAR.tracks[" .. i .. "].confidence", time, confidence)
                update_signal("LIDAR.tracks[" .. i .. "].age", time, age)
            end

            local numTracks = readField(buffer, "uint8_t", 794)
            update_signal("LIDAR.numTracks", time, numTracks)

            trigger_packet_callbacks("LIDAR", time)
            return true
        end
    end

    -- RADAR Packet (908 bytes with expanded fields)
    if header == "RAD" and length >= 908 then
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

            -- Track data (24 tracks, 36 bytes each: 7 floats + 4 uint8s + 2 uint16s)
            for i = 0, 23 do
                local offset = 32 + i * 36
                local trackRange = readField(buffer, "float", offset)
                local trackAzimuth = readField(buffer, "float", offset + 4)
                local trackElevation = readField(buffer, "float", offset + 8)
                local trackVelocity = readField(buffer, "float", offset + 12)
                local rangeRate = readField(buffer, "float", offset + 16)
                local rcs = readField(buffer, "float", offset + 20)
                local snr = readField(buffer, "float", offset + 24)
                local trackTrackID = readField(buffer, "uint8_t", offset + 28)
                local trackStatus = readField(buffer, "uint8_t", offset + 29)
                local classification = readField(buffer, "uint8_t", offset + 30)
                local confidence = readField(buffer, "uint8_t", offset + 31)
                local age = readField(buffer, "uint16_t", offset + 32)
                local hits = readField(buffer, "uint16_t", offset + 34)
                update_signal("RADAR.tracks[" .. i .. "].range", time, trackRange)
                update_signal("RADAR.tracks[" .. i .. "].azimuth", time, trackAzimuth)
                update_signal("RADAR.tracks[" .. i .. "].elevation", time, trackElevation)
                update_signal("RADAR.tracks[" .. i .. "].velocity", time, trackVelocity)
                update_signal("RADAR.tracks[" .. i .. "].rangeRate", time, rangeRate)
                update_signal("RADAR.tracks[" .. i .. "].rcs", time, rcs)
                update_signal("RADAR.tracks[" .. i .. "].snr", time, snr)
                update_signal("RADAR.tracks[" .. i .. "].trackID", time, trackTrackID)
                update_signal("RADAR.tracks[" .. i .. "].trackStatus", time, trackStatus)
                update_signal("RADAR.tracks[" .. i .. "].classification", time, classification)
                update_signal("RADAR.tracks[" .. i .. "].confidence", time, confidence)
                update_signal("RADAR.tracks[" .. i .. "].age", time, age)
                update_signal("RADAR.tracks[" .. i .. "].hits", time, hits)
            end

            -- Radar status fields (at end of packet after 24 tracks)
            local ambientNoise = readField(buffer, "float", 896)
            local temperature = readField(buffer, "float", 900)
            local mode = readField(buffer, "uint8_t", 904)
            local interference = readField(buffer, "uint8_t", 905)
            update_signal("RADAR.ambientNoise", time, ambientNoise)
            update_signal("RADAR.temperature", time, temperature)
            update_signal("RADAR.mode", time, mode)
            update_signal("RADAR.interference", time, interference)

            trigger_packet_callbacks("RADAR", time)
            return true
        end
    end

    -- State Packet (420 bytes with expanded fields)
    if header == "STA" and length >= 420 then
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

            -- Extended system state
            local diskUsage = readField(buffer, "float", 32)
            local networkTxRate = readField(buffer, "float", 36)
            local networkRxRate = readField(buffer, "float", 40)
            local gpuUsage = readField(buffer, "float", 44)
            local gpuMemoryUsage = readField(buffer, "float", 48)
            local gpuTemperature = readField(buffer, "float", 52)
            local cpuTemperature = readField(buffer, "float", 56)
            local boardTemperature = readField(buffer, "float", 60)
            update_signal("State.diskUsage", time, diskUsage)
            update_signal("State.networkTxRate", time, networkTxRate)
            update_signal("State.networkRxRate", time, networkRxRate)
            update_signal("State.gpuUsage", time, gpuUsage)
            update_signal("State.gpuMemoryUsage", time, gpuMemoryUsage)
            update_signal("State.gpuTemperature", time, gpuTemperature)
            update_signal("State.cpuTemperature", time, cpuTemperature)
            update_signal("State.boardTemperature", time, boardTemperature)

            -- Subsystem health (16 subsystems, 14 bytes each: uint8 status, uint8 health, uint16 errorCode, float cpu, float mem, uint16 padding)
            for i = 0, 15 do
                local offset = 64 + i * 14
                local status = readField(buffer, "uint8_t", offset)
                local health = readField(buffer, "uint8_t", offset + 1)
                local subsysErrorCode = readField(buffer, "uint16_t", offset + 2)
                local cpuUsageSub = readField(buffer, "float", offset + 4)
                local memUsage = readField(buffer, "float", offset + 8)
                update_signal("State.subsystems[" .. i .. "].status", time, status)
                update_signal("State.subsystems[" .. i .. "].health", time, health)
                update_signal("State.subsystems[" .. i .. "].errorCode", time, subsysErrorCode)
                update_signal("State.subsystems[" .. i .. "].cpuUsage", time, cpuUsageSub)
                update_signal("State.subsystems[" .. i .. "].memUsage", time, memUsage)
            end

            -- Error history (8 errors, 12 bytes each: uint32 errorCode, uint32 timestamp, uint8 severity, uint8 subsystem, uint16 padding)
            for i = 0, 7 do
                local offset = 288 + i * 12
                local errCode = readField(buffer, "uint32_t", offset)
                local timestamp = readField(buffer, "uint32_t", offset + 4)
                local severity = readField(buffer, "uint8_t", offset + 8)
                local subsystem = readField(buffer, "uint8_t", offset + 9)
                update_signal("State.errorHistory[" .. i .. "].errorCode", time, errCode)
                update_signal("State.errorHistory[" .. i .. "].timestamp", time, timestamp)
                update_signal("State.errorHistory[" .. i .. "].severity", time, severity)
                update_signal("State.errorHistory[" .. i .. "].subsystem", time, subsystem)
            end

            local errorHistoryCount = readField(buffer, "uint8_t", 384)
            update_signal("State.errorHistoryCount", time, errorHistoryCount)

            trigger_packet_callbacks("State", time)
            return true
        end
    end

    -- Debug Packet (874 bytes with expanded fields)
    if header == "DBG" and length >= 874 then
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

            -- Extended debug data: metrics array (32 floats)
            for i = 0, 31 do
                local metricVal = readField(buffer, "float", 34 + i * 4)
                update_signal("Debug.metrics[" .. i .. "]", time, metricVal)
            end

            -- Values array (32 int32s)
            for i = 0, 31 do
                local valueVal = readField(buffer, "int32_t", 162 + i * 4)
                update_signal("Debug.values[" .. i .. "]", time, valueVal)
            end

            -- Performance counters (8 counters, 40 bytes each: char[16] name, uint64 count, double rate, double average)
            for i = 0, 7 do
                local offset = 290 + i * 40
                -- Skip reading the name as string for now, just read the numeric fields
                local count = readField(buffer, "uint64_t", offset + 16)
                local rate = readField(buffer, "double", offset + 24)
                local average = readField(buffer, "double", offset + 32)
                update_signal("Debug.counters[" .. i .. "].count", time, count)
                update_signal("Debug.counters[" .. i .. "].rate", time, rate)
                update_signal("Debug.counters[" .. i .. "].average", time, average)
            end

            -- Stack trace (16 frames, 16 bytes each: uint64 address, uint32 offset, uint32 line)
            for i = 0, 15 do
                local offset = 610 + i * 16
                local address = readField(buffer, "uint64_t", offset)
                local offsetVal = readField(buffer, "uint32_t", offset + 8)
                local line = readField(buffer, "uint32_t", offset + 12)
                update_signal("Debug.stackTrace[" .. i .. "].address", time, address)
                update_signal("Debug.stackTrace[" .. i .. "].offset", time, offsetVal)
                update_signal("Debug.stackTrace[" .. i .. "].line", time, line)
            end

            local stackDepth = readField(buffer, "uint8_t", 866)
            update_signal("Debug.stackDepth", time, stackDepth)

            trigger_packet_callbacks("Debug", time)
            return true
        end
    end

    -- Motor Packet (126 bytes with expanded fields)
    if header == "MTR" and length >= 126 then
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

            -- Extended motor diagnostics
            local voltage = readField(buffer, "float", 30)
            local current = readField(buffer, "float", 34)
            local backEMF = readField(buffer, "float", 38)
            local efficiency = readField(buffer, "float", 42)
            local dutyCycle = readField(buffer, "float", 46)
            local motorTemp = readField(buffer, "float", 50)
            local controllerTemp = readField(buffer, "float", 54)
            local targetRPM = readField(buffer, "float", 58)
            local rpmError = readField(buffer, "float", 62)
            update_signal("Motor.voltage", time, voltage)
            update_signal("Motor.current", time, current)
            update_signal("Motor.backEMF", time, backEMF)
            update_signal("Motor.efficiency", time, efficiency)
            update_signal("Motor.dutyCycle", time, dutyCycle)
            update_signal("Motor.motorTemp", time, motorTemp)
            update_signal("Motor.controllerTemp", time, controllerTemp)
            update_signal("Motor.targetRPM", time, targetRPM)
            update_signal("Motor.rpmError", time, rpmError)

            -- Phase currents (3-phase motor)
            local phaseA_current = readField(buffer, "float", 66)
            local phaseB_current = readField(buffer, "float", 70)
            local phaseC_current = readField(buffer, "float", 74)
            update_signal("Motor.phaseA_current", time, phaseA_current)
            update_signal("Motor.phaseB_current", time, phaseB_current)
            update_signal("Motor.phaseC_current", time, phaseC_current)

            -- Phase voltages
            local phaseA_voltage = readField(buffer, "float", 78)
            local phaseB_voltage = readField(buffer, "float", 82)
            local phaseC_voltage = readField(buffer, "float", 86)
            update_signal("Motor.phaseA_voltage", time, phaseA_voltage)
            update_signal("Motor.phaseB_voltage", time, phaseB_voltage)
            update_signal("Motor.phaseC_voltage", time, phaseC_voltage)

            -- PID control signals
            local pidP = readField(buffer, "float", 90)
            local pidI = readField(buffer, "float", 94)
            local pidD = readField(buffer, "float", 98)
            local pidOutput = readField(buffer, "float", 102)
            update_signal("Motor.pidP", time, pidP)
            update_signal("Motor.pidI", time, pidI)
            update_signal("Motor.pidD", time, pidD)
            update_signal("Motor.pidOutput", time, pidOutput)

            -- Health metrics
            local vibration = readField(buffer, "float", 106)
            local acousticNoise = readField(buffer, "float", 110)
            local runTime = readField(buffer, "uint32_t", 114)
            local startCount = readField(buffer, "uint32_t", 118)
            local warningFlags = readField(buffer, "uint16_t", 122)
            update_signal("Motor.vibration", time, vibration)
            update_signal("Motor.acousticNoise", time, acousticNoise)
            update_signal("Motor.runTime", time, runTime)
            update_signal("Motor.startCount", time, startCount)
            update_signal("Motor.warningFlags", time, warningFlags)

            trigger_packet_callbacks("Motor", time)
            return true
        end
    end

    -- No packet matched
    return false
end)

log("Legacy binary parser registered successfully")
