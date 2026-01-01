# Telemetry GUI - Program Documentation

## Overview

The Telemetry GUI is a real-time data visualization tool designed to receive packed C struct telemetry data over UDP, parse it dynamically based on YAML configuration, and display it in interactive, draggable plots.

## Architecture

### Components

The project consists of three main executables:

1. **telemetry_gui** - Main GUI application
2. **mock_device** - Test data generator
3. **test_loader** - Configuration validation tool

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
│              Telemetry GUI Application                   │
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │  Network Receiver Thread                           │ │
│  │  - Binds to UDP port 5000                          │ │
│  │  - Receives raw packet buffers                     │ │
│  │  - Matches packets by size and header string       │ │
│  │  - Extracts fields using configured offsets/types  │ │
│  │  - Stores (time, value) pairs in Signal buffers    │ │
│  └────────────────┬───────────────────────────────────┘ │
│                   │                                      │
│                   │ Thread-safe access via mutex         │
│                   ▼                                      │
│  ┌────────────────────────────────────────────────────┐ │
│  │  Signal Registry (std::map<string, Signal>)        │ │
│  │  - "IMU.accelX" -> {dataX[], dataY[], offset}      │ │
│  │  - "IMU.accelY" -> ...                             │ │
│  │  - "GPS.latitude" -> ...                           │ │
│  └────────────────┬───────────────────────────────────┘ │
│                   │                                      │
│                   │ Read by GUI thread                   │
│                   ▼                                      │
│  ┌────────────────────────────────────────────────────┐ │
│  │  Main GUI Thread (ImGui/ImPlot)                    │ │
│  │  ┌──────────────────┐  ┌──────────────────────┐   │ │
│  │  │  Signal Browser  │  │  Plot Windows        │   │ │
│  │  │  - List all      │  │  - Multiple plots    │   │ │
│  │  │    signals       │  │  - Drag & drop       │   │ │
│  │  │  - Drag source   │  │  - Real-time update  │   │ │
│  │  └──────────────────┘  └──────────────────────┘   │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

## Component Details

### 1. telemetry_gui

**Purpose**: Real-time visualization of telemetry data streams.

**Key Features**:
- Dynamic packet parsing based on YAML configuration
- Live plotting with ImPlot (up to 2000 samples per signal)
- Drag-and-drop signal assignment to plots
- Multiple independent plot windows
- Pause/resume functionality per plot
- Circular buffer for efficient memory usage

**Main Components**:

- **NetworkReceiverThread** (`src/main.cpp:106-197`)
  - Runs in background thread
  - Loads `signals.yaml` configuration
  - Initializes signal registry
  - Receives UDP packets on port 5000
  - Parses packets based on configuration
  - Updates signal buffers (thread-safe)

- **Signal Registry** (`src/main.cpp:83`)
  - Maps signal names to data buffers
  - Each signal stores X (time) and Y (value) arrays
  - Circular buffer with configurable size (default 2000 samples)

- **GUI Rendering** (`src/main.cpp:246-390`)
  - Left panel: Signal browser with drag sources
  - Right area: Dynamic plot windows
  - Each plot can contain multiple signals
  - Auto-scaling X-axis follows latest data

**Performance Optimizations**:
- Uses `SDL_WaitEventTimeout()` to reduce CPU usage when idle
- Mutex-protected data access between network and GUI threads
- Windows-specific vsync hints for smooth rendering
- Static linking for portable executables

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

### 3. test_loader

**Purpose**: Validate YAML configuration file parsing.

**Usage**:
```bash
./test_loader
```

Verifies that `signals.yaml` can be successfully loaded and parsed.

## Configuration System

### signals.yaml Format

The configuration file defines packet structures and signal mappings:

```yaml
packets:
  - id: "IMU"                    # Packet identifier
    header_string: "IMU"         # First bytes of packet (for identification)
    size_check: 36               # Expected packet size in bytes
    time_field: "time"           # Which field contains timestamp
    fields:
      - {name: "time",   type: "double", offset: 4}
      - {name: "accelX", type: "float",  offset: 12}
      - {name: "accelY", type: "float",  offset: 16}
      - {name: "accelZ", type: "float",  offset: 20}
      - {name: "gyroX",  type: "float",  offset: 24}
      - {name: "gyroY",  type: "float",  offset: 28}
      - {name: "gyroZ",  type: "float",  offset: 32}

  - id: "GPS"
    header_string: "GPS"
    size_check: 36
    time_field: "time"
    fields:
      - {name: "time",      type: "double", offset: 4}
      - {name: "latitude",  type: "double", offset: 12}
      - {name: "longitude", type: "double", offset: 20}
      - {name: "altitude",  type: "float",  offset: 28}
      - {name: "speed",     type: "float",  offset: 32}
```

**Signal Auto-Generation**:
- Each non-time field automatically becomes a signal
- Signal names: `<packet_id>.<field_name>` (e.g., "IMU.accelX")
- Signals appear in the GUI's signal browser

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

