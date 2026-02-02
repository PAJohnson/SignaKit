-- TelemetryProject.lua
-- Unified Data Source + Parser for Binary Telemetry
-- Combines online/offline data source handling with FFI-based packet parsing
-- Features automatic signal cache generation - just define structs and packet config!

print("========================================")
print("TelemetryProject.lua - Unified Telemetry System")
print("========================================")

local ffi = require("ffi")

-- ==================== FFI STRUCT DEFINITIONS ====================
-- Just paste your C struct definitions here - signal cache is built automatically!

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

-- ==================== PACKET CONFIGURATION ====================
-- Define your packets here with their struct names and signal prefixes
-- The FIELDS table specifies which fields to extract (or use "all" for simple structs)

local PACKETS = {
    {
        header = "IMU",
        struct_type = "struct IMUData",
        prefix = "IMU",
        fields = {"time", "accelX", "accelY", "accelZ", "gyroX", "gyroY", "gyroZ",
                  "magX", "magY", "magZ", "temperature"}
    },
    {
        header = "GPS",
        struct_type = "struct GPSData",
        prefix = "GPS",
        fields = {"time", "latitude", "longitude", "altitude", "speed", "heading",
                  "verticalSpeed", "numSatellites", "fixType", "hdop", "vdop"}
    },
    {
        header = "BAT",
        struct_type = "struct BatteryData",
        prefix = "Battery",
        fields = {"time", "voltage", "current", "temperature", "percentage", "health",
                  "cycleCount", "powerOut", "energyConsumed", "chargeState", "faultFlags"}
    },
    {
        header = "LID",
        struct_type = "struct LIDARData",
        prefix = "LIDAR",
        fields = {"time", "range", "intensity", "angleX", "angleY", "quality", "flags", "numTracks"},
        nested = {
            {field = "tracks", count = 32, count_field = "numTracks", subfields = {"range", "velocity"}}
        }
    },
    {
        header = "RAD",
        struct_type = "struct RADARData",
        prefix = "RADAR",
        fields = {"time", "range", "velocity", "azimuth", "elevation", "signalStrength",
                  "targetCount", "ambientNoise", "temperature"},
        nested = {
            {field = "tracks", count = 24, count_field = "targetCount", subfields = {"range", "velocity"}}
        }
    },
    {
        header = "MTR",
        struct_type = "struct MotorData",
        prefix = "Motor",
        fields = {"time", "rpm", "torque", "power", "temperature", "throttle", "voltage",
                  "current", "targetRPM", "rpmError"}
    },
    {
        header = "STA",
        struct_type = "struct StateData",
        prefix = "State",
        fields = {"time", "systemMode", "armed", "statusFlags", "cpuUsage", "memoryUsage",
                  "boardTemperature"}
    },
    {
        header = "DBG",
        struct_type = "struct DebugData",
        prefix = "Debug",
        fields = {"time", "counter", "value1", "value2", "metric", "stackDepth"}
    }
}

-- ==================== AUTOMATIC SIGNAL CACHE GENERATION ====================

local function build_signal_cache()
    log("Building automatic signal cache...")
    local sigs = {}
    local count = 0

    for _, packet in ipairs(PACKETS) do
        log(string.format("  Caching signals for %s (%s)", packet.prefix, packet.header))

        -- Cache basic fields
        for _, field in ipairs(packet.fields) do
            local signal_name = packet.prefix .. "." .. field
            local sig_id = get_signal_id(signal_name)
            sigs[signal_name] = sig_id
            count = count + 1
        end

        -- Cache nested array fields
        if packet.nested then
            for _, nested in ipairs(packet.nested) do
                for i = 0, nested.count - 1 do
                    for _, subfield in ipairs(nested.subfields) do
                        local signal_name = packet.prefix .. "." .. nested.field .. "[" .. i .. "]." .. subfield
                        local sig_id = get_signal_id(signal_name)
                        sigs[signal_name] = sig_id
                        count = count + 1
                    end
                end
            end
        end
    end

    log(string.format("Signal cache built: %d signals cached", count))
    return sigs
