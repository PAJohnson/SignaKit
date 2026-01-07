# Building SignaKit

This document provides instructions for building SignaKit on Windows.

## Prerequisites

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
   pacman -S mingw-w64-ucrt-x86_64-luajit
   ```

## Dependencies

The build system automatically fetches the following dependencies via CMake FetchContent:

- **SDL2** - Cross-platform windowing and input
- **ImGui** (docking branch) - Immediate mode GUI framework
- **ImPlot** - Real-time plotting library for ImGui
- **sol2** - C++ Lua bindings
- **sockpp** - Modern C++ socket library for Lua network I/O
- **pffft** - Optimized FFT library for spectrograms

All dependencies are built statically for maximum portability.

## Building

### Using MSYS2 UCRT64

1. **Clone the repository**

   Open MSYS2 UCRT64 terminal:

   ```bash
   git clone <repository-url>
   cd SignaKit
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
   ./signakit.exe
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

The build system produces two executables:

- **signakit** - Main GUI application for real-time telemetry visualization
- **mock_device** - Mock data generator for testing (sends simulated IMU and GPS data)

### Building Individual Targets

```bash
cmake --build . --target signakit
cmake --build . --target mock_device
```

## Build Configuration

### Static Linking

All executables are statically linked for maximum portability. This means:
- No external DLLs required
- Executables can be copied and run on any Windows machine
- Larger executable size but easier distribution

### Resource Files

The build system automatically copies required resources to the build directory:
- `Roboto-Medium.ttf` - Font file for ImGui
- `scripts/` - Lua scripts directory (parsers, transforms, callbacks)

## Troubleshooting

### Missing DLL Errors

If you see errors about missing DLLs despite static linking, do a clean rebuild:

```bash
rm -rf build
mkdir build
cd build
cmake .. -G "MSYS Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/windows_host_windows_target.cmake
cmake --build .
```

### Font Loading Warnings

If you see font loading warnings, ensure the build process completed successfully. The font file should be automatically copied to the build directory.

### Build Performance

To speed up builds, use multiple cores:

```bash
# MSYS2
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
