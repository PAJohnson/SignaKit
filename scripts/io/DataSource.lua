-- DataSource.lua
-- Unified data source handler for both Online (UDP) and Offline (file playback) modes
-- Replaces both C++ NetworkReceiverThread and offline file loading
-- This script runs as a frame callback at ~60 FPS

print("========================================")
print("DataSource.lua - Unified Online/Offline Data Handler")
print("========================================")

-- ==================== STATE VARIABLES ====================
-- Online mode state
local udpSocket = nil
local udpConnected = false
local packetsReceived = 0
local startTime = get_time_seconds()
local lastStatsTime = startTime
local logFile = nil

-- Offline mode state
local offlineLoadedFilePath = ""
local offlineLoading = false
local fileDialogOpen = false

-- ==================== HELPER FUNCTIONS ====================

-- Get configuration from GUI controls
local function getOnlineConfig()
    local ip = get_text_input("UDP IP") or "127.0.0.1"
    local portStr = get_text_input("UDP Port") or "12345"
    local port = tonumber(portStr) or 12345
    return ip, port
end

-- Check if we're in online mode
local function isOnlineMode()
    return get_toggle_state("Online") == true
end

-- ==================== ONLINE MODE FUNCTIONS ====================

local function connectOnline()
    if udpConnected then
        return true
    end

    local ip, port = getOnlineConfig()
    print(string.format("[DataSource] Online: Attempting to bind to %s:%d", ip, port))

    udpSocket = create_udp_socket()
    if not udpSocket then
        print("[DataSource] Online: Failed to create UDP socket")
        return false
    end

    if not udpSocket:bind(ip, port) then
        print("[DataSource] Online: Failed to bind socket")
        udpSocket = nil
        return false
    end

    if not udpSocket:set_non_blocking(true) then
        print("[DataSource] Online: Failed to set non-blocking mode")
        udpSocket:close()
        udpSocket = nil
        return false
    end

    udpConnected = true
    logFile = io.open("packet_log.bin", "wb")
    print(string.format("[DataSource] Online: Connected to UDP %s:%d", ip, port))
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
    print("[DataSource] Online: Disconnected")
end

