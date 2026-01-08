# Image Window API Documentation (Updated)

The Image Window API allows displaying images in the GUI. Windows are created visually through the menu, and Lua scripts control the content.

## Creating an Image Window

Image windows are created through the GUI menu:

1. Click **Add New...** in the Signal Browser
2. Select **Image Display**
3. A new window appears (e.g., "Image 1")

The window title can be renamed by clicking on it (similar to other windows).

## Loading Images from Files

```lua
local success = set_image_file(window_title, file_path)
```

Parameters:
- `window_title` (string): Title of the image window (e.g., "Image 1")
- `file_path` (string): Path to image file

Returns:
- `true` if successful, `false` otherwise

Example:
```lua
-- Load a PNG file into the window titled "Image 1"
set_image_file("Image 1", "C:/images/camera_frame.png")
```

## Updating Images from UDP/Network (Live Streaming)

```lua
local success = update_image_buffer(window_title, image_data)
```

Parameters:
- `window_title` (string): Title of the image window
- `image_data` (string): Raw image file data (JPEG, PNG, etc.)

Returns:
- `true` if successful, `false` otherwise

This function accepts encoded image data (JPEG, PNG, BMP, etc.) and automatically decodes it using stb_image. Perfect for receiving images over UDP/TCP.

Example - Receiving JPEG from UDP:
```lua
spawn(function()
    local socket = create_udp_socket()
    socket:bind("0.0.0.0", 5555)
    
    while is_app_running() do
        local jpeg_data, err = socket:receive_async(65536)
        
        if jpeg_data and #jpeg_data > 0 then
            -- Decode and display the JPEG
            update_image_buffer("Image 1", jpeg_data)
            log("Updated image from network (" .. #jpeg_data .. " bytes)")
        end
        
        sleep(0.1)  -- 10 FPS update rate
    end
    
    socket:close()
end)
```

## Supported Image Formats

Both functions support:
- JPEG
- PNG
- BMP
- TGA
- PSD
- GIF (first frame only)
- HDR
- PIC

## Window Controls

Each image window has built-in controls:
- **Fit to Window**: Scales image to fit available space while maintaining aspect ratio
- **Actual Size**: Displays image at its original resolution

## Notes

- Windows must be created via GUI before Lua can set their content
- Image data is automatically decoded - no need to handle decompression in Lua
- For video streams, call `update_image_buffer()` for each frame
- Memory usage scales with image resolution
- Old textures are automatically cleaned up when loading new images
- Window titles must match exactly (case-sensitive)

## Performance Tips

- For high frame rate video (>30 FPS), consider reducing update frequency
- JPEG compression provides good balance of quality and network bandwidth
- Monitor memory usage for large images (>4K resolution)
- The `update_image_buffer()` function is designed for ~10 FPS streaming

## Example: Camera Stream from UDP

```lua
-- On the camera/sender side, send JPEG frames over UDP
-- Example Python code:
-- import socket, cv2
-- sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
-- while True:
--     ret, frame = camera.read()
--     _, jpeg = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 80])
--     sock.sendto(jpeg.tobytes(), ('localhost', 5555))
--     time.sleep(0.1)

-- On the telemetry GUI side:
spawn(function()
    local socket = create_udp_socket()
    socket:bind("0.0.0.0", 5555)
    log("Receiving camera stream on UDP 5555...")
    
    while is_app_running() do
        local frame_data, err = socket:receive_async(65536)
        if frame_data and #frame_data > 0 then
            update_image_buffer("Camera View", frame_data)
        end
        sleep(0.05)  -- 20 FPS
    end
end)
```
