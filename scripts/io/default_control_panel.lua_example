-- Tier 5: Default Control Panel
-- This script creates the standard UDP telemetry interface using Tier 4 GUI controls
-- Users can customize this for their specific needs

print("========================================")
print("Setting up default UDP control panel...")
print("========================================")

-- Note: The control creation APIs from Tier 4 may not be fully implemented yet.
-- This script assumes the following APIs exist:
-- - control_exists(title) -> bool
-- - create_text_input(windowTitle, label, defaultValue)
-- - create_button(windowTitle, label)
-- - create_text_display(windowTitle, label, defaultValue)

-- For now, we'll use the existing Tier 4 APIs that read control states
-- The actual controls are created manually by the user through the GUI

-- Display instructions for manual setup
print("Manual Control Setup Instructions:")
print("-----------------------------------")
print("1. Use the GUI to create a 'Control Window'")
print("2. Add a Text Input control titled 'UDP IP' with default value '127.0.0.1'")
print("3. Add a Text Input control titled 'UDP Port' with default value '12345'")
print("4. Add a Button control titled 'UDP Connect'")
print("5. Add a Button control titled 'UDP Disconnect'")
print("")
print("Once these controls are created, the UDPDataSink.lua script will use them")
print("to manage the UDP connection.")
print("")
print("========================================")

-- Future: When Tier 4 programmatic control creation is available, uncomment:
--[[
local function setup_controls()
    -- Network Settings window
    local windowTitle = "Network Settings"

    -- IP Address text input
    if not control_exists("UDP IP") then
        create_text_input(windowTitle, "UDP IP", "127.0.0.1")
        print("✓ Created 'UDP IP' text input")
    end

    -- Port text input
    if not control_exists("UDP Port") then
        create_text_input(windowTitle, "UDP Port", "12345")
        print("✓ Created 'UDP Port' text input")
    end

    -- Connect button
    if not control_exists("UDP Connect") then
        create_button(windowTitle, "UDP Connect")
        print("✓ Created 'UDP Connect' button")
    end

    -- Disconnect button
    if not control_exists("UDP Disconnect") then
        create_button(windowTitle, "UDP Disconnect")
        print("✓ Created 'UDP Disconnect' button")
    end

    -- Connection status display (read-only text)
    if not control_exists("Connection Status") then
        create_text_display(windowTitle, "Connection Status", "Disconnected")
        print("✓ Created 'Connection Status' display")
    end

    print("========================================")
    print("Default UDP control panel setup complete!")
    print("========================================")
end

setup_controls()
]]--
