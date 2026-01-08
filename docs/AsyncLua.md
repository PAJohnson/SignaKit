# Async Lua Scripting in SignaKit

SignaKit uses a coroutine-based cooperative multitasking framework for Lua scripts. This allows you to perform network I/O and other potentially blocking operations without stalling the GUI or other scripts.

## The Async Pattern

The core of the system is the `yield()` function. When a script calls `yield()`, it pauses execution and allows the C++ scheduler to run other coroutines or update the UI.

### Sockets and Non-Blocking I/O

All sockets in SignaKit (`UDPSocket` and `TCPSocket`) are **non-blocking by default**. To make them easy to use, we provide `_async` methods that handle polling and yielding for you.

#### UDP Example
```lua
local udp = create_udp_socket()
udp:bind("127.0.0.1", 12345)

spawn(function()
    while true do
        local data, err = udp:receive_async()
        if data then
            print("Received UDP: " .. data)
        end
    end
end)
```

#### TCP Example
```lua
local tcp = create_tcp_socket()

spawn(function()
    local success = tcp:connect_async("127.0.0.1", 8080)
    if success then
        tcp:send_async("Hello Server!")
        
        -- High-performance FFI receive
        local buf = SharedBuffer.new(1024)
        local len = tcp:receive_ptr_async(buf:get_ptr(), 1024)
        
        -- Normal string receive
        local resp = tcp:receive_async()
        print("Server said: " .. resp)
    end
end)
```

## REST APIs via Raw Sockets

Since `TCPSocket` is available, you can implement simple HTTP clients directly in Lua. Note that raw sockets do not support SSL/TLS (HTTPS).

```lua
function http_get(host, path)
    local tcp = create_tcp_socket()
    if tcp:connect_async(host, 80) then
        tcp:send_async("GET " .. path .. " HTTP/1.1\r\nHost: " .. host .. "\r\n\r\n")
        local response = ""
        -- ... read loop ...
        return response
    end
end
```
See `scripts/test/rest_api_demo.lua` for a complete implementation.

## Best Practices

1.  **Always use `spawn()`**: Scripts that need to run continuously or wait for I/O should be wrapped in `spawn()`.
2.  **Yield in Loops**: If you have a `while true` loop that doesn't use an `_async` helper, call `yield()` at the end of the loop to prevent "hanging" the GUI.
3.  **Check `is_app_running()`**: Use `while is_app_running() do` for long-running background tasks.
