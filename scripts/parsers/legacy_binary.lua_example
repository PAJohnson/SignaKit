-- Parser for legacy binary telemetry protocol
-- Handles IMU, GPS, Battery, LIDAR, RADAR, State, Debug, and Motor packets
-- This parser demonstrates the Lua packet parsing API

log("Loading legacy binary parser for telemetry protocol")

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

-- Optimization: Check if a packet or its signals are actually needed
local function is_packet_needed(packetType, signals)
    -- If there's an on_packet() callback, we MUST parse the packet
    if has_packet_callback(packetType) then
        return true
    end

    -- If any individual signal in this packet is active in the UI, we parse it
    for _, signalName in ipairs(signals) do
        if is_signal_active(packetType .. "." .. signalName) then
            return true
        end
    end

    return false
end

-- Register all protocol signals at startup so they are visible in the Signal Browser
local function register_all_signals()
    log("Registering all protocol signals for Signal Browser...")

    -- IMU Signals
    create_signal("IMU.time")
    create_signal("IMU.accelX")
    create_signal("IMU.accelY")
    create_signal("IMU.accelZ")
    create_signal("IMU.gyroX")
    create_signal("IMU.gyroY")
    create_signal("IMU.gyroZ")
    create_signal("IMU.magX")
    create_signal("IMU.magY")
    create_signal("IMU.magZ")
    create_signal("IMU.temperature")
    for i=0,8 do
        create_signal("IMU.accelCov["..i.."]")
        create_signal("IMU.gyroCov["..i.."]")
        create_signal("IMU.magCov["..i.."]")
    end
    create_signal("IMU.accelCalibStatus")
    create_signal("IMU.gyroCalibStatus")
    create_signal("IMU.magCalibStatus")

    -- GPS Signals (mapped to "GPS" prefix as used in is_packet_needed)
    create_signal("GPS.time")
    create_signal("GPS.latitude")
    create_signal("GPS.longitude")
    create_signal("GPS.altitude")
    create_signal("GPS.speed")
    create_signal("GPS.heading")
    create_signal("GPS.verticalSpeed")
    for i=0,8 do
        create_signal("GPS.posCov["..i.."]")
        create_signal("GPS.velCov["..i.."]")
    end
    create_signal("GPS.numSatellites")
    create_signal("GPS.fixType")
    create_signal("GPS.hdop")
    create_signal("GPS.vdop")
    for i=0,11 do
        create_signal("GPS.satellites["..i.."].prn")
        create_signal("GPS.satellites["..i.."].snr")
        create_signal("GPS.satellites["..i.."].elevation")
        create_signal("GPS.satellites["..i.."].azimuth")
    end

    -- Battery Signals
    create_signal("Battery.time")
    create_signal("Battery.voltage")
    create_signal("Battery.current")
    create_signal("Battery.temperature")
    create_signal("Battery.percentage")
    create_signal("Battery.health")
    create_signal("Battery.cycleCount")
    for i=0,15 do
        create_signal("Battery.cells["..i.."].voltage")
        create_signal("Battery.cells["..i.."].temperature")
        create_signal("Battery.cells["..i.."].health")
        create_signal("Battery.cells["..i.."].balancing")
        create_signal("Battery.cells["..i.."].resistance")
    end
    create_signal("Battery.maxCellVoltage")
    create_signal("Battery.minCellVoltage")
    create_signal("Battery.avgCellVoltage")
    create_signal("Battery.maxCellTemp")
    create_signal("Battery.minCellTemp")
    create_signal("Battery.avgCellTemp")
    create_signal("Battery.powerOut")
    create_signal("Battery.energyConsumed")
    create_signal("Battery.energyRemaining")
    create_signal("Battery.timeToEmpty")
    create_signal("Battery.timeToFull")
    create_signal("Battery.chargeState")
    create_signal("Battery.faultFlags")

    -- LIDAR Signals
    create_signal("LIDAR.time")
    create_signal("LIDAR.range")
    create_signal("LIDAR.intensity")
    create_signal("LIDAR.angleX")
    create_signal("LIDAR.angleY")
    create_signal("LIDAR.quality")
    create_signal("LIDAR.flags")
    for i=0,31 do
        create_signal("LIDAR.tracks["..i.."].range")
        create_signal("LIDAR.tracks["..i.."].azimuth")
        create_signal("LIDAR.tracks["..i.."].elevation")
        create_signal("LIDAR.tracks["..i.."].velocity")
        create_signal("LIDAR.tracks["..i.."].rcs")
        create_signal("LIDAR.tracks["..i.."].trackID")
        create_signal("LIDAR.tracks["..i.."].confidence")
        create_signal("LIDAR.tracks["..i.."].age")
    end
    create_signal("LIDAR.numTracks")

    -- RADAR Signals
    create_signal("RADAR.time")
    create_signal("RADAR.range")
    create_signal("RADAR.velocity")
    create_signal("RADAR.azimuth")
    create_signal("RADAR.elevation")
    create_signal("RADAR.signalStrength")
    create_signal("RADAR.targetCount")
    create_signal("RADAR.trackID")
    for i=0,23 do
        create_signal("RADAR.tracks["..i.."].range")
        create_signal("RADAR.tracks["..i.."].azimuth")
        create_signal("RADAR.tracks["..i.."].elevation")
        create_signal("RADAR.tracks["..i.."].velocity")
        create_signal("RADAR.tracks["..i.."].rangeRate")
        create_signal("RADAR.tracks["..i.."].rcs")
        create_signal("RADAR.tracks["..i.."].snr")
        create_signal("RADAR.tracks["..i.."].trackID")
        create_signal("RADAR.tracks["..i.."].trackStatus")
        create_signal("RADAR.tracks["..i.."].classification")
        create_signal("RADAR.tracks["..i.."].confidence")
        create_signal("RADAR.tracks["..i.."].age")
        create_signal("RADAR.tracks["..i.."].hits")
    end
    create_signal("RADAR.ambientNoise")
    create_signal("RADAR.temperature")
    create_signal("RADAR.mode")
    create_signal("RADAR.interference")

    -- State Signals
    create_signal("State.time")
    create_signal("State.systemMode")
    create_signal("State.armed")
    create_signal("State.statusFlags")
    create_signal("State.errorCode")
    create_signal("State.uptime")
    create_signal("State.cpuUsage")
    create_signal("State.memoryUsage")
    create_signal("State.diskUsage")
    create_signal("State.networkTxRate")
    create_signal("State.networkRxRate")
    create_signal("State.gpuUsage")
    create_signal("State.gpuMemoryUsage")
    create_signal("State.gpuTemperature")
    create_signal("State.cpuTemperature")
    create_signal("State.boardTemperature")
    for i=0,15 do
        create_signal("State.subsystems["..i.."].status")
        create_signal("State.subsystems["..i.."].health")
        create_signal("State.subsystems["..i.."].errorCode")
        create_signal("State.subsystems["..i.."].cpuUsage")
        create_signal("State.subsystems["..i.."].memUsage")
    end
    for i=0,7 do
        create_signal("State.errorHistory["..i.."].errorCode")
        create_signal("State.errorHistory["..i.."].timestamp")
        create_signal("State.errorHistory["..i.."].severity")
        create_signal("State.errorHistory["..i.."].subsystem")
    end
    create_signal("State.errorHistoryCount")

    -- Debug Signals
    create_signal("Debug.time")
    create_signal("Debug.counter")
    create_signal("Debug.eventID")
    create_signal("Debug.priority")
    create_signal("Debug.subsystem")
    create_signal("Debug.value1")
    create_signal("Debug.value2")
    create_signal("Debug.metric")
    for i=0,31 do
        create_signal("Debug.metrics["..i.."]")
        create_signal("Debug.values["..i.."]")
    end
    for i=0,7 do
        create_signal("Debug.counters["..i.."].count")
        create_signal("Debug.counters["..i.."].rate")
        create_signal("Debug.counters["..i.."].average")
    end
    for i=0,15 do
        create_signal("Debug.stackTrace["..i.."].address")
        create_signal("Debug.stackTrace["..i.."].offset")
        create_signal("Debug.stackTrace["..i.."].line")
    end
    create_signal("Debug.stackDepth")

    -- Motor Signals
    create_signal("Motor.time")
    create_signal("Motor.rpm")
    create_signal("Motor.torque")
    create_signal("Motor.power")
    create_signal("Motor.temperature")
    create_signal("Motor.throttle")
    create_signal("Motor.faults")
    create_signal("Motor.totalRotations")
    create_signal("Motor.voltage")
    create_signal("Motor.current")
    create_signal("Motor.backEMF")
    create_signal("Motor.efficiency")
    create_signal("Motor.dutyCycle")
    create_signal("Motor.motorTemp")
    create_signal("Motor.controllerTemp")
    create_signal("Motor.targetRPM")
    create_signal("Motor.rpmError")
    create_signal("Motor.phaseA_current")
    create_signal("Motor.phaseB_current")
    create_signal("Motor.phaseC_current")
    create_signal("Motor.phaseA_voltage")
    create_signal("Motor.phaseB_voltage")
    create_signal("Motor.phaseC_voltage")
    create_signal("Motor.pidP")
    create_signal("Motor.pidI")
    create_signal("Motor.pidD")
    create_signal("Motor.pidOutput")
    create_signal("Motor.vibration")
    create_signal("Motor.acousticNoise")
    create_signal("Motor.runTime")
    create_signal("Motor.startCount")
    create_signal("Motor.warningFlags")
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
        -- Define all signals in this packet for the activity check
        local imuSignals = { "time", "accelX", "accelY", "accelZ", "gyroX", "gyroY", "gyroZ", "magX", "magY", "magZ", "temperature" }
        -- Add covariance and status signals to the check list
        for i=0,8 do 
            table.insert(imuSignals, "accelCov["..i.."]")
            table.insert(imuSignals, "gyroCov["..i.."]")
            table.insert(imuSignals, "magCov["..i.."]")
        end
        table.insert(imuSignals, "accelCalibStatus")
        table.insert(imuSignals, "gyroCalibStatus")
        table.insert(imuSignals, "magCalibStatus")

        if not is_packet_needed("IMU", imuSignals) then
            return true -- Packet handled (skipped)
        end

        local time = readDouble(buffer, 4, true)
        if not time then return true end

        update_signal("IMU.time", time, time)
        
        if is_signal_active("IMU.accelX") then update_signal("IMU.accelX", time, readField(buffer, "float", 12)) end
        if is_signal_active("IMU.accelY") then update_signal("IMU.accelY", time, readField(buffer, "float", 16)) end
        if is_signal_active("IMU.accelZ") then update_signal("IMU.accelZ", time, readField(buffer, "float", 20)) end
        if is_signal_active("IMU.gyroX") then update_signal("IMU.gyroX", time, readField(buffer, "float", 24)) end
        if is_signal_active("IMU.gyroY") then update_signal("IMU.gyroY", time, readField(buffer, "float", 28)) end
        if is_signal_active("IMU.gyroZ") then update_signal("IMU.gyroZ", time, readField(buffer, "float", 32)) end
        if is_signal_active("IMU.magX") then update_signal("IMU.magX", time, readField(buffer, "float", 36)) end
        if is_signal_active("IMU.magY") then update_signal("IMU.magY", time, readField(buffer, "float", 40)) end
        if is_signal_active("IMU.magZ") then update_signal("IMU.magZ", time, readField(buffer, "float", 44)) end
        if is_signal_active("IMU.temperature") then update_signal("IMU.temperature", time, readField(buffer, "float", 48)) end

        -- Covariance matrices (9 elements each)
        for i = 0, 8 do
            if is_signal_active("IMU.accelCov[" .. i .. "]") then update_signal("IMU.accelCov[" .. i .. "]", time, readField(buffer, "float", 52 + i * 4)) end
            if is_signal_active("IMU.gyroCov[" .. i .. "]") then update_signal("IMU.gyroCov[" .. i .. "]", time, readField(buffer, "float", 88 + i * 4)) end
            if is_signal_active("IMU.magCov[" .. i .. "]") then update_signal("IMU.magCov[" .. i .. "]", time, readField(buffer, "float", 124 + i * 4)) end
        end

        -- Calibration status
        if is_signal_active("IMU.accelCalibStatus") then update_signal("IMU.accelCalibStatus", time, readField(buffer, "uint8_t", 160)) end
        if is_signal_active("IMU.gyroCalibStatus") then update_signal("IMU.gyroCalibStatus", time, readField(buffer, "uint8_t", 161)) end
        if is_signal_active("IMU.magCalibStatus") then update_signal("IMU.magCalibStatus", time, readField(buffer, "uint8_t", 162)) end

        trigger_packet_callbacks("IMU", time)
        return true
    end

    -- GPS Packet (174 bytes with expanded fields)
    if header == "GPS" and length >= 174 then
        -- Define relevant signals for activity check
        local gpsSignals = { "time", "latitude", "longitude", "altitude", "speed", "heading", "verticalSpeed", "numSatellites", "fixType", "hdop", "vdop" }
        for i=0,8 do table.insert(gpsSignals, "posCov["..i.."]") end
        for i=0,8 do table.insert(gpsSignals, "velCov["..i.."]") end
        for i=0,11 do 
            table.insert(gpsSignals, "satellites["..i.."].prn")
            table.insert(gpsSignals, "satellites["..i.."].snr")
            table.insert(gpsSignals, "satellites["..i.."].elevation")
            table.insert(gpsSignals, "satellites["..i.."].azimuth")
        end

        if not is_packet_needed("GPS", gpsSignals) then
            return true
        end

        local time = readDouble(buffer, 4, true)
        if not time then return true end

        update_signal("GPS.time", time, time)
        if is_signal_active("GPS.latitude") then update_signal("GPS.latitude", time, readField(buffer, "double", 12)) end
        if is_signal_active("GPS.longitude") then update_signal("GPS.longitude", time, readField(buffer, "double", 20)) end
        if is_signal_active("GPS.altitude") then update_signal("GPS.altitude", time, readField(buffer, "float", 28)) end
        if is_signal_active("GPS.speed") then update_signal("GPS.speed", time, readField(buffer, "float", 32)) end
        if is_signal_active("GPS.heading") then update_signal("GPS.heading", time, readField(buffer, "float", 36)) end
        if is_signal_active("GPS.verticalSpeed") then update_signal("GPS.verticalSpeed", time, readField(buffer, "float", 40)) end

        -- Position and velocity covariance (9 elements each)
        for i = 0, 8 do
            if is_signal_active("GPS.posCov[" .. i .. "]") then update_signal("GPS.posCov[" .. i .. "]", time, readField(buffer, "float", 44 + i * 4)) end
            if is_signal_active("GPS.velCov[" .. i .. "]") then update_signal("GPS.velCov[" .. i .. "]", time, readField(buffer, "float", 80 + i * 4)) end
        end

        -- GPS quality metrics
        if is_signal_active("GPS.numSatellites") then update_signal("GPS.numSatellites", time, readField(buffer, "uint8_t", 116)) end
        if is_signal_active("GPS.fixType") then update_signal("GPS.fixType", time, readField(buffer, "uint8_t", 117)) end
        if is_signal_active("GPS.hdop") then update_signal("GPS.hdop", time, readField(buffer, "float", 118)) end
        if is_signal_active("GPS.vdop") then update_signal("GPS.vdop", time, readField(buffer, "float", 122)) end

        -- Satellite info (12 satellites, 4 bytes each)
        for i = 0, 11 do
            local offset = 126 + i * 4
            if is_signal_active("GPS.satellites[" .. i .. "].prn") then update_signal("GPS.satellites[" .. i .. "].prn", time, readField(buffer, "uint8_t", offset)) end
            if is_signal_active("GPS.satellites[" .. i .. "].snr") then update_signal("GPS.satellites[" .. i .. "].snr", time, readField(buffer, "uint8_t", offset + 1)) end
            if is_signal_active("GPS.satellites[" .. i .. "].elevation") then update_signal("GPS.satellites[" .. i .. "].elevation", time, readField(buffer, "uint8_t", offset + 2)) end
            if is_signal_active("GPS.satellites[" .. i .. "].azimuth") then update_signal("GPS.satellites[" .. i .. "].azimuth", time, readField(buffer, "uint8_t", offset + 3)) end
        end

        trigger_packet_callbacks("GPS", time)
        return true
    end

    -- Battery Packet (268 bytes with expanded fields)
    if header == "BAT" and length >= 268 then
        -- Define relevant signals for activity check
        local batSignals = { "time", "voltage", "current", "temperature", "percentage", "health", "cycleCount" }
        for i=0,15 do 
            table.insert(batSignals, "cells["..i.."].voltage")
            table.insert(batSignals, "cells["..i.."].temperature")
            table.insert(batSignals, "cells["..i.."].health")
            table.insert(batSignals, "cells["..i.."].balancing")
            table.insert(batSignals, "cells["..i.."].resistance")
        end
        local extraSignals = { "maxCellVoltage", "minCellVoltage", "avgCellVoltage", "maxCellTemp", "minCellTemp", "avgCellTemp", "powerOut", "energyConsumed", "energyRemaining", "timeToEmpty", "timeToFull", "chargeState", "faultFlags" }
        for _, s in ipairs(extraSignals) do table.insert(batSignals, s) end

        if not is_packet_needed("Battery", batSignals) then
            return true
        end

        local time = readDouble(buffer, 4, true)
        if not time then return true end

        update_signal("Battery.time", time, time)
        if is_signal_active("Battery.voltage") then update_signal("Battery.voltage", time, readField(buffer, "float", 12)) end
        if is_signal_active("Battery.current") then update_signal("Battery.current", time, readField(buffer, "float", 16)) end
        if is_signal_active("Battery.temperature") then update_signal("Battery.temperature", time, readField(buffer, "float", 20)) end
        if is_signal_active("Battery.percentage") then update_signal("Battery.percentage", time, readField(buffer, "uint8_t", 24)) end
        if is_signal_active("Battery.health") then update_signal("Battery.health", time, readField(buffer, "uint8_t", 25)) end
        if is_signal_active("Battery.cycleCount") then update_signal("Battery.cycleCount", time, readField(buffer, "uint16_t", 26)) end

        -- Cell-level diagnostics
        for i = 0, 15 do
            local offset = 28 + i * 10
            if is_signal_active("Battery.cells[" .. i .. "].voltage") then update_signal("Battery.cells[" .. i .. "].voltage", time, readField(buffer, "float", offset)) end
            if is_signal_active("Battery.cells[" .. i .. "].temperature") then update_signal("Battery.cells[" .. i .. "].temperature", time, readField(buffer, "float", offset + 4)) end
            if is_signal_active("Battery.cells[" .. i .. "].health") then update_signal("Battery.cells[" .. i .. "].health", time, readField(buffer, "uint8_t", offset + 8)) end
            if is_signal_active("Battery.cells[" .. i .. "].balancing") then update_signal("Battery.cells[" .. i .. "].balancing", time, readField(buffer, "uint8_t", offset + 9)) end
            if is_signal_active("Battery.cells[" .. i .. "].resistance") then update_signal("Battery.cells[" .. i .. "].resistance", time, readField(buffer, "uint16_t", offset + 10)) end
        end

        -- Extended battery pack status
        if is_signal_active("Battery.maxCellVoltage") then update_signal("Battery.maxCellVoltage", time, readField(buffer, "float", 188)) end
        if is_signal_active("Battery.minCellVoltage") then update_signal("Battery.minCellVoltage", time, readField(buffer, "float", 192)) end
        if is_signal_active("Battery.avgCellVoltage") then update_signal("Battery.avgCellVoltage", time, readField(buffer, "float", 196)) end
        if is_signal_active("Battery.maxCellTemp") then update_signal("Battery.maxCellTemp", time, readField(buffer, "float", 200)) end
        if is_signal_active("Battery.minCellTemp") then update_signal("Battery.minCellTemp", time, readField(buffer, "float", 204)) end
        if is_signal_active("Battery.avgCellTemp") then update_signal("Battery.avgCellTemp", time, readField(buffer, "float", 208)) end
        if is_signal_active("Battery.powerOut") then update_signal("Battery.powerOut", time, readField(buffer, "float", 212)) end
        if is_signal_active("Battery.energyConsumed") then update_signal("Battery.energyConsumed", time, readField(buffer, "float", 216)) end
        if is_signal_active("Battery.energyRemaining") then update_signal("Battery.energyRemaining", time, readField(buffer, "float", 220)) end
        if is_signal_active("Battery.timeToEmpty") then update_signal("Battery.timeToEmpty", time, readField(buffer, "uint32_t", 224)) end
        if is_signal_active("Battery.timeToFull") then update_signal("Battery.timeToFull", time, readField(buffer, "uint32_t", 228)) end
        if is_signal_active("Battery.chargeState") then update_signal("Battery.chargeState", time, readField(buffer, "uint8_t", 232)) end
        if is_signal_active("Battery.faultFlags") then update_signal("Battery.faultFlags", time, readField(buffer, "uint8_t", 233)) end

        trigger_packet_callbacks("Battery", time)
        return true
    end

    -- LIDAR Packet (798 bytes with expanded fields)
    if header == "LID" and length >= 798 then
        -- Define relevant signals for activity check
        local lidSignals = { "time", "range", "intensity", "angleX", "angleY", "quality", "flags", "numTracks" }
        for i=0,31 do 
            table.insert(lidSignals, "tracks["..i.."].range")
            table.insert(lidSignals, "tracks["..i.."].azimuth")
            table.insert(lidSignals, "tracks["..i.."].elevation")
            table.insert(lidSignals, "tracks["..i.."].velocity")
            table.insert(lidSignals, "tracks["..i.."].rcs")
            table.insert(lidSignals, "tracks["..i.."].trackID")
            table.insert(lidSignals, "tracks["..i.."].confidence")
            table.insert(lidSignals, "tracks["..i.."].age")
        end

        if not is_packet_needed("LIDAR", lidSignals) then
            return true
        end

        local time = readDouble(buffer, 4, true)
        if not time then return true end

        update_signal("LIDAR.time", time, time)
        if is_signal_active("LIDAR.range") then update_signal("LIDAR.range", time, readField(buffer, "float", 12)) end
        if is_signal_active("LIDAR.intensity") then update_signal("LIDAR.intensity", time, readField(buffer, "float", 16)) end
        if is_signal_active("LIDAR.angleX") then update_signal("LIDAR.angleX", time, readField(buffer, "int16_t", 20)) end
        if is_signal_active("LIDAR.angleY") then update_signal("LIDAR.angleY", time, readField(buffer, "int16_t", 22)) end
        if is_signal_active("LIDAR.quality") then update_signal("LIDAR.quality", time, readField(buffer, "uint8_t", 24)) end
        if is_signal_active("LIDAR.flags") then update_signal("LIDAR.flags", time, readField(buffer, "uint8_t", 25)) end

        -- Track data (32 tracks)
        for i = 0, 31 do
            local offset = 26 + i * 24
            if is_signal_active("LIDAR.tracks[" .. i .. "].range") then update_signal("LIDAR.tracks[" .. i .. "].range", time, readField(buffer, "float", offset)) end
            if is_signal_active("LIDAR.tracks[" .. i .. "].azimuth") then update_signal("LIDAR.tracks[" .. i .. "].azimuth", time, readField(buffer, "float", offset + 4)) end
            if is_signal_active("LIDAR.tracks[" .. i .. "].elevation") then update_signal("LIDAR.tracks[" .. i .. "].elevation", time, readField(buffer, "float", offset + 8)) end
            if is_signal_active("LIDAR.tracks[" .. i .. "].velocity") then update_signal("LIDAR.tracks[" .. i .. "].velocity", time, readField(buffer, "float", offset + 12)) end
            if is_signal_active("LIDAR.tracks[" .. i .. "].rcs") then update_signal("LIDAR.tracks[" .. i .. "].rcs", time, readField(buffer, "float", offset + 16)) end
            if is_signal_active("LIDAR.tracks[" .. i .. "].trackID") then update_signal("LIDAR.tracks[" .. i .. "].trackID", time, readField(buffer, "uint8_t", offset + 20)) end
            if is_signal_active("LIDAR.tracks[" .. i .. "].confidence") then update_signal("LIDAR.tracks[" .. i .. "].confidence", time, readField(buffer, "uint8_t", offset + 21)) end
            if is_signal_active("LIDAR.tracks[" .. i .. "].age") then update_signal("LIDAR.tracks[" .. i .. "].age", time, readField(buffer, "uint16_t", offset + 22)) end
        end

        if is_signal_active("LIDAR.numTracks") then update_signal("LIDAR.numTracks", time, readField(buffer, "uint8_t", 794)) end

        trigger_packet_callbacks("LIDAR", time)
        return true
    end

    -- RADAR Packet (908 bytes with expanded fields)
    if header == "RAD" and length >= 908 then
        -- Define relevant signals for activity check
        local radSignals = { "time", "range", "velocity", "azimuth", "elevation", "signalStrength", "targetCount", "trackID", "ambientNoise", "temperature", "mode", "interference" }
        for i=0,23 do 
            table.insert(radSignals, "tracks["..i.."].range")
            table.insert(radSignals, "tracks["..i.."].azimuth")
            table.insert(radSignals, "tracks["..i.."].elevation")
            table.insert(radSignals, "tracks["..i.."].velocity")
            table.insert(radSignals, "tracks["..i.."].rangeRate")
            table.insert(radSignals, "tracks["..i.."].rcs")
            table.insert(radSignals, "tracks["..i.."].snr")
            table.insert(radSignals, "tracks["..i.."].trackID")
            table.insert(radSignals, "tracks["..i.."].trackStatus")
            table.insert(radSignals, "tracks["..i.."].classification")
            table.insert(radSignals, "tracks["..i.."].confidence")
            table.insert(radSignals, "tracks["..i.."].age")
            table.insert(radSignals, "tracks["..i.."].hits")
        end

        if not is_packet_needed("RADAR", radSignals) then
            return true
        end

        local time = readDouble(buffer, 4, true)
        if not time then return true end

        update_signal("RADAR.time", time, time)
        if is_signal_active("RADAR.range") then update_signal("RADAR.range", time, readField(buffer, "float", 12)) end
        if is_signal_active("RADAR.velocity") then update_signal("RADAR.velocity", time, readField(buffer, "float", 16)) end
        if is_signal_active("RADAR.azimuth") then update_signal("RADAR.azimuth", time, readField(buffer, "float", 20)) end
        if is_signal_active("RADAR.elevation") then update_signal("RADAR.elevation", time, readField(buffer, "float", 24)) end
        if is_signal_active("RADAR.signalStrength") then update_signal("RADAR.signalStrength", time, readField(buffer, "int16_t", 28)) end
        if is_signal_active("RADAR.targetCount") then update_signal("RADAR.targetCount", time, readField(buffer, "uint8_t", 30)) end
        if is_signal_active("RADAR.trackID") then update_signal("RADAR.trackID", time, readField(buffer, "uint8_t", 31)) end

        -- Track data (24 tracks)
        for i = 0, 23 do
            local offset = 32 + i * 36
            if is_signal_active("RADAR.tracks[" .. i .. "].range") then update_signal("RADAR.tracks[" .. i .. "].range", time, readField(buffer, "float", offset)) end
            if is_signal_active("RADAR.tracks[" .. i .. "].azimuth") then update_signal("RADAR.tracks[" .. i .. "].azimuth", time, readField(buffer, "float", offset + 4)) end
            if is_signal_active("RADAR.tracks[" .. i .. "].elevation") then update_signal("RADAR.tracks[" .. i .. "].elevation", time, readField(buffer, "float", offset + 8)) end
            if is_signal_active("RADAR.tracks[" .. i .. "].velocity") then update_signal("RADAR.tracks[" .. i .. "].velocity", time, readField(buffer, "float", offset + 12)) end
            if is_signal_active("RADAR.tracks[" .. i .. "].rangeRate") then update_signal("RADAR.tracks[" .. i .. "].rangeRate", time, readField(buffer, "float", offset + 16)) end
            if is_signal_active("RADAR.tracks[" .. i .. "].rcs") then update_signal("RADAR.tracks[" .. i .. "].rcs", time, readField(buffer, "float", offset + 20)) end
            if is_signal_active("RADAR.tracks[" .. i .. "].snr") then update_signal("RADAR.tracks[" .. i .. "].snr", time, readField(buffer, "float", offset + 24)) end
            if is_signal_active("RADAR.tracks[" .. i .. "].trackID") then update_signal("RADAR.tracks[" .. i .. "].trackID", time, readField(buffer, "uint8_t", offset + 28)) end
            if is_signal_active("RADAR.tracks[" .. i .. "].trackStatus") then update_signal("RADAR.tracks[" .. i .. "].trackStatus", time, readField(buffer, "uint8_t", offset + 29)) end
            if is_signal_active("RADAR.tracks[" .. i .. "].classification") then update_signal("RADAR.tracks[" .. i .. "].classification", time, readField(buffer, "uint8_t", offset + 30)) end
            if is_signal_active("RADAR.tracks[" .. i .. "].confidence") then update_signal("RADAR.tracks[" .. i .. "].confidence", time, readField(buffer, "uint8_t", offset + 31)) end
            if is_signal_active("RADAR.tracks[" .. i .. "].age") then update_signal("RADAR.tracks[" .. i .. "].age", time, readField(buffer, "uint16_t", offset + 32)) end
            if is_signal_active("RADAR.tracks[" .. i .. "].hits") then update_signal("RADAR.tracks[" .. i .. "].hits", time, readField(buffer, "uint16_t", offset + 34)) end
        end

        -- Radar status fields
        if is_signal_active("RADAR.ambientNoise") then update_signal("RADAR.ambientNoise", time, readField(buffer, "float", 896)) end
        if is_signal_active("RADAR.temperature") then update_signal("RADAR.temperature", time, readField(buffer, "float", 900)) end
        if is_signal_active("RADAR.mode") then update_signal("RADAR.mode", time, readField(buffer, "uint8_t", 904)) end
        if is_signal_active("RADAR.interference") then update_signal("RADAR.interference", time, readField(buffer, "uint8_t", 905)) end

        trigger_packet_callbacks("RADAR", time)
        return true
    end

    -- State Packet (420 bytes with expanded fields)
    if header == "STA" and length >= 420 then
        -- Define relevant signals for activity check
        local stateSignals = { "time", "systemMode", "armed", "statusFlags", "errorCode", "uptime", "cpuUsage", "memoryUsage", "diskUsage", "networkTxRate", "networkRxRate", "gpuUsage", "gpuMemoryUsage", "gpuTemperature", "cpuTemperature", "boardTemperature", "errorHistoryCount" }
        for i=0,15 do 
            table.insert(stateSignals, "subsystems["..i.."].status")
            table.insert(stateSignals, "subsystems["..i.."].health")
            table.insert(stateSignals, "subsystems["..i.."].errorCode")
            table.insert(stateSignals, "subsystems["..i.."].cpuUsage")
            table.insert(stateSignals, "subsystems["..i.."].memUsage")
        end
        for i=0,7 do
            table.insert(stateSignals, "errorHistory["..i.."].errorCode")
            table.insert(stateSignals, "errorHistory["..i.."].timestamp")
            table.insert(stateSignals, "errorHistory["..i.."].severity")
            table.insert(stateSignals, "errorHistory["..i.."].subsystem")
        end

        if not is_packet_needed("State", stateSignals) then
            return true
        end

        local time = readDouble(buffer, 4, true)
        if not time then return true end

        update_signal("State.time", time, time)
        if is_signal_active("State.systemMode") then update_signal("State.systemMode", time, readField(buffer, "uint8_t", 12)) end
        if is_signal_active("State.armed") then update_signal("State.armed", time, readField(buffer, "uint8_t", 13)) end
        if is_signal_active("State.statusFlags") then update_signal("State.statusFlags", time, readField(buffer, "uint16_t", 14)) end
        if is_signal_active("State.errorCode") then update_signal("State.errorCode", time, readField(buffer, "int32_t", 16)) end
        if is_signal_active("State.uptime") then update_signal("State.uptime", time, readField(buffer, "uint32_t", 20)) end
        if is_signal_active("State.cpuUsage") then update_signal("State.cpuUsage", time, readField(buffer, "float", 24)) end
        if is_signal_active("State.memoryUsage") then update_signal("State.memoryUsage", time, readField(buffer, "float", 28)) end
        if is_signal_active("State.diskUsage") then update_signal("State.diskUsage", time, readField(buffer, "float", 32)) end
        if is_signal_active("State.networkTxRate") then update_signal("State.networkTxRate", time, readField(buffer, "float", 36)) end
        if is_signal_active("State.networkRxRate") then update_signal("State.networkRxRate", time, readField(buffer, "float", 40)) end
        if is_signal_active("State.gpuUsage") then update_signal("State.gpuUsage", time, readField(buffer, "float", 44)) end
        if is_signal_active("State.gpuMemoryUsage") then update_signal("State.gpuMemoryUsage", time, readField(buffer, "float", 48)) end
        if is_signal_active("State.gpuTemperature") then update_signal("State.gpuTemperature", time, readField(buffer, "float", 52)) end
        if is_signal_active("State.cpuTemperature") then update_signal("State.cpuTemperature", time, readField(buffer, "float", 56)) end
        if is_signal_active("State.boardTemperature") then update_signal("State.boardTemperature", time, readField(buffer, "float", 60)) end

        -- Subsystem health
        for i = 0, 15 do
            local offset = 64 + i * 14
            if is_signal_active("State.subsystems[" .. i .. "].status") then update_signal("State.subsystems[" .. i .. "].status", time, readField(buffer, "uint8_t", offset)) end
            if is_signal_active("State.subsystems[" .. i .. "].health") then update_signal("State.subsystems[" .. i .. "].health", time, readField(buffer, "uint8_t", offset + 1)) end
            if is_signal_active("State.subsystems[" .. i .. "].errorCode") then update_signal("State.subsystems[" .. i .. "].errorCode", time, readField(buffer, "uint16_t", offset + 2)) end
            if is_signal_active("State.subsystems[" .. i .. "].cpuUsage") then update_signal("State.subsystems[" .. i .. "].cpuUsage", time, readField(buffer, "float", offset + 4)) end
            if is_signal_active("State.subsystems[" .. i .. "].memUsage") then update_signal("State.subsystems[" .. i .. "].memUsage", time, readField(buffer, "float", offset + 8)) end
        end

        -- Error history
        for i = 0, 7 do
            local offset = 288 + i * 12
            if is_signal_active("State.errorHistory[" .. i .. "].errorCode") then update_signal("State.errorHistory[" .. i .. "].errorCode", time, readField(buffer, "uint32_t", offset)) end
            if is_signal_active("State.errorHistory[" .. i .. "].timestamp") then update_signal("State.errorHistory[" .. i .. "].timestamp", time, readField(buffer, "uint32_t", offset + 4)) end
            if is_signal_active("State.errorHistory[" .. i .. "].severity") then update_signal("State.errorHistory[" .. i .. "].severity", time, readField(buffer, "uint8_t", offset + 8)) end
            if is_signal_active("State.errorHistory[" .. i .. "].subsystem") then update_signal("State.errorHistory[" .. i .. "].subsystem", time, readField(buffer, "uint8_t", offset + 9)) end
        end

        if is_signal_active("State.errorHistoryCount") then update_signal("State.errorHistoryCount", time, readField(buffer, "uint8_t", 384)) end

        trigger_packet_callbacks("State", time)
        return true
    end

    -- Debug Packet (874 bytes with expanded fields)
    if header == "DBG" and length >= 874 then
        -- Define relevant signals for activity check
        local dbgSignals = { "time", "counter", "eventID", "priority", "subsystem", "value1", "value2", "metric", "stackDepth" }
        for i=0,31 do table.insert(dbgSignals, "metrics["..i.."]") end
        for i=0,31 do table.insert(dbgSignals, "values["..i.."]") end
        for i=0,7 do
            table.insert(dbgSignals, "counters["..i.."].count")
            table.insert(dbgSignals, "counters["..i.."].rate")
            table.insert(dbgSignals, "counters["..i.."].average")
        end
        for i=0,15 do
            table.insert(dbgSignals, "stackTrace["..i.."].address")
            table.insert(dbgSignals, "stackTrace["..i.."].offset")
            table.insert(dbgSignals, "stackTrace["..i.."].line")
        end

        if not is_packet_needed("Debug", dbgSignals) then
            return true
        end

        local time = readDouble(buffer, 4, true)
        if not time then return true end

        update_signal("Debug.time", time, time)
        if is_signal_active("Debug.counter") then update_signal("Debug.counter", time, readField(buffer, "int64_t", 12)) end
        if is_signal_active("Debug.eventID") then update_signal("Debug.eventID", time, readField(buffer, "uint32_t", 20)) end
        if is_signal_active("Debug.priority") then update_signal("Debug.priority", time, readField(buffer, "int8_t", 24)) end
        if is_signal_active("Debug.subsystem") then update_signal("Debug.subsystem", time, readField(buffer, "uint8_t", 25)) end
        if is_signal_active("Debug.value1") then update_signal("Debug.value1", time, readField(buffer, "int16_t", 26)) end
        if is_signal_active("Debug.value2") then update_signal("Debug.value2", time, readField(buffer, "int16_t", 28)) end
        if is_signal_active("Debug.metric") then update_signal("Debug.metric", time, readField(buffer, "float", 30)) end

        -- Extended debug data: metrics array
        for i = 0, 31 do
            if is_signal_active("Debug.metrics[" .. i .. "]") then update_signal("Debug.metrics[" .. i .. "]", time, readField(buffer, "float", 34 + i * 4)) end
        end

        -- Values array
        for i = 0, 31 do
            if is_signal_active("Debug.values[" .. i .. "]") then update_signal("Debug.values[" .. i .. "]", time, readField(buffer, "int32_t", 162 + i * 4)) end
        end

        -- Performance counters
        for i = 0, 7 do
            local offset = 290 + i * 40
            if is_signal_active("Debug.counters[" .. i .. "].count") then update_signal("Debug.counters[" .. i .. "].count", time, readField(buffer, "uint64_t", offset + 16)) end
            if is_signal_active("Debug.counters[" .. i .. "].rate") then update_signal("Debug.counters[" .. i .. "].rate", time, readField(buffer, "double", offset + 24)) end
            if is_signal_active("Debug.counters[" .. i .. "].average") then update_signal("Debug.counters[" .. i .. "].average", time, readField(buffer, "double", offset + 32)) end
        end

        -- Stack trace
        for i = 0, 15 do
            local offset = 610 + i * 16
            if is_signal_active("Debug.stackTrace[" .. i .. "].address") then update_signal("Debug.stackTrace[" .. i .. "].address", time, readField(buffer, "uint64_t", offset)) end
            if is_signal_active("Debug.stackTrace[" .. i .. "].offset") then update_signal("Debug.stackTrace[" .. i .. "].offset", time, readField(buffer, "uint32_t", offset + 8)) end
            if is_signal_active("Debug.stackTrace[" .. i .. "].line") then update_signal("Debug.stackTrace[" .. i .. "].line", time, readField(buffer, "uint32_t", offset + 12)) end
        end

        if is_signal_active("Debug.stackDepth") then update_signal("Debug.stackDepth", time, readField(buffer, "uint8_t", 866)) end

        trigger_packet_callbacks("Debug", time)
        return true
    end

    -- Motor Packet (126 bytes with expanded fields)
    if header == "MTR" and length >= 126 then
        -- Define relevant signals for activity check
        local mtrSignals = { "time", "rpm", "torque", "power", "temperature", "throttle", "faults", "totalRotations", "voltage", "current", "backEMF", "efficiency", "dutyCycle", "motorTemp", "controllerTemp", "targetRPM", "rpmError", "phaseA_current", "phaseB_current", "phaseC_current", "phaseA_voltage", "phaseB_voltage", "phaseC_voltage", "pidP", "pidI", "pidD", "pidOutput", "vibration", "acousticNoise", "runTime", "startCount", "warningFlags" }

        if not is_packet_needed("Motor", mtrSignals) then
            return true
        end

        local time = readDouble(buffer, 4, true)
        if not time then return true end

        update_signal("Motor.time", time, time)
        if is_signal_active("Motor.rpm") then update_signal("Motor.rpm", time, readField(buffer, "int16_t", 12)) end
        if is_signal_active("Motor.torque") then update_signal("Motor.torque", time, readField(buffer, "float", 14)) end
        if is_signal_active("Motor.power") then update_signal("Motor.power", time, readField(buffer, "float", 18)) end
        if is_signal_active("Motor.temperature") then update_signal("Motor.temperature", time, readField(buffer, "int8_t", 22)) end
        if is_signal_active("Motor.throttle") then update_signal("Motor.throttle", time, readField(buffer, "uint8_t", 23)) end
        if is_signal_active("Motor.faults") then update_signal("Motor.faults", time, readField(buffer, "uint16_t", 24)) end
        if is_signal_active("Motor.totalRotations") then update_signal("Motor.totalRotations", time, readField(buffer, "uint32_t", 26)) end

        -- Extended motor diagnostics
        if is_signal_active("Motor.voltage") then update_signal("Motor.voltage", time, readField(buffer, "float", 30)) end
        if is_signal_active("Motor.current") then update_signal("Motor.current", time, readField(buffer, "float", 34)) end
        if is_signal_active("Motor.backEMF") then update_signal("Motor.backEMF", time, readField(buffer, "float", 38)) end
        if is_signal_active("Motor.efficiency") then update_signal("Motor.efficiency", time, readField(buffer, "float", 42)) end
        if is_signal_active("Motor.dutyCycle") then update_signal("Motor.dutyCycle", time, readField(buffer, "float", 46)) end
        if is_signal_active("Motor.motorTemp") then update_signal("Motor.motorTemp", time, readField(buffer, "float", 50)) end
        if is_signal_active("Motor.controllerTemp") then update_signal("Motor.controllerTemp", time, readField(buffer, "float", 54)) end
        if is_signal_active("Motor.targetRPM") then update_signal("Motor.targetRPM", time, readField(buffer, "float", 58)) end
        if is_signal_active("Motor.rpmError") then update_signal("Motor.rpmError", time, readField(buffer, "float", 62)) end

        -- Phase currents
        if is_signal_active("Motor.phaseA_current") then update_signal("Motor.phaseA_current", time, readField(buffer, "float", 66)) end
        if is_signal_active("Motor.phaseB_current") then update_signal("Motor.phaseB_current", time, readField(buffer, "float", 70)) end
        if is_signal_active("Motor.phaseC_current") then update_signal("Motor.phaseC_current", time, readField(buffer, "float", 74)) end

        -- Phase voltages
        if is_signal_active("Motor.phaseA_voltage") then update_signal("Motor.phaseA_voltage", time, readField(buffer, "float", 78)) end
        if is_signal_active("Motor.phaseB_voltage") then update_signal("Motor.phaseB_voltage", time, readField(buffer, "float", 82)) end
        if is_signal_active("Motor.phaseC_voltage") then update_signal("Motor.phaseC_voltage", time, readField(buffer, "float", 86)) end

        -- PID control signals
        if is_signal_active("Motor.pidP") then update_signal("Motor.pidP", time, readField(buffer, "float", 90)) end
        if is_signal_active("Motor.pidI") then update_signal("Motor.pidI", time, readField(buffer, "float", 94)) end
        if is_signal_active("Motor.pidD") then update_signal("Motor.pidD", time, readField(buffer, "float", 98)) end
        if is_signal_active("Motor.pidOutput") then update_signal("Motor.pidOutput", time, readField(buffer, "float", 102)) end

        -- Health metrics
        if is_signal_active("Motor.vibration") then update_signal("Motor.vibration", time, readField(buffer, "float", 106)) end
        if is_signal_active("Motor.acousticNoise") then update_signal("Motor.acousticNoise", time, readField(buffer, "float", 110)) end
        if is_signal_active("Motor.runTime") then update_signal("Motor.runTime", time, readField(buffer, "uint32_t", 114)) end
        if is_signal_active("Motor.startCount") then update_signal("Motor.startCount", time, readField(buffer, "uint32_t", 118)) end
        if is_signal_active("Motor.warningFlags") then update_signal("Motor.warningFlags", time, readField(buffer, "uint16_t", 122)) end

        trigger_packet_callbacks("Motor", time)
        return true
    end

    -- No packet matched
    return false
end)

-- Register all signals once at startup
register_all_signals()

log("Legacy binary parser registered successfully")