end

local sigs = build_signal_cache()

-- Debug: Print first few signal IDs to verify cache is working
local debug_signals = {"IMU.time", "IMU.accelX", "GPS.latitude", "Battery.voltage"}
log("Debug: Checking sample signal IDs:")
for _, sig_name in ipairs(debug_signals) do
    if sigs[sig_name] then
        log(string.format("  %s = %s", sig_name, tostring(sigs[sig_name])))
    else
        log(string.format("  %s = NIL (ERROR!)", sig_name))
    end
end

-- ==================== DATA SOURCE (ONLINE/OFFLINE) ====================

local udpSocket = nil
local udpConnected = false
local packetsReceived = 0
local startTime = get_time_seconds()
local lastStatsTime = startTime
local logFile = nil
local offlineLoadedFilePath = ""
local offlineLoading = false
local fileDialogOpen = false

-- Shared buffer for zero-copy packet reception
local RECV_BUFFER_SIZE = 65536
local sharedBuffer = SharedBuffer.new(RECV_BUFFER_SIZE)
local rawPtr = sharedBuffer:get_ptr()
local recvBuffer = ffi.cast("uint8_t*", rawPtr)

local function getOnlineConfig()
    local ip = get_text_input("UDP IP") or "127.0.0.1"
    local portStr = get_text_input("UDP Port") or "12345"
    local port = tonumber(portStr) or 12345
    return ip, port
end

local function isOnlineMode()
    return get_toggle_state("Online") == true
end

local function connectOnline()
    if udpConnected then
        return true
    end

    local ip, port = getOnlineConfig()
    print(string.format("[TelemetryProject] Connecting to UDP %s:%d", ip, port))

    udpSocket = create_udp_socket()
    if not udpSocket then
        print("[TelemetryProject] Failed to create UDP socket")
        return false
    end

    if not udpSocket:bind(ip, port) then
        print("[TelemetryProject] Failed to bind socket")
        udpSocket = nil
        return false
    end

    if not udpSocket:set_non_blocking(true) then
        print("[TelemetryProject] Failed to set non-blocking mode")
        udpSocket:close()
        udpSocket = nil
        return false
    end

    udpConnected = true
    logFile = io.open("packet_log.bin", "wb")
    print(string.format("[TelemetryProject] Connected to UDP %s:%d", ip, port))
    return true
end

local function disconnectOnline()
    if udpSocket then
        udpSocket:close()
        udpSocket = nil
    end
    if logFile then
        logFile:close()
        logFile = nil
    end
    udpConnected = false
    print("[TelemetryProject] Disconnected from UDP")
end

-- Build packet size lookup table
local PACKET_SIZES = {}
for _, packet in ipairs(PACKETS) do
    PACKET_SIZES[packet.header] = ffi.sizeof(packet.struct_type)
end

local function getPacketSize(buffer)
    if #buffer < 3 then
        return nil
    end
    local header = buffer:sub(1, 3)
    return PACKET_SIZES[header]
end