### Step 2: Update signals.yaml

Add the new packet definition:

```yaml
packets:
  # ... existing IMU and GPS packets ...

  - id: "Battery"
    header_string: "BAT"
    size_check: 26              # sizeof(BatteryData)
    time_field: "time"
    fields:
      - {name: "time",        type: "double",  offset: 4}   # After header
      - {name: "voltage",     type: "float",   offset: 12}
      - {name: "current",     type: "float",   offset: 16}
      - {name: "temperature", type: "float",   offset: 20}
      - {name: "percentage",  type: "uint8_t", offset: 24}
      - {name: "health",      type: "uint8_t", offset: 25}
      - {name: "cycleCount",  type: "uint16_t", offset: 26}
```

**Field Offsets**:
- Calculate carefully based on struct layout
- Use `offsetof()` macro for verification if needed:
  ```cpp
  printf("voltage offset: %zu\n", offsetof(BatteryData, voltage));
  ```

**Supported Types**:

Floating Point:
- `double` - 8 bytes, double precision floating point
- `float` - 4 bytes, single precision floating point

Signed Integers (stdint.h):
- `int8_t` or `int8` - 1 byte, signed integer (-128 to 127)
- `int16_t` or `int16` - 2 bytes, signed integer (-32,768 to 32,767)
- `int32_t` or `int32` or `int` - 4 bytes, signed integer (-2^31 to 2^31-1)
- `int64_t` or `int64` - 8 bytes, signed integer (-2^63 to 2^63-1)

Unsigned Integers (stdint.h):
- `uint8_t` or `uint8` - 1 byte, unsigned integer (0 to 255)
- `uint16_t` or `uint16` - 2 bytes, unsigned integer (0 to 65,535)
- `uint32_t` or `uint32` - 4 bytes, unsigned integer (0 to 2^32-1)
- `uint64_t` or `uint64` - 8 bytes, unsigned integer (0 to 2^64-1)

Legacy C Types (backwards compatibility):
- `char` - typically 1 byte, signed or unsigned depending on platform
- `short` - typically 2 bytes, signed integer
- `long` - platform-dependent size, signed integer
- `unsigned char` - typically 1 byte, unsigned integer
- `unsigned short` - typically 2 bytes, unsigned integer
- `unsigned int` - typically 4 bytes, unsigned integer
- `unsigned long` - platform-dependent size, unsigned integer

**Note**: For portability, prefer using the stdint.h types (`int8_t`, `uint32_t`, etc.) as they have guaranteed sizes across platforms.

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

1. **Verify configuration**:
   ```bash
   ./test_loader
   ```

2. **Run the GUI**:
   ```bash
   ./telemetry_gui
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

1. Launch the telemetry GUI:
   ```bash
   ./telemetry_gui
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
    SDL_CreateWindow("Telemetry Analyzer", SDL_WINDOWPOS_CENTERED,
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

The application uses two threads:

1. **Network Receiver Thread** - Receives UDP packets, parses data, updates signal registry
2. **Main GUI Thread** - Renders UI, handles user input, reads signal data

**Synchronization**:
- `std::mutex stateMutex` protects the signal registry
- `std::lock_guard` used for automatic lock/unlock
- Network thread holds lock while updating data
- GUI thread holds lock during rendering

**Atomic Variables**:
- `std::atomic<bool> appRunning` for clean shutdown

## Performance Characteristics

### Memory Usage
- Each signal stores 2000 samples by default (configurable)
- ~32 KB per signal (2000 × 2 × 8 bytes for double precision)
- Circular buffer prevents unbounded growth

### CPU Usage
- Near-zero when idle (thanks to `SDL_WaitEventTimeout`)
- ~1-5% during active plotting on modern hardware
- Network thread minimal overhead

### Latency
- Sub-millisecond from packet arrival to display update
- Bounded by vsync (typically 16.67ms at 60 Hz)

## Troubleshooting

### No Data Appearing in Plots

1. Check that `signals.yaml` exists in the same directory as the executable
2. Verify the data source is sending to `127.0.0.1:5000`
3. Check firewall settings (Windows Firewall may block UDP)
4. Look for error messages: "Failed to load signals.yaml"

### Signals Not Appearing in Browser

1. Verify packet structure matches `signals.yaml` exactly
2. Check packet size with `sizeof(YourStruct)`
3. Ensure header string matches configuration
4. Verify field offsets are correct

### Plot Performance Issues

1. Reduce number of signals per plot
2. Decrease sample buffer size in `Signal` constructor
3. Lower data transmission rate from source
4. Check that vsync is enabled

### Windows Firewall Blocking UDP

Allow the application through Windows Firewall:
```powershell
New-NetFirewallRule -DisplayName "Telemetry GUI" -Direction Inbound -Program "C:\path\to\telemetry_gui.exe" -Action Allow
```
