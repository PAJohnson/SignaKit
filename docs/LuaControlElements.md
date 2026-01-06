# Lua Control Elements (Tier 4)

## Overview

Tier 4 adds interactive GUI control elements (Buttons, Toggles, and Text Inputs) that expose their state to Lua frame callbacks. This enables dynamic control panels for sending commands, adjusting parameters, and interacting with your telemetry system.

## Creating Control Elements

Control elements are created via the **"Add New..."** menu in the Signal Browser:

1. Click **"Add New..."**
2. Select **Button**, **Toggle**, or **Text Input**
3. A new control window appears with default settings
4. Edit the **Title** and **Label** fields to customize

## Control Types

### 1. Button Control

**Purpose**: Trigger actions when clicked

**Properties**:
- `title` - Window title (editable)
- `buttonLabel` - Button text (editable)
- `clicked` - True for **one frame** after button is clicked

**Visual Layout**:
```
┌─────────────────────────┐
│ Title: [Button 1      ] │  ← Editable title
├─────────────────────────┤
│ Label: [Send Command  ] │  ← Editable label
│                         │
│    ┌───────────────┐    │
│    │ Send Command  │    │  ← Clickable button
│    └───────────────┘    │
└─────────────────────────┘
```

### 2. Toggle Control

**Purpose**: Binary on/off state

**Properties**:
- `title` - Window title (editable)
- `toggleLabel` - Checkbox label (editable)
- `state` - Boolean value (true/false)

**Visual Layout**:
```
┌─────────────────────────┐
│ Title: [Toggle 1      ] │  ← Editable title
├─────────────────────────┤
│ Label: [Enable Logging] │  ← Editable label
│                         │
│      ☑ Enable Logging   │  ← Checkbox
└─────────────────────────┘
```

### 3. Text Input Control

**Purpose**: Enter text/numeric values

**Properties**:
- `title` - Window title (editable)
- `inputLabel` - Input field label (editable)
- `textBuffer` - Current text value (string)
- `enterPressed` - True for **one frame** after Enter is pressed

**Visual Layout**:
```
┌─────────────────────────┐
│ Title: [Text Input 1  ] │  ← Editable title
├─────────────────────────┤
│ Label: [PID P Gain    ] │  ← Editable label
├─────────────────────────┤
│ PID P Gain              │
│ [1.5_____________]      │  ← Text input field
│ Press Enter to submit   │
└─────────────────────────┘
```

## Lua API

### Button Functions

#### `get_button_clicked(buttonTitle)`
Returns `true` if the button was clicked **this frame**, otherwise `false`.

**Parameters**:
- `buttonTitle` (string) - The title of the button window

**Returns**: `boolean`

**Example**:
```lua
if get_button_clicked("Send Command") then
    log("Command button clicked!")
end
```

**Important**: The `clicked` state is only `true` for **one frame**. Check it every frame in your callback.

---

### Toggle Functions

#### `get_toggle_state(toggleTitle)`
Returns the current state of the toggle (true = checked, false = unchecked).

**Parameters**:
- `toggleTitle` (string) - The title of the toggle window

**Returns**: `boolean`

**Example**:
```lua
if get_toggle_state("Enable Logging") then
    -- Logging is enabled
    log("Logging active")
end
```

#### `set_toggle_state(toggleTitle, state)`
Programmatically set the toggle state from Lua.

**Parameters**:
- `toggleTitle` (string) - The title of the toggle window
- `state` (boolean) - New state (true/false)

**Example**:
```lua
-- Turn on logging toggle
set_toggle_state("Enable Logging", true)
```

---

### Text Input Functions

#### `get_text_input(inputTitle)`
Returns the current text in the input field, or `nil` if not found.

**Parameters**:
- `inputTitle` (string) - The title of the text input window

**Returns**: `string` or `nil`

**Example**:
```lua
local gain = get_text_input("PID P Gain")
if gain then
    local value = tonumber(gain)
    log("P Gain: " .. tostring(value))
end
```

#### `get_text_input_enter_pressed(inputTitle)`
Returns `true` if Enter was pressed **this frame** in the input field.

**Parameters**:
- `inputTitle` (string) - The title of the text input window

**Returns**: `boolean`

**Example**:
```lua
if get_text_input_enter_pressed("PID P Gain") then
    local gain = get_text_input("PID P Gain")
    log("User submitted: " .. gain)
end
```

