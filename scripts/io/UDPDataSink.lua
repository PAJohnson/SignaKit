-- Tier 5: UDPDataSink.lua
-- Lua implementation of UDP packet reception and parsing
-- Replaces the C++ NetworkReceiverThread and UDPDataSink classes
-- This script runs as a frame callback at 60 FPS (non-blocking I/O)

print("========================================")
print("UDPDataSink.lua - Lua-based UDP I/O (sockpp)")
print("========================================")

-- State variables (persist across frame callbacks)
local udpSocket = nil
local logFile = nil
local connected = false
local packetsReceived = 0
local startTime = get_time_seconds()
local lastStatsTime = startTime

-- Configuration - read from GUI text inputs
local function getConfig()
    local ip = get_text_input("UDP IP") or "127.0.0.1"
    local portStr = get_text_input("UDP Port") or "12345"
    local port = tonumber(portStr) or 12345
    return ip, port
end

-- Connect to UDP socket
local function connect()
    if connected then
        return true
    end

    local ip, port = getConfig()
    print(string.format("[UDPDataSink] Attempting to bind to %s:%d", ip, port))

    -- Create UDP socket using sockpp wrapper
    udpSocket = create_udp_socket()
    if not udpSocket then
        print("[UDPDataSink] Failed to create UDP socket")
        return false
    end

    -- Bind to IP:PORT
    if not udpSocket:bind(ip, port) then
        print("[UDPDataSink] Failed to bind socket")
        udpSocket = nil
        return false
    end

    -- Set non-blocking mode
    if not udpSocket:set_non_blocking(true) then
        print("[UDPDataSink] Failed to set non-blocking mode")
        udpSocket:close()
        udpSocket = nil
        return false
    end

    connected = true

    -- Optional: Open binary log file
    logFile = io.open("packet_log.bin", "wb")

    print(string.format("[UDPDataSink] Connected to UDP %s:%d", ip, port))
    return true
end

-- Disconnect from UDP socket
local function disconnect()
    if udpSocket then
        udpSocket:close()
        udpSocket = nil
    end

    if logFile then
        logFile:close()
        logFile = nil
    end

    connected = false
    print("[UDPDataSink] Disconnected")
end

-- Frame callback function (runs every frame at ~60 FPS)
local function udp_frame_callback()
    -- Check if user toggled Connect
    if get_toggle_state("UDP Connect") then
        if not connected then
            connect()
        end
    else
        if connected then
            disconnect()
        end
    end

    -- If connected, receive and parse packets (non-blocking)
    if connected and udpSocket then
        -- Process multiple packets per frame to avoid buffer overflow
        local maxPacketsPerFrame = 100
        local packetsThisFrame = 0

        while packetsThisFrame < maxPacketsPerFrame do
            -- Receive returns tuple: (data, error_string)
            local data, err = udpSocket:receive(65536)

            if data and #data > 0 then
                packetsReceived = packetsReceived + 1
                packetsThisFrame = packetsThisFrame + 1

                -- Optional: Log raw packet to file
                if logFile then
                    logFile:write(data)
                    logFile:flush()
                end

                -- Parse packet using registered parser
                -- The legacy_binary parser is already registered by Tier 2
                local parseSuccess = parse_packet(data, #data)
                if not parseSuccess then
                    -- Silently ignore unparseable packets (this is normal)
                end
            elseif err == "timeout" then
                -- No more data available this frame
                break
            elseif err and #err > 0 then
                -- Socket error
                print("[UDPDataSink] Socket error: " .. tostring(err))
                disconnect()
                break
            else
                -- Empty data, no error - just break
                break
            end
        end

        -- Print statistics every 5 seconds
        local currentTime = get_time_seconds()
        if currentTime - lastStatsTime >= 5.0 and packetsReceived > 0 then
            local elapsed = currentTime - startTime
            local packetsPerSec = packetsReceived / elapsed
            print(string.format("[UDPDataSink] Stats: %d packets received, %.1f packets/sec",
                               packetsReceived, packetsPerSec))
            lastStatsTime = currentTime
        end
    end
end

-- Register cleanup handler
on_cleanup(function()
    print("[UDPDataSink] Cleanup called, disconnecting...")
    disconnect()
end)

-- Register the frame callback (runs every frame at ~60 FPS)
on_frame(udp_frame_callback)
print("[UDPDataSink] Registered UDP I/O frame callback")
