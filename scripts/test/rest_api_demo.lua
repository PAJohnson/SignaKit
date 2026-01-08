-- rest_api_demo.lua
-- Prototype of a REST API client built on raw TCP sockets

print("========================================")
print("rest_api_demo.lua - Raw Socket REST Prototype")
print("========================================")

-- Helper function for raw HTTP GET
local function http_get(host, path, port)
    port = port or 80
    local tcp = create_tcp_socket()
    
    print(string.format("[HTTP] GET http://%s%s", host, path))
    
    local success, err = tcp:connect_async(host, port)
    if not success then
        return nil, "connection failed: " .. tostring(err)
    end
    
    -- Formulate HTTP/1.1 request
    local request = string.format(
        "GET %s HTTP/1.1\r\n" ..
        "Host: %s\r\n" ..
        "User-Agent: SignaKit-Telemetry-GUI/1.0\r\n" ..
        "Accept: */*\r\n" ..
        "Connection: close\r\n\r\n",
        path, host
    )
    
    tcp:send_async(request)
    
    -- Read response
    local response = ""
    while true do
        local chunk, rerr = tcp:receive_async(4096)
        if chunk then
            response = response .. chunk
        elseif rerr == "closed" then
            break
        else
            return nil, "receive error: " .. tostring(rerr)
        end
    end
    
    tcp:close()
    
    -- Basic parsing (split headers and body)
    local header_end = response:find("\r\n\r\n", 1, true)
    if not header_end then
        return response, "incomplete response"
    end
    
    local headers = response:sub(1, header_end - 1)
    local body = response:sub(header_end + 4)
    
    return body, nil, headers
end

spawn(function()
    print("[REST] Requesting data from httpbin.org...")
    
    -- Testing with httpbin.org (public API)
    -- Note: This is plain HTTP (port 80), not HTTPS. 
    -- Raw sockets don't support TLS/SSL without an extra library like OpenSSL.
    local body, err, headers = http_get("httpbin.org", "/get?msg=HelloSignaKit")
    
    if body then
        -- print("[REST] Headers:\n" .. headers)
        print("[REST] Response Body:\n" .. body)
        
        -- Try to extract a value if it's JSON
        local msg = body:match('"msg":%s*"([^"]+)"')
        if msg then
            print("[REST] Extracted msg: " .. msg)
        end
    else
        print("[REST] Error: " .. tostring(err))
    end
end)