**Important**: The `enterPressed` state is only `true` for **one frame**.

#### `set_text_input(inputTitle, text)`
Programmatically set the text input value from Lua.

**Parameters**:
- `inputTitle` (string) - The title of the text input window
- `text` (string) - New text value

**Example**:
```lua
-- Set default value
set_text_input("PID P Gain", "1.5")
```

---

## Complete Examples

### Example 1: Button Click Logger

```lua
-- File: scripts/callbacks/button_logger.lua
-- Logs when any button is clicked

on_frame(function()
    -- Check multiple buttons
    if get_button_clicked("Button 1") then
        log("Button 1 pressed!")
    end

    if get_button_clicked("Send Command") then
        log("Send Command pressed!")
    end

    if get_button_clicked("Reset System") then
        log("Reset System pressed!")
    end
end)
```

### Example 2: Toggle-Based Feature Control

```lua
-- File: scripts/callbacks/toggle_control.lua
-- Enable/disable features based on toggles

on_frame(function()
    -- Check if data logging is enabled
    if get_toggle_state("Enable Logging") then
        -- Log current altitude every frame
        local alt = get_signal("GPS.altitude")
        if alt then
            log("Altitude: " .. tostring(alt) .. " m")
        end
    end

    -- Check if alerts are enabled
    if get_toggle_state("Enable Alerts") then
        local battery = get_signal("BAT.voltage")
        if battery and battery < 3.3 then
            log("WARNING: Low battery!")
        end
    end
end)
```

### Example 3: PID Gain Adjustment

```lua
-- File: scripts/callbacks/pid_control.lua
-- Send PID gains when button is clicked

on_frame(function()
    -- Check if "Send Gains" button was clicked
    if get_button_clicked("Send Gains") then
        -- Read gain values from text inputs
        local p_gain = get_text_input("PID P Gain")
        local i_gain = get_text_input("PID I Gain")
        local d_gain = get_text_input("PID D Gain")

        -- Convert to numbers
        local p = tonumber(p_gain)
        local i = tonumber(i_gain)
        local d = tonumber(d_gain)

        if p and i and d then
            log(string.format("Sending PID gains: P=%.3f, I=%.3f, D=%.3f", p, i, d))
            -- TODO (Tier 5): Send gains over UDP/Serial
        else
            log("ERROR: Invalid PID gain values")
        end
    end
end)
```

### Example 4: Enter Key Detection

```lua
-- File: scripts/callbacks/quick_submit.lua
-- Submit text input when Enter is pressed

on_frame(function()
    -- Check if Enter was pressed in the command input
    if get_text_input_enter_pressed("Command Input") then
        local cmd = get_text_input("Command Input")
        log("Command entered: " .. cmd)

        -- Clear the input after submission
        set_text_input("Command Input", "")
    end
end)
```

### Example 5: Multi-Control Dashboard

```lua
-- File: scripts/callbacks/control_dashboard.lua
-- Complete control panel with buttons, toggles, and inputs

on_frame(function()
    -- Emergency stop button
    if get_button_clicked("Emergency Stop") then
        log("EMERGENCY STOP ACTIVATED!")
        set_toggle_state("Motors Enabled", false)
    end

    -- Start/Stop motors based on toggle
    local motors_enabled = get_toggle_state("Motors Enabled")
    if motors_enabled then
        -- Motors are running
        local throttle = get_text_input("Throttle %")
        if throttle then
            local throttle_val = tonumber(throttle)
            if throttle_val then
                log("Motors running at " .. throttle_val .. "%")
            end
        end
    end

    -- Auto-mode toggle
    if get_toggle_state("Auto Mode") then
        -- Set default throttle for auto mode
        set_text_input("Throttle %", "75")
    end

    -- Calibrate button
    if get_button_clicked("Calibrate") then
        log("Starting calibration sequence...")
        -- Set progress indicator
        set_text_input("Status", "Calibrating...")
    end
end)
```

---

## Use Cases

### 1. Command & Control
- **Buttons** to send commands (arm/disarm, reset, calibrate)
- **Toggles** to enable/disable features
- **Text Inputs** for parameter adjustment

### 2. Parameter Tuning
- Adjust PID gains, filter coefficients, thresholds
- Live parameter updates without recompilation
- Save/Load parameter presets

### 3. Real-time Configuration
- Enable/disable logging, alerts, features
- Switch between modes (manual/auto/test)
- Configure data sources dynamically

