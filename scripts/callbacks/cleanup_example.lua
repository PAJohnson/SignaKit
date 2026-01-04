-- File: scripts/callbacks/cleanup_example.lua
-- Demonstrates cleanup callback usage with a simple counter

-- Counter persists using global variable
counter = counter or 0

-- Simulated "resource" that needs cleanup
resource_active = true

-- Register cleanup callback
on_cleanup(function()
    log("[Cleanup] Cleanup callback executed!")
    log("[Cleanup] Final counter value: " .. counter)
    log("[Cleanup] Releasing simulated resource...")
    resource_active = false
end)

-- Frame callback
on_frame(function()
    -- Increment counter every 60 frames (~1 second)
    if get_frame_number() % 60 == 0 then
        counter = counter + 1
        log("Counter: " .. counter .. " (resource active: " .. tostring(resource_active) .. ")")
    end
end)

log("Cleanup example script loaded. Counter starts at: " .. counter)
log("Try reloading scripts to see the cleanup callback in action!")
