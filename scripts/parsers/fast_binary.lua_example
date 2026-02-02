-- Fast FFI-based parser for binary telemetry protocol
-- Replaces legacy_binary.lua with Zero-Copy parsing and O(1) signal updates

local ffi = require("ffi")

log("Loading FAST binary parser (FFI-based)")

-- Define C structs matching src/telemetry_defs.h
ffi.cdef[[
    struct __attribute__((packed)) IMUData {
        char header[4];
        double time;
        float accelX; float accelY; float accelZ;
        float gyroX; float gyroY; float gyroZ;
        float magX; float magY; float magZ;
        float temperature;
        float accelCov[9];
        float gyroCov[9];
        float magCov[9];
        uint8_t accelCalibStatus;
        uint8_t gyroCalibStatus;
        uint8_t magCalibStatus;
        uint8_t padding;
    };

    struct __attribute__((packed)) GPSSatellite {
        uint8_t prn;
        uint8_t snr;
        uint8_t elevation;
        uint8_t azimuth;
    };

    struct __attribute__((packed)) GPSData {
        char header[4];
        double time;
        double latitude;
        double longitude;
        float altitude;
        float speed;
        float heading;
        float verticalSpeed;
        float posCov[9];
        float velCov[9];
        uint8_t numSatellites;
        uint8_t fixType;
        float hdop;
        float vdop;
        struct GPSSatellite satellites[12];
    };

    struct __attribute__((packed)) BatteryCell {
        float voltage;
        float temperature;
        uint8_t health;
        uint8_t balancing;
        uint16_t resistance;
    };

    struct __attribute__((packed)) BatteryData {
        char header[4];
        double time;
        float voltage;
        float current;
        float temperature;
        uint8_t percentage;
        uint8_t health;
        uint16_t cycleCount;
        struct BatteryCell cells[16];
        float maxCellVoltage;
        float minCellVoltage;
        float avgCellVoltage;
        float maxCellTemp;
        float minCellTemp;
        float avgCellTemp;
        float powerOut;
        float energyConsumed;
        float energyRemaining;
        uint32_t timeToEmpty;
        uint32_t timeToFull;
        uint8_t chargeState;
        uint8_t faultFlags;
        uint16_t padding;
    };

    struct __attribute__((packed)) LIDARTrack {
        float range;
        float azimuth;
        float elevation;
        float velocity;
        float rcs;
        uint8_t trackID;
        uint8_t confidence;
        uint16_t age;
    };

    struct __attribute__((packed)) LIDARData {
        char header[4];
        double time;
        float range;
        float intensity;
        int16_t angleX;
        int16_t angleY;
        uint8_t quality;
        uint8_t flags;
        struct LIDARTrack tracks[32];
        uint8_t numTracks;
        uint8_t padding[3];
    };

    struct __attribute__((packed)) RADARTrack {
        float range;
        float azimuth;
        float elevation;
        float velocity;
        float rangeRate;
        float rcs;
        float snr;
        uint8_t trackID;
        uint8_t trackStatus;
        uint8_t classification;
        uint8_t confidence;
        uint16_t age;
        uint16_t hits;
    };

    struct __attribute__((packed)) RADARData {
        char header[4];
        double time;
        float range;
        float velocity;
        float azimuth;
        float elevation;
        int16_t signalStrength;
        uint8_t targetCount;
        uint8_t trackID;
        struct RADARTrack tracks[24];
        float ambientNoise;
        float temperature;
        uint8_t mode;
        uint8_t interference;
        uint16_t padding;
    };

    struct __attribute__((packed)) SubsystemStatus {
        uint8_t status;
        uint8_t health;
        uint16_t errorCode;
        float cpuUsage;
        float memUsage;
        uint16_t padding;
    };

    struct __attribute__((packed)) ErrorEntry {
        uint32_t errorCode;
        uint32_t timestamp;
        uint8_t severity;
        uint8_t subsystem;
        uint16_t padding;
    };

    struct __attribute__((packed)) StateData {
        char header[4];
        double time;
        uint8_t systemMode;
        uint8_t armed;
        uint16_t statusFlags;
        int32_t errorCode;
        uint32_t uptime;
        float cpuUsage;
        float memoryUsage;
        float diskUsage;
        float networkTxRate;
        float networkRxRate;
        float gpuUsage;
        float gpuMemoryUsage;
        float gpuTemperature;
        float cpuTemperature;
        float boardTemperature;
        struct SubsystemStatus subsystems[16];
        struct ErrorEntry errorHistory[8];
        uint8_t errorHistoryCount;
        uint8_t padding[3];
    };

    struct __attribute__((packed)) DebugCounter {
        char name[16];
        uint64_t count;
        double rate;
        double average;
    };

    struct __attribute__((packed)) StackFrame {
        uint64_t address;
        uint32_t offset;
        uint32_t line;
    };

    struct __attribute__((packed)) DebugData {
        char header[4];
        double time;
        int64_t counter;
        uint32_t eventID;
        int8_t priority;
        uint8_t subsystem;
        int16_t value1;
        int16_t value2;
        float metric;
        float metrics[32];
        int32_t values[32];
        struct DebugCounter counters[8];
        struct StackFrame stackTrace[16];
        uint8_t stackDepth;
        uint8_t padding[7];
    };

    struct __attribute__((packed)) MotorData {
        char header[4];
        double time;
        int16_t rpm;
        float torque;
        float power;
        int8_t temperature;
        uint8_t throttle;
        uint16_t faults;
        uint32_t totalRotations;
        float voltage;
        float current;
        float backEMF;
        float efficiency;
        float dutyCycle;
        float motorTemp;
        float controllerTemp;
        float targetRPM;
        float rpmError;
        float phaseA_current;
        float phaseB_current;
        float phaseC_current;
        float phaseA_voltage;
        float phaseB_voltage;
        float phaseC_voltage;
        float pidP;
        float pidI;
        float pidD;
        float pidOutput;
        float vibration;
        float acousticNoise;
        uint32_t runTime;
        uint32_t startCount;
        uint16_t warningFlags;
        uint16_t padding;
    };
]]