local function loadOfflineFile(filepath)
    print(string.format("[TelemetryProject] Loading offline file: %s", filepath))
    offlineLoading = true
    set_default_signal_mode("offline")
    clear_all_signals()

    local file = io.open(filepath, "rb")
    if not file then
        print("[TelemetryProject] Failed to open file")
        offlineLoading = false
        return false
    end

    file:seek("end")
    local fileSize = file:seek()
    file:seek("set", 0)

    local currentPos = 0
    local packetsProcessed = 0
    local chunkSize = 1024
    local lastYieldTime = get_time_seconds()

    while currentPos < fileSize do
        file:seek("set", currentPos)
        local remainingBytes = fileSize - currentPos
        local bytesToRead = math.min(remainingBytes, chunkSize)
        local chunk = file:read(bytesToRead)

        if not chunk or #chunk == 0 then break end

        local packetSize = getPacketSize(chunk)
        if packetSize and #chunk >= packetSize then
            if parse_telemetry_string(chunk, packetSize) then
                packetsProcessed = packetsProcessed + 1
                currentPos = currentPos + packetSize
            else
                currentPos = currentPos + 1
            end
        else
            currentPos = currentPos + 1
        end

        local now = get_time_seconds()
        if now - lastYieldTime > 0.01 then
            yield()
            lastYieldTime = now
        end

        if packetsProcessed % 5000 == 0 and packetsProcessed > 0 then
            print(string.format("[TelemetryProject] Load progress: %.1f%% (%d packets)",
                               (currentPos / fileSize) * 100, packetsProcessed))
        end
    end

    file:close()
    print(string.format("[TelemetryProject] Loaded %d packets", packetsProcessed))
    offlineLoadedFilePath = filepath
    offlineLoading = false
    return true
end

-- ==================== PACKET PARSER ====================
-- Direct parsing functions - no registration needed!

local parser_initialized = false
local packet_count = 0

-- Parse from a raw pointer (for zero-copy UDP reception)
local function parse_telemetry_packet(raw_ptr, len)
    if not raw_ptr or len < 4 then return false end

    local ptr = ffi.cast("void*", raw_ptr)
    if ptr == nil then return false end

    local header = ffi.string(ptr, 3)

    if not parser_initialized then
        log(string.format("[Parser] First packet received: header='%s', len=%d", header, len))
        parser_initialized = true
    end

    -- Find matching packet configuration
    for _, packet in ipairs(PACKETS) do
        if header == packet.header and len >= ffi.sizeof(packet.struct_type) then
            packet_count = packet_count + 1

            if packet_count <= 3 then
                log(string.format("[Parser] Matched packet: %s (len=%d, struct_size=%d)",
                    header, len, ffi.sizeof(packet.struct_type)))
            end

            local data = ffi.cast(packet.struct_type .. "*", ptr)
            local t = data.time

            -- Update basic fields
            for _, field in ipairs(packet.fields) do
                local signal_name = packet.prefix .. "." .. field
                local sig_id = sigs[signal_name]

                if not sig_id then
                    log(string.format("[Parser] ERROR: Signal ID not found for '%s'", signal_name))
                    return false
                end

                local value = data[field]
                -- Handle int64 conversion for Lua
                if type(value) == "cdata" and tostring(value):match("^%-?%d+LL$") then
                    value = tonumber(value)
                end
                update_signal_fast(sig_id, t, value)
            end

            -- Update nested array fields
            if packet.nested then
                for _, nested in ipairs(packet.nested) do
                    local nested_array = data[nested.field]
                    local count_field = nested.count_field or ("num" .. nested.field:sub(1,1):upper() .. nested.field:sub(2))
                    local actual_count = nested.count

                    -- Try to get dynamic count if available
                    if data[count_field] ~= nil then
                        actual_count = math.min(tonumber(data[count_field]), nested.count)
                    end

                    for i = 0, actual_count - 1 do
                        for _, subfield in ipairs(nested.subfields) do
                            local signal_name = packet.prefix .. "." .. nested.field .. "[" .. i .. "]." .. subfield
                            local sig_id = sigs[signal_name]
                            if sig_id then
                                update_signal_fast(sig_id, t, nested_array[i][subfield])
                            end
                        end
                    end
                end
            end

            trigger_packet_callbacks(packet.prefix, t)
            return true
        end
    end

    if packet_count <= 5 then
        log(string.format("[Parser] No match for header='%s', len=%d", header, len))
    end
    return false
end

