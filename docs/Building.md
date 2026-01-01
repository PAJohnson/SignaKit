# Building Telemetry GUI

This document provides instructions for building the Telemetry GUI on both Linux and Windows platforms.

## Prerequisites

### Linux (Ubuntu/Debian)

Install the following packages:

```bash
sudo apt update
sudo apt install -y build-essential cmake git \
    libsdl2-dev libgl1-mesa-dev
```

### Windows (MSYS2)

1. **Install MSYS2**
   - Download and install MSYS2 from [https://www.msys2.org/](https://www.msys2.org/)
   - Recommended installation path: `C:/Users/<USERNAME>/tools/msys64`

2. **Install Build Tools**

   Open the MSYS2 UCRT64 terminal and run:

   ```bash
   pacman -Syu
   pacman -S mingw-w64-ucrt-x86_64-gcc \
             mingw-w64-ucrt-x86_64-cmake \
             mingw-w64-ucrt-x86_64-make \
             git
   ```

## Dependencies

The build system automatically fetches the following dependencies via CMake FetchContent:

- **SDL2** - Cross-platform windowing and input (fetched on Windows, system package on Linux)
- **ImGui** (docking branch) - Immediate mode GUI framework
- **ImPlot** - Real-time plotting library for ImGui
- **yaml-cpp** - YAML configuration file parser

All dependencies are built statically for maximum portability.

## Building on Linux

1. **Clone the repository**

   ```bash
   git clone <repository-url>
   cd Telemetry_GUI
   ```

2. **Configure CMake**

   ```bash
   mkdir build
   cd build
   cmake ..
   ```

3. **Build**

   ```bash
   make -j$(nproc)
   ```

4. **Run**

   ```bash
   ./telemetry_gui
   ```

## Building on Windows

### Using MSYS2 UCRT64

1. **Clone the repository**

   Open MSYS2 UCRT64 terminal:

   ```bash
   git clone <repository-url>
   cd Telemetry_GUI
   ```

2. **Configure CMake with Toolchain File**

   ```bash
   mkdir build
   cd build
   cmake .. -G "MSYS Makefiles" \
       -DCMAKE_TOOLCHAIN_FILE=../cmake/windows_host_windows_target.cmake
   ```

   The toolchain file automatically detects your Windows username and configures the MSYS2 UCRT64 compiler paths.

3. **Build**

   ```bash
   cmake --build .
   ```

   Alternatively:

   ```bash
   make -j$(nproc)
   ```

4. **Run**

   The executables will be in the `build` directory. You can run them from PowerShell, CMD, or the MSYS2 terminal:

   ```bash
   ./telemetry_gui.exe
   ./mock_device.exe
   ```

### Custom MSYS2 Installation Path

If MSYS2 is installed in a non-standard location, set the `MSYS2_ROOT` environment variable:

```bash
export MSYS2_ROOT="C:/custom/path/to/msys64"
cmake .. -G "MSYS Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/windows_host_windows_target.cmake
```

## Build Targets

The build system produces three executables:

- **telemetry_gui** - Main GUI application for real-time telemetry visualization
- **mock_device** - Mock data generator for testing (sends simulated IMU and GPS data)
- **test_loader** - Unit test for YAML signal configuration loader

### Building Individual Targets

```bash
cmake --build . --target telemetry_gui
cmake --build . --target mock_device
cmake --build . --target test_loader
```

## Build Configuration

### Static Linking (Windows)

All executables are statically linked on Windows for maximum portability. This means:
- No external DLLs required
- Executables can be copied and run on any Windows machine
- Larger executable size but easier distribution

### Resource Files

The build system automatically copies required resources to the build directory:
- `Roboto-Medium.ttf` - Font file for ImGui
- `signals.yaml` - Signal configuration file

## Cross-Compilation

### Linux to Windows (MinGW Cross-Compile)

The repository includes a cross-compilation toolchain file:

```bash
mkdir build-mingw
cd build-mingw
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-w64-x86_64.cmake
make
```

Note: This requires MinGW-w64 cross-compiler installed on your Linux system:

```bash
sudo apt install mingw-w64
```

## Emscripten (WebAssembly) Build

To build for web browsers using Emscripten:

```bash
mkdir build-web
cd build-web
emcmake cmake ..
emmake make
```

This produces `telemetry_gui.html` which can be served via a web server.

## Troubleshooting

### Windows: Missing DLL Errors

If you see errors about missing DLLs despite static linking, do a clean rebuild:

```bash
rm -rf build
mkdir build
cd build
cmake .. -G "MSYS Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/windows_host_windows_target.cmake
cmake --build .
```

### Linux: SDL2 Not Found

If CMake cannot find SDL2:

```bash
sudo apt install libsdl2-dev
```

### Font Loading Warnings

If you see font loading warnings, ensure the build process completed successfully. The font file should be automatically copied to the build directory.

### Build Performance

To speed up builds, use multiple cores:

```bash
# Linux/MSYS2
make -j$(nproc)

# Or with CMake
cmake --build . -j$(nproc)
```

## Clean Build

To perform a clean build:

```bash
rm -rf build
mkdir build
cd build
# ... run cmake and build commands
```
