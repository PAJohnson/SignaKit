-- Example: JSON telemetry parser
-- This demonstrates parsing JSON-formatted packets
--
-- Example JSON packet format:
-- {"type":"sensor","timestamp":12345.67,"temperature":23.5,"humidity":45.2}
--
-- NOTE: This is a simplified example. For production use, you would need
-- a proper JSON parsing library like lua-cjson or dkjson

log("Loading JSON parser example")

-- Simple JSON parser helper (very basic, for demonstration only)
local function parseSimpleJSON(jsonStr)
    local data = {}

    -- Extract type field
    local typeMatch = jsonStr:match('"type"%s*:%s*"([^"]+)"')
    if typeMatch then data.type = typeMatch end

    -- Extract numeric fields
    for key, value in jsonStr:gmatch('"([^"]+)"%s*:%s*([%d%.%-]+)') do
        data[key] = tonumber(value)
    end

    return data
end

-- Register JSON parser (disabled by default - uncomment to enable)
--[[
register_parser("json_telemetry", function(buffer, length)
    -- Check if buffer starts with '{'
    local firstByte = getBufferByte(buffer, 0)
    if not firstByte or firstByte ~= string.byte('{') then
        return false
    end

    -- Convert buffer to string
    local jsonStr = readString(buffer, 0, length)
    if not jsonStr then
        return false
    end

    -- Parse JSON
    local data = parseSimpleJSON(jsonStr)

    -- Handle sensor data
    if data.type == "sensor" and data.timestamp then
        local time = data.timestamp

        if data.temperature then
            update_signal("JSON.temperature", time, data.temperature)
        end

        if data.humidity then
            update_signal("JSON.humidity", time, data.humidity)
        end

        if data.pressure then
            update_signal("JSON.pressure", time, data.pressure)
        end

        return true
    end

    -- Handle GPS data
    if data.type == "gps" and data.timestamp then
        local time = data.timestamp

        if data.lat then update_signal("JSON.GPS.latitude", time, data.lat) end
        if data.lon then update_signal("JSON.GPS.longitude", time, data.lon) end
        if data.alt then update_signal("JSON.GPS.altitude", time, data.alt) end
        if data.speed then update_signal("JSON.GPS.speed", time, data.speed) end

        return true
    end

    return false
end)

log("JSON parser registered (DISABLED by default - uncomment to enable)")
--]]

log("JSON parser example loaded (commented out - edit file to enable)")