-- Parse from a string buffer (for offline file loading)
local function parse_telemetry_string(buffer, len)
    -- Cast string to pointer and call main parser
    local temp_buf = ffi.new("uint8_t[?]", len)
    ffi.copy(temp_buf, buffer, len)
    return parse_telemetry_packet(temp_buf, len)
end

-- ==================== MAIN THREAD LOOP ====================
-- This loop runs in a dedicated OS thread for high-performance data processing
-- The main thread spawns a worker thread that re-executes this file, giving it access to
-- all FFI definitions, helper functions, and the signal cache.

-- Main thread: spawn worker thread
if not IS_WORKER_THREAD then
    print("[TelemetryProject] Main thread: spawning worker thread")
    spawn_thread_file("scripts/parsers/TelemetryProject.lua")
    print("[TelemetryProject] Initialized successfully (worker thread spawned)")
    return -- Exit main thread execution here
end

-- Worker thread: spawn coroutine with main loop
print("[TelemetryProject] Worker thread started (THREAD_ID=" .. tostring(THREAD_ID) .. ")")

local _keepAlive = sharedBuffer

spawn_thread(function()
    print("[TelemetryProject] Worker coroutine started")

    while is_app_running() do
        local online = isOnlineMode()
        print(string.format("[TelemetryProject] Loop iteration - online=%s, running=%s", tostring(online), tostring(is_app_running())))

        if online then
            if offlineLoadedFilePath ~= "" then
                print("[TelemetryProject] Switching to online mode")
                set_default_signal_mode("online")
                clear_all_signals()
                offlineLoadedFilePath = ""
            end

            if get_toggle_state("UDP Connect") then
                if not udpConnected then
                    connectOnline()
                end
            else
                if udpConnected then
                    disconnectOnline()
                end
            end

            if udpConnected and udpSocket then
                -- Non-blocking receive (returns immediately)
                local len, err = udpSocket:receive_ptr(rawPtr, RECV_BUFFER_SIZE)

                if len > 0 then
                    packetsReceived = packetsReceived + 1

                    if logFile then
                        local dataStr = ffi.string(recvBuffer, len)
                        logFile:write(dataStr)
                        logFile:flush()
                    end

                    parse_telemetry_packet(rawPtr, len)

                    -- Drain remaining packets
                    while true do
                        local l2, e2 = udpSocket:receive_ptr(rawPtr, RECV_BUFFER_SIZE)
                        if l2 > 0 then
                            packetsReceived = packetsReceived + 1
                            parse_telemetry_packet(rawPtr, l2)
                        else
                            break
                        end
                    end
                elseif err and #err > 0 and err ~= "timeout" then
                    print("[TelemetryProject] Socket error: " .. tostring(err))
                    disconnectOnline()
                end
            end

            -- Small sleep to avoid spinning CPU when no data
            sleep_ms(1)
        else
            -- Offline mode
            if udpConnected then
                disconnectOnline()
            end

            if fileDialogOpen then
                if is_file_dialog_open("OfflineLogFileDlg") then
                    local result = get_file_dialog_result("OfflineLogFileDlg")
                    if result then
                        fileDialogOpen = false
                        loadOfflineFile(result)
                    end
                end
            end

            if get_button_clicked("Load File") and not offlineLoading then
                print("[TelemetryProject] Opening file dialog")
                open_file_dialog("OfflineLogFileDlg", "Open Log File", ".bin")
                fileDialogOpen = true
            end

            -- Sleep in offline mode
            sleep_ms(10)
        end

        -- Stats printing
        local currentTime = get_time_seconds()
        if currentTime - lastStatsTime >= 5.0 and packetsReceived > 0 then
            local elapsed = currentTime - startTime
            local packetsPerSec = packetsReceived / elapsed
            print(string.format("[TelemetryProject] %d packets, %.1f pkt/s",
                               packetsReceived, packetsPerSec))
            lastStatsTime = currentTime
        end
    end

    print("[TelemetryProject] Worker coroutine exiting")
end)
