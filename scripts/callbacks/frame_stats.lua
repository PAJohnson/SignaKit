-- Frame Statistics Tracker
-- Demonstrates: Frame callbacks, statistics accumulation
--
-- This script tracks frame rate and timing statistics every second

local frameCount = 0
local timeAccumulator = 0.0
local minDeltaTime = math.huge
local maxDeltaTime = 0.0

on_frame(function()
    local dt = get_delta_time()
    local frameNum = get_frame_number()

    -- Accumulate statistics
    frameCount = frameCount + 1
    timeAccumulator = timeAccumulator + dt

    if dt < minDeltaTime then minDeltaTime = dt end
    if dt > maxDeltaTime then maxDeltaTime = dt end

    -- Print stats every second (~60 frames)
    if timeAccumulator >= 1.0 then
        local avgFPS = frameCount / timeAccumulator
        local avgFrameTime = (timeAccumulator / frameCount) * 1000.0

        log(string.format("Frame Stats - FPS: %.1f | Avg: %.2fms | Min: %.2fms | Max: %.2fms | Plots: %d",
            avgFPS, avgFrameTime, minDeltaTime * 1000.0, maxDeltaTime * 1000.0, get_plot_count()))

        -- Reset counters
        frameCount = 0
        timeAccumulator = 0.0
        minDeltaTime = math.huge
        maxDeltaTime = 0.0
    end
end)

log("Loaded frame_stats.lua - tracking FPS and frame timing")
