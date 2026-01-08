-- Serial Port Example Script
-- This demonstrates reading from a serial port asynchronously

local port = create_serial_port()

spawn(function()
    log("Opening serial port COM3 at 115200 baud...")
    
    local success = port:open("COM3", 115200)
    if not success then
        log("Failed to open serial port!")
        return
    end
    
    log("Serial port opened successfully!")
    
    -- Optionally configure the port (9600, 8N1 is default)
    -- port:configure(9600, "none", 1, 8)
    
    -- Read data in a loop
    while is_app_running() do
        local data, err = port:receive_async(1024)
        
        if data and #data > 0 then
            log("Received " .. #data .. " bytes from serial port")
            
            -- Example: If expecting binary telemetry packets
            -- You could parse the data here and update signals
            -- parse_packet(data, #data)
            
            -- Example: If expecting text data
            -- log("Data: " .. data)
        elseif err then
            log("Serial port error: " .. err)
            break
        end
        
        sleep(0.01)  -- Yield briefly (10ms) before next read
    end
    
    port:close()
    log("Serial port closed")
end)

log("Serial port reader script loaded!")
