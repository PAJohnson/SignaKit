# Protocol Examples: MAVLink and SCPI

This document provides usage examples for MAVLink (drone telemetry) and SCPI (lab equipment) using existing UDP and TCP socket support.

---

## MAVLink - Drone/UAV Telemetry

### Overview

MAVLink is the de facto standard for drone communication (ArduPilot, PX4). It's a binary protocol typically sent over UDP port 14550.

### Supported with Existing Features

MAVLink works out-of-the-box with your existing UDP sockets. You just need to parse the binary packets in Lua.

### Example: Basic MAVLink Receiver

```lua
-- scripts/examples/mavlink_drone.lua

-- MAVLink message IDs
local MAVLINK_MSG_ID_HEARTBEAT = 0
local MAVLINK_MSG_ID_GPS_RAW_INT = 24
local MAVLINK_MSG_ID_ATTITUDE = 30
local MAVLINK_MSG_ID_VFR_HUD = 74

-- Simple MAVLink v1 parser
local function parse_mavlink_v1(data)
    if #data < 8 then return nil end
    
    local stx = string.byte(data, 1)
    if stx ~= 0xFE then return nil end  -- MAVLink v1 magic byte
    
    local len = string.byte(data, 2)
    local seq = string.byte(data, 3)
    local sysid = string.byte(data, 4)
    local compid = string.byte(data, 5)
    local msgid = string.byte(data, 6)
    
    if #data < 8 + len then return nil end
    
    local payload = string.sub(data, 7, 6 + len)
    
    return {
        msgid = msgid,
        sysid = sysid,
        compid = compid,
        seq = seq,
        payload = payload
    }
end

-- Parse GPS_RAW_INT message
local function parse_gps_raw(payload)
    if #payload < 30 then return nil end
    
    -- Unpack fields (little-endian)
    local function unpack_i32(data, offset)
        local b1, b2, b3, b4 = string.byte(data, offset, offset+3)
        return b1 + b2*256 + b3*65536 + b4*16777216
    end
    
    local lat = unpack_i32(payload, 5) / 1e7   -- degrees
    local lon = unpack_i32(payload, 9) / 1e7   -- degrees
    local alt = unpack_i32(payload, 13) / 1000 -- meters
    
    return {latitude = lat, longitude = lon, altitude = alt}
end

-- Main receiver loop
spawn(function()
    local socket = create_udp_socket()
    
    if not socket:bind("0.0.0.0", 14550) then
        log("Failed to bind MAVLink socket")
        return
    end
    
    log("Listening for MAVLink on UDP 14550...")
    
    while is_app_running() do
        local data, err = socket:receive_async(2048)
        
        if data and #data > 0 then
            local msg = parse_mavlink_v1(data)
            
            if msg then
                if msg.msgid == MAVLINK_MSG_ID_GPS_RAW_INT then
                    local gps = parse_gps_raw(msg.payload)
                    if gps then
                        update_signal("Drone.Latitude", gps.latitude)
                        update_signal("Drone.Longitude", gps.longitude)
                        update_signal("Drone.Altitude", gps.altitude)
                        log(string.format("GPS: %.6f, %.6f, %.1fm", 
                            gps.latitude, gps.longitude, gps.altitude))
                    end
                elseif msg.msgid == MAVLINK_MSG_ID_HEARTBEAT then
                    log("Heartbeat from System " .. msg.sysid)
                end
            end
        end
        
        sleep(0.01)  -- 100 Hz check rate
    end
    
    socket:close()
end)

log("MAVLink receiver loaded!")
```

### Testing MAVLink

**Without Real Drone:**
1. Use **MAVProxy** simulator:
   ```bash
   pip install MAVProxy
   mavproxy.py --master=tcp:127.0.0.1:5760 --out=udp:127.0.0.1:14550
   ```

2. Or send test packets from Python:
   ```python
   import socket
   import struct
   
   sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
   
   # GPS_RAW_INT message
   payload = struct.pack('<QiiiHHHBBB',
       1234567890000,  # time_usec
       int(37.7749 * 1e7),  # lat (San Francisco)
       int(-122.4194 * 1e7),  # lon
       int(50 * 1000),  # alt (50m)
       65535, 65535, 65535,  # eph, epv, vel
       65535,  # cog
       0, 3)  # fix_type, satellites
   
   # Build MAVLink packet
   msg = bytes([0xFE, len(payload), 0, 1, 1, 24]) + payload
   sock.sendto(msg, ('127.0.0.1', 14550))
   ```

**With Real Drone:**
- Connect drone via USB or telemetry radio
- Configure to send MAVLink to UDP port 14550
- Script automatically receives and parses data

---

## SCPI - Test & Measurement Equipment

### Overview

SCPI (Standard Commands for Programmable Instruments) is used by oscilloscopes, multimeters, power supplies, spectrum analyzers, etc. It's a text-based protocol over:
- **LAN (TCP)** - Port 5025 (most modern equipment)
- **USB** - Via virtual COM port
- **GPIB** - Via GPIB-USB adapter (legacy)

### Supported with Existing Features

SCPI works with your existing **TCP** and **Serial** sockets!

### Example 1: Keysight/Agilent Multimeter (LAN)

