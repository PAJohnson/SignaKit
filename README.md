# TelemetryGui

TelemetryGui is a high-performance, real-time data visualization tool designed for monitoring telemetry streams. It features a zero-copy packet parsing architecture and a flexible Lua-based scripting environment for data processing and UI customization. It is heavily inspired by the utility of CANalyzer CAPL scripting, but oriented towards being capable of handling any data stream, not just CAN.

A demo program, parser, and dashboard layout are provided, as well as a set of lua script examples to demonstrate the capabilities of the system. See `mock_device` (UDP telemetry packet source), `DataSource.lua` and `fast_binary.lua` for an example of a dynamic and fast packet parser, and `DemoLayout.yaml` for a dashboard that works with those elements.

## Key Features

- **High-Performance Parsing**: Zero-copy packet parsing using LuaJIT FFI for O(1) signal updates.
- **Dynamic Visualization**: Real-time plotting and data display using Dear ImGui and ImPlot.
- **Extensible Scripting**: Full control over data ingestion, transformation, and UI elements via Lua.
- **Static Portability**: Built as a standalone executable with static linking for easy deployment on Windows.

## Real-time Visualization

The system utilizes **ImPlot** to provide high-performance, real-time visualization of telemetry data. All plots are updated at the GUI frame rate (typically 60 FPS) and support drag-and-drop signal assignment.

- **Time-based Plots**: Standard line plots with automatic scrolling and history management.
- **X/Y Plots**: Correlation plots for visualizing one signal against another, featuring a "trail" effect to show recent history.
- **FFT & Spectrograms**: Real-time frequency analysis and waterfall displays for vibration or signal processing tasks.
- **Histograms**: Live statistical distributions to monitor signal noise or value ranges.
- **Readout Boxes**: Digital displays for precise, instantaneous value monitoring.

## Live Data Transforms

The integrated Lua environment enables complex data manipulation directly on the incoming telemetry stream before it reaches the UI.

- **Derived Signals**: Create new telemetry channels by performing math on existing ones (e.g., computing 3D magnitude from raw X, Y, Z components).
- **Signal Filtering**: Implement filters (EMA, Butterworth, etc.) or signal conditioning in Lua that runs in the hot-path of every packet.
- **Stateful Processing**: Scripts maintain state between packets, allowing for integrators, accumulators, or complex state machine logic.
- **High-Speed FFI**: For extreme performance, LuaJIT FFI is used to map binary packet data directly to Lua tables or C structs, enabling O(1) data transforms.

## Getting Started

### Prerequisites

The project is designed to be built on Windows using CMake and a C++17 compliant compiler (e.g., MinGW-w64 or MSVC).

### Building

For detailed build instructions, including dependency management and CMake configuration, see [Building.md](docs/Building.md).

### Quick Start

To get started with writing your first telemetry parser or UI script, refer to the [Lua Quick Start Guide](docs/LuaQuickStart.md).

## Documentation

Comprehensive documentation is available in the `docs` directory:

### Core Concepts
- [Program Overview](docs/Program.md): High-level architecture and execution model.
- [Lua Scripting](docs/LuaScripting.md): Overview of the Lua integration and API.
- [Packet Parsing](docs/LuaPacketParsing.md): Technical details on how the demo packet parser works and how to create your own.

### API Reference
- [Control Elements](docs/LuaControlElements.md): Creating buttons, sliders, and other UI widgets.
- [Frame Callbacks](docs/LuaFrameCallbacks.md): Handling logic and rendering on every frame.
- [Cleanup Callbacks](docs/LuaCleanupCallbacks.md): Managing resource lifecycle and script termination.

### Development
- [Building and Installation](docs/Building.md): Technical guide for developers.
- [Todo / Future Work](docs/Todo.md): Current roadmap and planned improvements.

## License

This project is dual-use and is licensed under the MIT License. See the [LICENSE](LICENSE) file for the full license text and third-party software acknowledgments.
