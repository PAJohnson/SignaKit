# Serial Port API Documentation

The serial port API allows Lua scripts to communicate with serial devices (COM ports) on Windows.

## Creating a Serial Port

```lua
local port = create_serial_port()
```

Creates a new serial port object. The port is not yet opened.

## Opening a Port

```lua
local success = port:open(port_name, baud_rate)
```

Parameters:
- `port_name` (string): Name of the COM port, e.g., `"COM1"`, `"COM3"`, `"\\\\.\\COM10"` for ports >= 10
- `baud_rate` (number, optional): Baud rate (default: 9600). Common values: 9600, 19200, 38400, 57600, 115200

Returns:
- `true` if successful, `false` otherwise

The port is configured with 8 data bits, no parity, 1 stop bit (8N1) by default.

## Configuring Port Parameters

```lua
local success = port:configure(baud_rate, parity, stop_bits, byte_size)
```

Parameters:
- `baud_rate` (number): Baud rate
- `parity` (string): Parity mode - `"none"`, `"odd"`, `"even"`, `"mark"`, or `"space"`
- `stop_bits` (number): Number of stop bits - `1` or `2`
- `byte_size` (number, optional): Number of data bits (default: 8)

Returns:
- `true` if successful, `false` otherwise

## Sending Data

```lua
local bytes_written, error = port:send(data)
```

Parameters:
- `data` (string): Data to send

Returns:
- `bytes_written` (number): Number of bytes written (-1 on error)
- `error` (string): Error message (empty string if successful)

## Receiving Data (Non-Blocking)

```lua
local data, error = port:receive(max_size)
```

Parameters:
- `max_size` (number): Maximum number of bytes to read

Returns:
- `data` (string): Received data (empty if no data available)
- `error` (string): Error message or `"timeout"` if no data available

## Receiving Data (Async - Recommended)

```lua
local data, error = port:receive_async(max_size)
```

Similar to `receive()`, but yields until data is available. Use this in coroutines spawned with `spawn()`.

Parameters:
- `max_size` (number, optional): Maximum bytes to read (default: 65536)

Returns:
- `data` (string): Received data
- `error` (string): Error message if failed

## Sending Data (Async)

```lua
local bytes_written, error = port:send_async(data)
```

Async version of `send()` that yields if needed.

## Checking Port Status

```lua
local is_open = port:is_open()
```

Returns:
- `true` if port is open, `false` otherwise

## Closing the Port

```lua
port:close()
```

Closes the serial port and releases resources.

## Example Usage

```lua
local port = create_serial_port()

spawn(function()
    -- Open COM3 at 115200 baud
    if not port:open("COM3", 115200) then
        log("Failed to open serial port")
        return
    end
    
    log("Serial port opened")
    
    -- Read data in a loop
    while is_app_running() do
        local data, err = port:receive_async(1024)
        
        if data and #data > 0 then
            log("Received: " .. #data .. " bytes")
            -- Process data here
        elseif err then
            log("Error: " .. err)
            break
        end
        
        sleep(0.01)  -- Yield for 10ms
    end
    
    port:close()
    log("Serial port closed")
end)
```

## Notes

- Serial ports are non-blocking by default
- Use `receive_async()` in coroutines for easier async programming
- Always close ports when done to release resources
- Error handling is important -check return values
- For binary data, you can use `string.byte()` and `string.char()` for byte manipulation