### 4. Testing & Debug
- Trigger test sequences
- Inject test values
- Toggle debug outputs

---

## Best Practices

### 1. One-Frame Events
Button clicks and Enter presses are **one-frame events**:
```lua
-- ✅ CORRECT: Check every frame
on_frame(function()
    if get_button_clicked("Reset") then
        perform_reset()
    end
end)

-- ❌ WRONG: Don't store in variable
local button_clicked = get_button_clicked("Reset")  -- Will always be false!
on_frame(function()
    if button_clicked then  -- Never triggers
        perform_reset()
    end
end)
```

### 2. Validation
Always validate text input values:
```lua
local gain = get_text_input("Gain")
local value = tonumber(gain)
if value and value >= 0 and value <= 10 then
    -- Valid input
else
    log("ERROR: Gain must be 0-10")
end
```

### 3. Unique Titles
Control elements are identified by their **title**. Use unique, descriptive titles:
```lua
-- ✅ GOOD
get_button_clicked("Send PID Gains")
get_toggle_state("Enable Data Logging")

-- ❌ BAD (generic, might conflict)
get_button_clicked("Button 1")
get_toggle_state("Toggle 1")
```

### 4. State Management
For persistent state, use toggles instead of buttons:
```lua
-- ✅ GOOD: Toggle for persistent enable/disable
if get_toggle_state("Motors Enabled") then
    run_motors()
end

-- ❌ LESS IDEAL: Button only triggers once
if get_button_clicked("Enable Motors") then
    run_motors()  -- Only runs for one frame!
end
```

---

## Layout Persistence

Control elements are saved/loaded with layouts:
- Window **position** and **size** are preserved
- **Title** and **label** text are saved
- **Toggle state** and **text input values** are saved
- Button click state is **not** saved (transient)

---

## Network I/O from Lua

You can implement network communication (UDP/Serial) entirely in Lua using external libraries like LuaSocket. Combined with control elements and frame callbacks, this enables complete command & control interfaces.

**Important**: When using persistent resources like sockets or file handles, always use cleanup callbacks to prevent resource leaks on script reload. See [LuaCleanupCallbacks.md](LuaCleanupCallbacks.md) for details.

**Example**: UDP Receiver Configuration
```lua
local udp_socket = nil

-- Function to recreate socket with new port
local function bind_socket(port)
    if udp_socket then udp_socket:close() end
    udp_socket = create_udp_socket()
    if udp_socket:bind("0.0.0.0", port) then
        udp_socket:set_non_blocking(true)
        log("Bound to port " .. port)
    else
        log("Failed to bind port " .. port)
        udp_socket = nil
    end
end

-- Cleanup
on_cleanup(function()
    if udp_socket then udp_socket:close() end
end)

-- Initial bind
bind_socket(12345)
set_text_input("Port", "12345")

on_frame(function()
    -- Re-bind if "Apply Port" button clicked
    if get_button_clicked("Apply Port") then
        local port_str = get_text_input("Port")
        if port_str then
            local port = tonumber(port_str)
            if port then
                bind_socket(port)
            end
        end
    end

    -- Process incoming data
    if udp_socket then
        local data, err = udp_socket:receive(1024)
        if data then
            log("Received: " .. data)
        end
    end
end)
```

---

## Future Enhancements

- **Timers**: Scheduled/periodic actions independent of frame rate (can be implemented in Lua with global state)
- **Sliders**: Continuous numeric input controls
- **Dropdowns**: Selection from predefined options

---

## Troubleshooting

### Control not found
```lua
local state = get_toggle_state("My Toggle")
-- Returns false if "My Toggle" doesn't exist
```

**Solution**: Ensure the control window title **exactly** matches the string in your script.

### Button click not detected
- Buttons are **one-frame events**
- Check the button **every frame** in `on_frame()`
- Don't cache the result outside the callback

### Text input returns nil
- Text input returns `nil` if the control doesn't exist
- Always check for `nil` before using the value

---

## See Also

- [LuaScripting.md](LuaScripting.md) - Tier 1: Signal Transforms
- [LuaPacketParsing.md](LuaPacketParsing.md) - Tier 2: Packet Parsing
- [LuaFrameCallbacks.md](LuaFrameCallbacks.md) - Tier 3: Frame Callbacks & Alerts
- [LuaCleanupCallbacks.md](LuaCleanupCallbacks.md) - Tier 4.5: Resource Cleanup
