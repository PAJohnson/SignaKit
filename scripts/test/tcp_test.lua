-- tcp_test.lua
-- Verification script for the new TCPSocket support

print("========================================")
print("tcp_test.lua - Verifying TCP Async Sockets")
print("========================================")

spawn(function()
    print("[TCPTest] Starting TCP Client")
    local tcp = create_tcp_socket()
    
    -- Attempting connection to a non-existent port to test timeout/failure
    -- (In a real scenario, you'd connect to a server)
    local host = "127.0.0.1"
    local port = 12346
    
    print(string.format("[TCPTest] Connecting to %s:%d...", host, port))
    
    -- connect_async yields until connection is established or fails
    local success, err = tcp:connect_async(host, port)
    
    if success then
        print("[TCPTest] Connected!")
        tcp:send_async("Hello from SignaKit TCP!\n")
        
        while tcp:is_open() do
            local data, err = tcp:receive_async(1024)
            if data then
                print("[TCPTest] Received: " .. data)
            elseif err == "closed" then
                print("[TCPTest] Server closed connection")
                break
            else
                print("[TCPTest] Receive error: " .. tostring(err))
                break
            end
        end
    else
        print("[TCPTest] Connection failed (as expected if no server running): " .. tostring(err))
    end
    
    tcp:close()
    print("[TCPTest] Finished")
end)
