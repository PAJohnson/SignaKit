# SignaKit - Program Documentation

## Overview

SignaKit is a real-time data visualization tool designed to receive packed C struct telemetry data over UDP, parse it using Lua scripts, and display it in interactive, draggable plots.

## Architecture

### Components

The project consists of two main executables:

1. **signakit** - Main GUI application
2. **mock_device** - Test data generator

### Data Flow

```
┌─────────────────┐
│  Data Source    │ (e.g., mock_device, embedded device, etc.)
│  (UDP Sender)   │
└────────┬────────┘
         │ UDP packets (packed C structs)
         │ Port 5000
         ▼
┌─────────────────────────────────────────────────────────┐
│                  SignaKit Application                    │
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │  Main GUI Thread (60 FPS)                          │ │
│  │  ┌──────────────────────────────────────────────┐ │ │
│  │  │  Lua Frame Callbacks                         │ │ │
│  │  │  - scripts/io/DataSource.lua                 │ │ │
│  │  │  - Non-blocking UDP socket receive           │ │ │
│  │  │  - Calls Lua parsers with raw bytes          │ │ │
│  │  └──────────────────┬───────────────────────────┘ │ │
│  │                     │                              │ │
│  │                     ▼                              │ │
│  │  ┌──────────────────────────────────────────────┐ │ │
│  │  │  Lua Parsers (scripts/parsers/*.lua)         │ │ │
│  │  │  - Extract fields from raw bytes             │ │ │
│  │  │  - update_signal() to store data             │ │ │
│  │  └──────────────────┬───────────────────────────┘ │ │
│  └────────────────────┬┴───────────────────────────────┘ │
│                       │                                  │
│                       ▼                                  │
│  ┌────────────────────────────────────────────────────┐ │
│  │  Signal Registry (C++)                             │ │
│  │  - "IMU.accelX" -> {dataX[], dataY[], offset}      │ │
│  │  - "IMU.accelY" -> ...                             │ │
│  │  - "GPS.latitude" -> ...                           │ │
│  │  - Protected by stateMutex                         │ │
│  └────────────────┬───────────────────────────────────┘ │
│                   │                                      │
│                   │ Read by GUI rendering                │
│                   ▼                                      │
│  ┌────────────────────────────────────────────────────┐ │
│  │  GUI Rendering (ImGui/ImPlot)                      │ │
│  │  ┌──────────────────┐  ┌──────────────────────┐   │ │
│  │  │  Signal Browser  │  │  Plot Windows        │   │ │
│  │  │  - List all      │  │  - Multiple plots    │   │ │
│  │  │    signals       │  │  - Drag & drop       │   │ │
│  │  │  - Drag source   │  │  - Real-time update  │   │ │
│  │  └──────────────────┘  └──────────────────────┘   │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

**Note**: As of Tier 5 (Total Luafication), all network I/O is handled by Lua scripts
running as frame callbacks. See [docs/LuaTotalLuafication.md](LuaTotalLuafication.md)
for details.

## Component Details

### 1. signakit

**Purpose**: Real-time visualization of telemetry data streams.

**Key Features**:
- Dynamic packet parsing using Lua scripts
- Live plotting with ImPlot (up to 2000 samples per signal)
- Drag-and-drop signal assignment to plots
- Multiple independent plot windows
- Pause/resume functionality per plot
- Circular buffer for efficient memory usage

**Main Components**:

- **Lua I/O System** (`scripts/io/DataSource.lua`)
  - Runs as frame callback at 60 FPS
  - Non-blocking UDP socket operations (via `sockpp` library)
  - Receives packets and dispatches to Lua parsers
  - Handles both online (UDP) and offline (file replay) modes
  - Updates signal buffers via `update_signal()` API

- **Lua Parser System** (`scripts/parsers/*.lua`)
  - Registered parsers handle different packet types
  - Extract fields from raw byte buffers
  - Create/update signals dynamically
  - See [docs/LuaPacketParsing.md](LuaPacketParsing.md) for details

- **Signal Registry** (`src/ui_state.hpp`)
  - Maps signal names to data buffers
  - Each signal stores X (time) and Y (value) arrays
  - Circular buffer with configurable size (default 2000 samples)
  - Protected by stateMutex for thread-safe access

- **GUI Rendering** (`src/main.cpp`)
  - Left panel: Signal browser with drag sources
  - Right area: Dynamic plot windows
  - Each plot can contain multiple signals
  - Auto-scaling X-axis follows latest data

**Performance Optimizations**:
- Uses `SDL_WaitEventTimeout()` to reduce CPU usage when idle
- Non-blocking I/O prevents GUI freezing
- Windows-specific vsync hints for smooth rendering
- Static linking for portable executables
- All I/O in Lua allows rapid protocol adaptation without recompilation

### 2. mock_device

**Purpose**: Generate simulated telemetry data for testing.

**Functionality**:
- Sends two types of packets: IMU and GPS
- IMU data at ~100 Hz (accelerometer + gyroscope)
- GPS data at ~10 Hz (latitude, longitude, altitude, speed)
- Uses sinusoidal patterns for realistic-looking waveforms
- Sends to localhost:5000 by default

**Configuration**:
```cpp
#define DEST_IP "127.0.0.1"
#define DEST_PORT 5000
```

**Data Generation**:
- IMU: Sine/cosine waves for acceleration, random noise for gyro
- GPS: Circular path around NYC coordinates
- Proper packet headers and time stamps

## Adding New Data Structures

Follow these steps to add support for a new telemetry packet type:

### Step 1: Define the C Struct

Create or update `src/telemetry_defs.h`:

```cpp
// Example: Adding a Battery packet
#include <cstdint>  // For uint8_t, uint16_t, etc.

struct BatteryData {
  char header[4];      // "BAT\0"
  double time;         // Timestamp
  float voltage;       // Battery voltage (V)
  float current;       // Battery current (A)
  float temperature;   // Temperature (°C)
  uint8_t percentage;  // State of charge (0-100%)
  uint8_t health;      // Battery health (0-100%)
  uint16_t cycleCount; // Number of charge cycles
};
```

**Important**:
- First field should be `char header[4]` for packet identification
- Include a timestamp field (typically `double time`)
- Pay attention to struct padding and alignment
- Calculate total struct size: `sizeof(BatteryData)`

### Step 2: Create a Lua Parser

Create a parser script in `scripts/parsers/` directory. For example, `scripts/parsers/battery.lua`:

```lua
register_parser("Battery", function(buffer, length)
    -- Check packet header and size
    local header = readString(buffer, 0, 3)
    if header ~= "BAT" or length < 26 then
        return false  -- Not our packet
    end

    -- Parse fields
    local time = readDouble(buffer, 4, true)   -- Little-endian double at offset 4
    local voltage = readFloat(buffer, 12, true)
    local current = readFloat(buffer, 16, true)
    local temperature = readFloat(buffer, 20, true)
    local percentage = readUInt8(buffer, 24)
    local health = readUInt8(buffer, 25)
    local cycleCount = readUInt16(buffer, 26, true)

    -- Update signals
    update_signal("Battery.voltage", time, voltage)
    update_signal("Battery.current", time, current)
    update_signal("Battery.temperature", time, temperature)
    update_signal("Battery.percentage", time, percentage)
    update_signal("Battery.health", time, health)
    update_signal("Battery.cycleCount", time, cycleCount)

    return true  -- Successfully parsed
end)
```

See [docs/LuaPacketParsing.md](LuaPacketParsing.md) for complete parser API documentation.

### Step 3: Update Data Source

If using `mock_device`, add a new thread function:

```cpp
#ifdef _WIN32
void battery_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void battery_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    BatteryData packet;
    memcpy(packet.header, "BAT", 4);

    std::cout << "[Battery] Thread started (1Hz)..." << std::endl;

    while (running) {
        double t = get_time();

        packet.time = t;
        packet.voltage = 12.0f + 0.5f * sin(t * 0.1f);  // Simulate discharge
        packet.current = 5.0f + 1.0f * cos(t * 0.2f);   // Variable load
        packet.temperature = 25.0f + 3.0f * sin(t * 0.05f);
        packet.percentage = (uint8_t)(100 - (t / 10.0));  // Slow discharge (0-100%)
        packet.health = (uint8_t)(100 - (t / 100.0));     // Very slow degradation
        packet.cycleCount = (uint16_t)(t / 5.0);          // Increment every 5 seconds

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));

        // ~1Hz
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

// In main():
std::thread batteryThread(battery_thread_func, sockfd, dest_addr);
// ...
batteryThread.join();
```

### Step 4: Test

1. **Run the GUI**:
   ```bash
   ./signakit
   ```

3. **Start data source**:
   ```bash
   ./mock_device
   ```

4. **Check the signal browser** - New signals should appear:
   - Battery.voltage
   - Battery.current
   - Battery.temperature
   - Battery.percentage
   - Battery.health
   - Battery.cycleCount

5. **Drag signals to a plot** to visualize them in real-time.

## Usage Examples

### Basic Workflow

1. Launch SignaKit:
   ```bash
   ./signakit
   ```

2. Start a data source (e.g., mock_device):
   ```bash
   ./mock_device
   ```

3. In the GUI:
   - Browse available signals in the left panel
   - Click "Add New Plot Window" to create a plot
   - Drag signals from the browser into plot areas
   - Use Pause/Resume to freeze/unfreeze plots
   - Close plot windows when done

### Multiple Data Sources

The GUI can receive data from multiple sources simultaneously:
- Each source should send properly formatted packets
- Packet type is identified by header string and size
- All sources send to the same UDP port (5000)

### Modifying Plot Window Size

Edit `src/main.cpp:426`:
```cpp
SDL_Window *window =
    SDL_CreateWindow("SignaKit Analyzer", SDL_WINDOWPOS_CENTERED,
                     SDL_WINDOWPOS_CENTERED, 1920, 1080, window_flags);
                                         // ^^^^ Change resolution here
```

### Changing UDP Port

Edit `src/main.cpp:35`:
```cpp
#define UDP_PORT 5000  // Change this value
```

Also update in `src/mock_device.cpp:21`:
```cpp
#define DEST_PORT 5000  // Must match
```

## Thread Safety

The application uses a single-threaded architecture with frame callbacks:

1. **Main GUI Thread** - Handles everything at 60 FPS:
   - GUI rendering (ImGui/ImPlot)
   - Lua frame callbacks (including I/O via DataSource.lua)
   - Signal registry access
   - User input

**Synchronization**:
- `std::mutex stateMutex` protects the signal registry
- Lua frame callbacks execute with `stateMutex` held
- All signal updates and reads are mutex-protected
- Non-blocking I/O prevents frame drops

**Benefits**:
- Simpler architecture (no thread coordination)
- No race conditions or deadlocks
- Easier debugging
- Sufficient performance (60 FPS × 100 packets/frame = 6000+ packets/sec)

## Performance Characteristics

### Memory Usage
- Each signal stores 2000 samples by default (configurable)
- ~32 KB per signal (2000 × 2 × 8 bytes for double precision)
- Circular buffer prevents unbounded growth

### CPU Usage
- Near-zero when idle (thanks to `SDL_WaitEventTimeout`)
- ~1-5% during active plotting on modern hardware
- Non-blocking I/O minimal overhead

### Latency
- Sub-millisecond from packet arrival to display update
- Bounded by vsync (typically 16.67ms at 60 Hz)

## Troubleshooting

### No Data Appearing in Plots

1. Verify the data source is sending to `127.0.0.1:5000`
2. Check firewall settings (Windows Firewall may block UDP)
3. Check console output for Lua parser errors
4. Verify your parser script is in `scripts/parsers/` directory

### Signals Not Appearing in Browser

1. Check console output for parser errors
2. Verify packet structure matches your Lua parser
3. Check packet size with `sizeof(YourStruct)`
4. Ensure header string matches what your parser expects
5. Add debug logging to your parser with `log()` function

### Plot Performance Issues

1. Reduce number of signals per plot
2. Decrease sample buffer size in `Signal` constructor
3. Lower data transmission rate from source
4. Check that vsync is enabled

### Windows Firewall Blocking UDP

Allow the application through Windows Firewall:
```powershell
New-NetFirewallRule -DisplayName "SignaKit" -Direction Inbound -Program "C:\path\to\signakit.exe" -Action Allow
```