-- Signal ID Cache (Pre-fetched at startup for O(1) access)
local sigs = {}

local function cache_signal(name)
    sigs[name] = get_signal_id(name)
end

-- Pre-cache all signal IDs
local function init_signal_cache()
    log("Caching signal IDs for fast parser...")
    
    -- IMU
    cache_signal("IMU.time")
    cache_signal("IMU.accelX"); cache_signal("IMU.accelY"); cache_signal("IMU.accelZ")
    cache_signal("IMU.gyroX");  cache_signal("IMU.gyroY");  cache_signal("IMU.gyroZ")
    cache_signal("IMU.magX");   cache_signal("IMU.magY");   cache_signal("IMU.magZ")
    cache_signal("IMU.temperature")
    for i=0,8 do
        cache_signal("IMU.accelCov["..i.."]")
        cache_signal("IMU.gyroCov["..i.."]")
        cache_signal("IMU.magCov["..i.."]")
    end
    cache_signal("IMU.accelCalibStatus")
    cache_signal("IMU.gyroCalibStatus")
    cache_signal("IMU.magCalibStatus")

    -- GPS
    cache_signal("GPS.time")
    cache_signal("GPS.latitude"); cache_signal("GPS.longitude"); cache_signal("GPS.altitude")
    cache_signal("GPS.speed"); cache_signal("GPS.heading"); cache_signal("GPS.verticalSpeed")
    for i=0,8 do cache_signal("GPS.posCov["..i.."]"); cache_signal("GPS.velCov["..i.."]") end
    cache_signal("GPS.numSatellites"); cache_signal("GPS.fixType")
    cache_signal("GPS.hdop"); cache_signal("GPS.vdop")
    for i=0,11 do
        cache_signal("GPS.satellites["..i.."].prn")
        cache_signal("GPS.satellites["..i.."].snr")
        cache_signal("GPS.satellites["..i.."].elevation")
        cache_signal("GPS.satellites["..i.."].azimuth")
    end

    -- Battery
    cache_signal("Battery.time")
    cache_signal("Battery.voltage"); cache_signal("Battery.current"); cache_signal("Battery.temperature")
    cache_signal("Battery.percentage"); cache_signal("Battery.health"); cache_signal("Battery.cycleCount")
    for i=0,15 do
        cache_signal("Battery.cells["..i.."].voltage")
        cache_signal("Battery.cells["..i.."].temperature")
        cache_signal("Battery.cells["..i.."].health")
        cache_signal("Battery.cells["..i.."].balancing")
        cache_signal("Battery.cells["..i.."].resistance")
    end
    cache_signal("Battery.maxCellVoltage"); cache_signal("Battery.minCellVoltage"); cache_signal("Battery.avgCellVoltage")
    cache_signal("Battery.powerOut"); cache_signal("Battery.energyConsumed")
    cache_signal("Battery.chargeState"); cache_signal("Battery.faultFlags")

    -- LIDAR
    cache_signal("LIDAR.time")
    cache_signal("LIDAR.range"); cache_signal("LIDAR.intensity")
    cache_signal("LIDAR.angleX"); cache_signal("LIDAR.angleY")
    cache_signal("LIDAR.quality"); cache_signal("LIDAR.flags")
    cache_signal("LIDAR.numTracks")
    for i=0,31 do
        cache_signal("LIDAR.tracks["..i.."].range")
        cache_signal("LIDAR.tracks["..i.."].velocity")
        -- Add others if needed
    end

    -- RADAR
    cache_signal("RADAR.time")
    cache_signal("RADAR.range"); cache_signal("RADAR.velocity")
    cache_signal("RADAR.azimuth"); cache_signal("RADAR.elevation")
    cache_signal("RADAR.targetCount")
    for i=0,23 do
        cache_signal("RADAR.tracks["..i.."].range")
        cache_signal("RADAR.tracks["..i.."].velocity")
         -- Add others if needed
    end

    -- State
    cache_signal("State.time")
    cache_signal("State.systemMode"); cache_signal("State.armed"); cache_signal("State.statusFlags")
    cache_signal("State.cpuUsage"); cache_signal("State.memoryUsage")
    cache_signal("State.boardTemperature")

    -- Debug
    cache_signal("Debug.time")
    cache_signal("Debug.counter"); cache_signal("Debug.value1"); cache_signal("Debug.value2")
    cache_signal("Debug.metric"); cache_signal("Debug.stackDepth")

    -- Motor
    cache_signal("Motor.time")
    cache_signal("Motor.rpm"); cache_signal("Motor.torque"); cache_signal("Motor.power")
    cache_signal("Motor.temperature"); cache_signal("Motor.throttle")
    cache_signal("Motor.voltage"); cache_signal("Motor.current")
    cache_signal("Motor.targetRPM"); cache_signal("Motor.rpmError")