```lua
-- scripts/examples/scpi_multimeter.lua

spawn(function()
    local dmm = create_tcp_socket()
    
    -- Connect to multimeter (find IP in instrument settings)
    if not dmm:connect("192.168.1.10", 5025) then
        log("Failed to connect to multimeter")
        return
    end
    
    log("Connected to multimeter")
    
    -- Query instrument identity
    dmm:send("*IDN?\n")
    sleep(0.1)
    local id, err = dmm:receive(1024)
    if id then
        log("Instrument: " .. id)
    end
    
    -- Configure for DC voltage measurement
    dmm:send(":CONFigure:VOLTage:DC\n")
    sleep(0.05)
    
    -- Continuous measurement loop
    while is_app_running() do
        -- Trigger measurement and read
        dmm:send(":READ?\n")
        sleep(0.1)
        
        local response, err = dmm:receive(1024)
        if response and #response > 0 then
            local voltage = tonumber(response)
            if voltage then
                update_signal("DMM.Voltage", voltage)
                log(string.format("Voltage: %.6f V", voltage))
            end
        end
        
        sleep(1.0)  -- 1 Hz sample rate
    end
    
    dmm:close()
end)

log("SCPI multimeter reader loaded!")
```

### Example 2: Rigol Oscilloscope (LAN)

```lua
-- scripts/examples/scpi_oscilloscope.lua

spawn(function()
    local scope = create_tcp_socket()
    
    if not scope:connect("192.168.1.20", 5555) then  -- Rigol uses 5555
        log("Failed to connect to oscilloscope")
        return
    end
    
    log("Connected to oscilloscope")
    
    -- Get waveform data from channel 1
    while is_app_running() do
        -- Set waveform source to channel 1
        scope:send(":WAVeform:SOURce CHANnel1\n")
        sleep(0.05)
        
        -- Set format to ASCII (easier to parse)
        scope:send(":WAVeform:FORMat ASCII\n")
        sleep(0.05)
        
        -- Request waveform data
        scope:send(":WAVeform:DATA?\n")
        sleep(0.2)
        
        local data, err = scope:receive(65536)
        if data and #data > 0 then
            -- Parse ASCII waveform data
            -- Format: "#9000001200<data points>"
            local points = {}
            for value in string.gmatch(data, "([^,]+)") do
                table.insert(points, tonumber(value))
            end
            
            if #points > 0 then
                log("Captured " .. #points .. " waveform points")
                -- Could plot these points in a time-series plot
            end
        end
        
        sleep(0.5)  -- 2 Hz refresh
    end
    
    scope:close()
end)

log("SCPI oscilloscope reader loaded!")
```

### Example 3: Power Supply Control (Serial/USB)

```lua
-- scripts/examples/scpi_power_supply.lua

spawn(function()
    local psu = create_serial_port()
    
    -- Many USB power supplies appear as serial ports
    if not psu:open("COM5", 9600) then
        log("Failed to open power supply")
        return
    end
    
    log("Connected to power supply")
    
    -- Set voltage to 5.0V
    psu:send("VOLT 5.0\n")
    sleep(0.1)
    
    -- Set current limit to 1.0A
    psu:send("CURR 1.0\n")
    sleep(0.1)
    
    -- Enable output
    psu:send("OUTP ON\n")
    sleep(0.1)
    
    -- Monitor voltage and current
    while is_app_running() do
        -- Measure voltage
        psu:send("MEAS:VOLT?\n")
        sleep(0.1)
        local volt_str, err = psu:receive_async(256)
        if volt_str then
            local voltage = tonumber(volt_str)
            if voltage then
                update_signal("PSU.Voltage", voltage)
            end
        end
        
        -- Measure current
        psu:send("MEAS:CURR?\n")
        sleep(0.1)
        local curr_str, err = psu:receive_async(256)
        if curr_str then
            local current = tonumber(curr_str)
            if current then
                update_signal("PSU.Current", current)
            end
        end
        
        sleep(0.5)  -- 2 Hz sample rate
    end
    
    -- Disable output on exit
    psu:send("OUTP OFF\n")
    psu:close()
end)

log("SCPI power supply monitor loaded!")
```

### Common SCPI Commands

Most SCPI instruments support these standard commands:

```
*IDN?           - Get instrument identity
*RST            - Reset to defaults
*CLS            - Clear status
:MEASure?       - Trigger measurement
:READ?          - Read last measurement
:CONFigure      - Configure measurement type
:SYSTem:ERRor?  - Read error queue
```

### Finding Instrument IP Address

**Keysight/Agilent:**
- Press `Utility` → `I/O` → `LAN` → View IP

**Rigol:**
- Press `Utility` → `I/O Setting` → `LAN Config`

**Tektronix:**
- Press `Utility` → `System` → `Network`

### Testing Without Hardware

Use **PyVISA simulator** or **netcat**:

```python
# Simple SCPI server simulator
import socket

s = socket.socket()
s.bind(('0.0.0.0', 5025))
s.listen(1)

while True:
    conn, addr = s.accept()
    while True:
        data = conn.recv(1024)
        if b'*IDN?' in data:
            conn.send(b'ACME,DMM1000,12345,1.0\n')
        elif b':READ?' in data:
            conn.send(b'1.234567\n')  # Fake voltage
```

---

## Summary

**MAVLink:**
- ✅ Works with existing UDP sockets
- ✅ Parser in pure Lua (no C++ needed)
- ✅ Standard port 14550
- 📝 Can extend with full MAVLink v2 support

**SCPI:**
- ✅ Works with existing TCP sockets (LAN instruments)
- ✅ Works with existing Serial sockets (USB instruments)
- ✅ Simple text-based protocol
- 📝 Almost every lab has SCPI equipment

Both protocols require **zero code changes** to your GUI - just Lua scripts!