local function processOnlineMode()
    -- Check if user toggled UDP Connect
    if get_toggle_state("UDP Connect") then
        if not udpConnected then
            connectOnline()
        end
    else
        if udpConnected then
            disconnectOnline()
        end
    end

    -- If connected, receive and parse packets (non-blocking)
    -- Process ALL available packets each frame (no artificial limit)
    if udpConnected and udpSocket then
        local packetsThisFrame = 0

        while true do
            local data, err = udpSocket:receive(65536)

            if data and #data > 0 then
                packetsReceived = packetsReceived + 1
                packetsThisFrame = packetsThisFrame + 1

                -- Log raw packet to file
                if logFile then
                    logFile:write(data)
                    logFile:flush()
                end

                -- Parse packet using registered parser
                local parseSuccess = parse_packet(data, #data)
                if not parseSuccess then
                    -- Silently ignore unparseable packets
                end
            elseif err == "timeout" then
                -- No more data available, exit loop
                break
            elseif err and #err > 0 then
                print("[DataSource] Online: Socket error: " .. tostring(err))
                disconnectOnline()
                break
            else
                break
            end
        end

        -- Print statistics every 5 seconds
        local currentTime = get_time_seconds()
        if currentTime - lastStatsTime >= 5.0 and packetsReceived > 0 then
            local elapsed = currentTime - startTime
            local packetsPerSec = packetsReceived / elapsed
            print(string.format("[DataSource] Online: %d packets received, %.1f packets/sec",
                               packetsReceived, packetsPerSec))
            lastStatsTime = currentTime
        end
    end
end

-- ==================== OFFLINE MODE FUNCTIONS ====================

-- Packet size lookup table (updated with expanded packet structures)
local PACKET_SIZES = {
    ["IMU"] = 164,
    ["GPS"] = 174,
    ["BAT"] = 268,
    ["LID"] = 798,
    ["RAD"] = 908,
    ["STA"] = 420,
    ["DBG"] = 874,
    ["MTR"] = 126
}

-- Try to detect packet type from buffer and return size
local function getPacketSize(buffer)
    if #buffer < 3 then
        return nil
    end
    local header = buffer:sub(1, 3)
    return PACKET_SIZES[header]
end

local function loadOfflineFile(filepath)
    print(string.format("[DataSource] Offline: Loading file: %s", filepath))
    offlineLoading = true

    -- Set default mode to offline so all new signals grow indefinitely
    set_default_signal_mode("offline")

    -- Clear all existing signals
    clear_all_signals()

    -- Open file in binary mode
    local file = io.open(filepath, "rb")
    if not file then
        print("[DataSource] Offline: Failed to open file")
        offlineLoading = false
        return false
    end

    -- Get file size
    file:seek("end")
    local fileSize = file:seek()
    file:seek("set", 0)

    print(string.format("[DataSource] Offline: File size: %d bytes", fileSize))

    -- Parse entire file to populate signals (static viewer - no playback)
    local currentPos = 0
    local packetsProcessed = 0
    local chunkSize = 1024  -- Read 1KB at a time (same as C++ version)

    while currentPos < fileSize do
        -- Seek to current position
        file:seek("set", currentPos)

        -- Read chunk
        local remainingBytes = fileSize - currentPos
        local bytesToRead = math.min(remainingBytes, chunkSize)
        local chunk = file:read(bytesToRead)

        if not chunk or #chunk == 0 then
            print(string.format("[DataSource] Offline: Failed to read at position %d", currentPos))
            break
        end

        -- Try to detect packet type and get size
        local packetSize = getPacketSize(chunk)

        if packetSize and #chunk >= packetSize then
            -- We have enough data for a complete packet, try to parse it
            local parseSuccess = parse_packet(chunk, #chunk)

            if parseSuccess then
                packetsProcessed = packetsProcessed + 1
                -- Advance by the actual packet size
                currentPos = currentPos + packetSize
            else
                -- Parse failed even though header matched, skip one byte
                currentPos = currentPos + 1
            end
        else
            -- Unknown packet header or incomplete packet, skip one byte and try again
            currentPos = currentPos + 1
        end

        -- Progress indicator every 1000 packets
        if packetsProcessed % 1000 == 0 and packetsProcessed > 0 then
            local progress = (currentPos / fileSize) * 100
            print(string.format("[DataSource] Offline: Progress: %d packets (%.1f%%)",
                               packetsProcessed, progress))
        end
    end

    file:close()

    print(string.format("[DataSource] Offline: Loaded %d packets from %d bytes",
                       packetsProcessed, fileSize))

    offlineLoadedFilePath = filepath
    offlineLoading = false
    return true
end

local function processOfflineMode()
    -- Check if file dialog is open
    if fileDialogOpen then
        if is_file_dialog_open("OfflineLogFileDlg") then
            -- Dialog is being displayed, check for result
            local result = get_file_dialog_result("OfflineLogFileDlg")
            if result then
                -- File was selected
                fileDialogOpen = false
                loadOfflineFile(result)
            end
        end
    end

    -- Check if user clicked a "Load File" button (if it exists in the layout)
    if get_button_clicked("Load File") and not offlineLoading then
        print("[DataSource] Offline: Opening file dialog...")
        open_file_dialog("OfflineLogFileDlg", "Open Log File", ".bin")
        fileDialogOpen = true
    end
end

-- ==================== MAIN FRAME CALLBACK ====================

local function data_source_frame_callback()
    if isOnlineMode() then
        -- If switching to online mode, clear offline data and reset mode
        if offlineLoadedFilePath ~= "" then
            print("[DataSource] Switching to online mode, clearing offline data")
            set_default_signal_mode("online")
            clear_all_signals()
            offlineLoadedFilePath = ""
        end
        processOnlineMode()
    else
        -- Disconnect online if switching modes
        if udpConnected then
            disconnectOnline()
        end
        processOfflineMode()
    end
end

-- ==================== CLEANUP ====================

on_cleanup(function()
    print("[DataSource] Cleanup called")
    disconnectOnline()
end)

-- ==================== REGISTER CALLBACK ====================

on_frame(data_source_frame_callback)
print("[DataSource] Registered unified data source frame callback")
