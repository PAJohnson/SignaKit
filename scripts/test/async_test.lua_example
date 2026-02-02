-- async_test.lua
-- Verification script for the new Async Coroutine architecture

print("========================================")
print("async_test.lua - Verifying Coroutine Scheduler")
print("========================================")

-- Counter 1: High frequency (10Hz)
spawn(function()
    local count = 0
    print("[AsyncTest] Starting Counter 1 (10Hz)")
    while true do
        count = count + 1
        update_signal("Test.Counter1", get_time_seconds(), count)
        sleep(0.1) -- Sleep for 100ms
        
        if count % 50 == 0 then
            print("[AsyncTest] Counter 1 reached " .. count)
        end
    end
end)

-- Counter 2: Low frequency (1Hz)
spawn(function()
    local count = 0
    print("[AsyncTest] Starting Counter 2 (1Hz)")
    while true do
        count = count + 1
        update_signal("Test.Counter2", get_time_seconds(), count)
        sleep(1.0) -- Sleep for 1 second
        
        if count % 5 == 0 then
            print("[AsyncTest] Counter 2 reached " .. count)
        end
    end
end)

-- Background IO Simulator: Yields every frame
spawn(function()
    print("[AsyncTest] Starting IO Simulator")
    local last_time = get_time_seconds()
    while true do
        local t = get_time_seconds()
        local dt = t - last_time
        last_time = t
        
        -- Simulate "work"
        local val = math.sin(t)
        update_signal("Test.Sine", t, val)
        
        -- Yield until next frame
        yield()
    end
end)

print("[AsyncTest] All coroutines spawned")
