-- Example: CSV telemetry parser
-- This demonstrates parsing CSV-formatted packets
--
-- Example CSV format:
-- timestamp,sensor_name,value
-- 12345.67,temperature,23.5
-- 12345.67,humidity,45.2
--
-- Or multi-column format:
-- timestamp,temp,humidity,pressure
-- 12345.67,23.5,45.2,1013.25

log("Loading CSV parser example")

-- Helper to split CSV line
local function splitCSV(line)
    local fields = {}
    for field in line:gmatch("([^,]+)") do
        table.insert(fields, field)
    end
    return fields
end

-- Register CSV parser (disabled by default - uncomment to enable)
--[[
register_parser("csv_telemetry", function(buffer, length)
    -- Convert buffer to string
    local line = readString(buffer, 0, length)
    if not line then
        return false
    end

    -- Remove whitespace
    line = line:gsub("^%s+", ""):gsub("%s+$", "")

    -- Check if it looks like CSV (contains commas)
    if not line:find(",") then
        return false
    end

    -- Split into fields
    local fields = splitCSV(line)

    -- Parse format 1: timestamp,sensor_name,value
    if #fields == 3 then
        local timestamp = tonumber(fields[1])
        local sensorName = fields[2]
        local value = tonumber(fields[3])

        if timestamp and sensorName and value then
            update_signal("CSV." .. sensorName, timestamp, value)
            return true
        end
    end

    -- Parse format 2: timestamp,temp,humidity,pressure
    if #fields == 4 then
        local timestamp = tonumber(fields[1])
        local temp = tonumber(fields[2])
        local humidity = tonumber(fields[3])
        local pressure = tonumber(fields[4])

        if timestamp and temp and humidity and pressure then
            update_signal("CSV.temperature", timestamp, temp)
            update_signal("CSV.humidity", timestamp, humidity)
            update_signal("CSV.pressure", timestamp, pressure)
            return true
        end
    end

    return false
end)

log("CSV parser registered (DISABLED by default - uncomment to enable)")
--]]

log("CSV parser example loaded (commented out - edit file to enable)")