end

init_signal_cache()

-- Register the FAST FFI parser
register_parser("fast_binary", function(raw_ptr, len)
    -- Check for valid address and length
    if not raw_ptr or len < 4 then return false end

    -- Cast lightuserdata to void pointer for field access
    local ptr = ffi.cast("void*", raw_ptr)
    
    if ptr == nil then return false end

    -- Read header (first 3 bytes)
    local header = ffi.string(ptr, 3)

    -- === IMU Handling ===
    if header == "IMU" and len >= ffi.sizeof("struct IMUData") then
        local packet = ffi.cast("struct IMUData*", ptr)
        local t = packet.time
        local accelX = packet.accelX

        update_signal_fast(sigs["IMU.time"], t, t)
        update_signal_fast(sigs["IMU.accelX"], t, accelX)
        update_signal_fast(sigs["IMU.accelY"], t, packet.accelY)
        update_signal_fast(sigs["IMU.accelZ"], t, packet.accelZ)
        update_signal_fast(sigs["IMU.gyroX"], t, packet.gyroX)
        update_signal_fast(sigs["IMU.gyroY"], t, packet.gyroY)
        update_signal_fast(sigs["IMU.gyroZ"], t, packet.gyroZ)
        update_signal_fast(sigs["IMU.magX"], t, packet.magX)
        update_signal_fast(sigs["IMU.magY"], t, packet.magY)
        update_signal_fast(sigs["IMU.magZ"], t, packet.magZ)
        update_signal_fast(sigs["IMU.temperature"], t, packet.temperature)
        
        trigger_packet_callbacks("IMU", t)
        return true
    end

    -- === GPS Handling ===
    if header == "GPS" and len >= ffi.sizeof("struct GPSData") then
        local packet = ffi.cast("struct GPSData*", ptr)
        local t = packet.time

        update_signal_fast(sigs["GPS.time"], t, t)
        update_signal_fast(sigs["GPS.latitude"], t, packet.latitude)
        update_signal_fast(sigs["GPS.longitude"], t, packet.longitude)
        update_signal_fast(sigs["GPS.altitude"], t, packet.altitude)
        update_signal_fast(sigs["GPS.speed"], t, packet.speed)
        update_signal_fast(sigs["GPS.heading"], t, packet.heading)
        update_signal_fast(sigs["GPS.numSatellites"], t, packet.numSatellites)
        
        trigger_packet_callbacks("GPS", t)
        return true
    end

    -- === Battery Handling ===
    if header == "BAT" and len >= ffi.sizeof("struct BatteryData") then
        local packet = ffi.cast("struct BatteryData*", ptr)
        local t = packet.time

        update_signal_fast(sigs["Battery.time"], t, t)
        update_signal_fast(sigs["Battery.voltage"], t, packet.voltage)
        update_signal_fast(sigs["Battery.current"], t, packet.current)
        update_signal_fast(sigs["Battery.percentage"], t, packet.percentage)
        update_signal_fast(sigs["Battery.powerOut"], t, packet.powerOut)
        
        trigger_packet_callbacks("Battery", t)
        return true
    end

    -- === LIDAR Handling ===
    if header == "LID" and len >= ffi.sizeof("struct LIDARData") then
        local packet = ffi.cast("struct LIDARData*", ptr)
        local t = packet.time

        update_signal_fast(sigs["LIDAR.time"], t, t)
        update_signal_fast(sigs["LIDAR.range"], t, packet.range)
        update_signal_fast(sigs["LIDAR.intensity"], t, packet.intensity)
        update_signal_fast(sigs["LIDAR.numTracks"], t, packet.numTracks)
        
        for i=0,packet.numTracks-1 do
            if i < 32 then
                update_signal_fast(sigs["LIDAR.tracks["..i.."].range"], t, packet.tracks[i].range)
                update_signal_fast(sigs["LIDAR.tracks["..i.."].velocity"], t, packet.tracks[i].velocity)
            end
        end

        trigger_packet_callbacks("LIDAR", t)
        return true
    end

    -- === RADAR Handling ===
    if header == "RAD" and len >= ffi.sizeof("struct RADARData") then
        local packet = ffi.cast("struct RADARData*", ptr)
        local t = packet.time

        update_signal_fast(sigs["RADAR.time"], t, t)
        update_signal_fast(sigs["RADAR.range"], t, packet.range)
        update_signal_fast(sigs["RADAR.velocity"], t, packet.velocity)
        update_signal_fast(sigs["RADAR.targetCount"], t, packet.targetCount)
        
        for i=0,packet.targetCount-1 do
            if i < 24 then
                update_signal_fast(sigs["RADAR.tracks["..i.."].range"], t, packet.tracks[i].range)
                update_signal_fast(sigs["RADAR.tracks["..i.."].velocity"], t, packet.tracks[i].velocity)
            end
        end

        trigger_packet_callbacks("RADAR", t)
        return true
    end

    -- === Motor Handling ===
    if header == "MTR" and len >= ffi.sizeof("struct MotorData") then
        local packet = ffi.cast("struct MotorData*", ptr)
        local t = packet.time

        update_signal_fast(sigs["Motor.time"], t, t)
        update_signal_fast(sigs["Motor.rpm"], t, packet.rpm)
        update_signal_fast(sigs["Motor.torque"], t, packet.torque)
        update_signal_fast(sigs["Motor.power"], t, packet.power)
        update_signal_fast(sigs["Motor.throttle"], t, packet.throttle)
        update_signal_fast(sigs["Motor.voltage"], t, packet.voltage)
        update_signal_fast(sigs["Motor.current"], t, packet.current)
        
        trigger_packet_callbacks("Motor", t)
        return true
    end

    -- === State Handling ===
    if header == "STA" and len >= ffi.sizeof("struct StateData") then
        local packet = ffi.cast("struct StateData*", ptr)
        local t = packet.time
        
        update_signal_fast(sigs["State.time"], t, t)
        update_signal_fast(sigs["State.systemMode"], t, packet.systemMode)
        update_signal_fast(sigs["State.armed"], t, packet.armed)
        update_signal_fast(sigs["State.cpuUsage"], t, packet.cpuUsage)
        update_signal_fast(sigs["State.boardTemperature"], t, packet.boardTemperature)

        trigger_packet_callbacks("State", t)
        return true
    end

    -- === Debug Handling ===
    if header == "DBG" and len >= ffi.sizeof("struct DebugData") then
        local packet = ffi.cast("struct DebugData*", ptr)
        local t = packet.time

        update_signal_fast(sigs["Debug.time"], t, t)
        update_signal_fast(sigs["Debug.counter"], t, tonumber(packet.counter))
        update_signal_fast(sigs["Debug.value1"], t, packet.value1)
        update_signal_fast(sigs["Debug.value2"], t, packet.value2)
        update_signal_fast(sigs["Debug.metric"], t, packet.metric)
        update_signal_fast(sigs["Debug.stackDepth"], t, packet.stackDepth)

        trigger_packet_callbacks("Debug", t)
        return true
    end

    return false
end)
