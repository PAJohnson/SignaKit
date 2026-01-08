-- Image Display Example Script (Updated)
-- This demonstrates loading images from files or UDP buffers

-- Example 1: Load image from file
-- First, create an "Image Display" window via the GUI menu (Add New... -> Image Display)
-- Then use this script to load content into it:

on_frame(function()
    -- Assuming you created an image window titled "Image 1" from the GUI
    local success = set_image_file("Image 1", "Capture.png")
    if success then
        log("Loaded image file into 'Image 1'")
    end
end)

-- Example 2: Receive JPEG from UDP and display
-- This demonstrates receiving image data from a UDP socket and displaying it

spawn(function()
    local socket = create_udp_socket()
    if not socket:bind("0.0.0.0", 5555) then
        log("Failed to bind UDP socket for images")
        return
    end
    
    log("Listening for JPEG images on UDP port 5555...")
    
    while is_app_running() do
        -- Receive image data (could be JPEG, PNG, etc.)
        local img_data, err = socket:receive_async(65536)
        
        if img_data and #img_data > 0 then
            -- Update the image window with the received JPEG data
            -- stb_image will automatically decode it
            local success = update_image_buffer("Image 1", img_data)
            if success then
                log("Updated image from UDP (" .. #img_data .. " bytes)")
            else
                log("Failed to update image (window not found or invalid data)")
            end
        end
        
        sleep(0.1)  -- Check for new images at ~10 Hz
    end
    
    socket:close()
end)

log("Image display script loaded! Create an 'Image Display' window from the menu.")
