cmake_minimum_required(VERSION 3.15)
project(TelemetryGui)

set(CMAKE_CXX_STANDARD 17)

# Enable Link Time Optimization (LTO/IPO) for smaller binaries
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    message(STATUS "Link-time optimization (LTO) enabled for Release builds")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
else()
    message(STATUS "Link-time optimization (LTO) not supported: ${ipo_error}")
endif()

# Fetch SDL2 for Windows
include(FetchContent)
message(STATUS "Fetching SDL2 for Windows build...")

# Force SDL2 to build as a static library
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.28.5
)
FetchContent_MakeAvailable(SDL2)
set(SDL2_LIBRARIES SDL2::SDL2main SDL2::SDL2-static)

find_package(OpenGL REQUIRED)

# Fetch yaml-cpp
set(YAML_CPP_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG master
)
FetchContent_MakeAvailable(yaml-cpp)

# Fetch ImGui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)
set(IMGUI_DIR ${imgui_SOURCE_DIR})

# Fetch ImPlot
FetchContent_Declare(
  implot
  GIT_REPOSITORY https://github.com/epezent/implot.git
  GIT_TAG master
)
FetchContent_MakeAvailable(implot)
set(IMPLOT_DIR ${implot_SOURCE_DIR})

# Fetch ImGuiFileDialog
FetchContent_Declare(
  ImGuiFileDialog
  GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
  GIT_TAG master
)
FetchContent_GetProperties(ImGuiFileDialog)
if(NOT imguifiledialog_POPULATED)
  FetchContent_Populate(ImGuiFileDialog)
endif()
set(IMGUIFILEDIALOG_DIR ${imguifiledialog_SOURCE_DIR})

# Find LuaJIT (preferred for performance) or fall back to Lua
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LUAJIT luajit)
endif()

if(LUAJIT_FOUND)
    message(STATUS "Using LuaJIT: ${LUAJIT_LIBRARIES}")
    set(LUA_LIBRARIES ${LUAJIT_LIBRARIES})
    set(LUA_INCLUDE_DIR ${LUAJIT_INCLUDE_DIRS})
else()
    # Fall back to standard Lua
    find_package(Lua 5.1 REQUIRED)
    message(STATUS "Using Lua: ${LUA_LIBRARIES}")
    set(LUA_INCLUDE_DIR ${LUA_INCLUDE_DIR})
endif()

message(STATUS "Lua include dir: ${LUA_INCLUDE_DIR}")

# Fetch sol2 (C++ Lua bindings)
# Using develop branch for better compiler compatibility and LuaJIT support
FetchContent_Declare(
  sol2
  GIT_REPOSITORY https://github.com/ThePhD/sol2.git
  GIT_TAG develop
)
FetchContent_MakeAvailable(sol2)

# Fetch sockpp for network I/O in Lua scripts (modern C++ sockets)
message(STATUS "Fetching sockpp for Lua network I/O...")
FetchContent_Declare(
    sockpp
    GIT_REPOSITORY https://github.com/fpagliughi/sockpp.git
    GIT_TAG v0.8.3
)
set(SOCKPP_BUILD_SHARED OFF CACHE BOOL "Build sockpp as static library")
set(SOCKPP_BUILD_STATIC ON CACHE BOOL "Build sockpp as static library")
FetchContent_MakeAvailable(sockpp)

message(STATUS "sockpp configured for static linking")

# ImGui Sources
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# ImPlot Sources
set(IMPLOT_SOURCES
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
    ${IMPLOT_DIR}/implot_demo.cpp
)

# ImGuiFileDialog Sources
set(IMGUIFILEDIALOG_SOURCES
    ${IMGUIFILEDIALOG_DIR}/ImGuiFileDialog.cpp
)

# Create main executable
add_executable(telemetry_gui
    src/main.cpp
    src/SignalConfigLoader.cpp
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
    ${IMGUIFILEDIALOG_SOURCES}
)

# Include directories
target_include_directories(telemetry_gui PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${IMGUIFILEDIALOG_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIR}
    src
)

# Copy resources to build directory
add_custom_command(TARGET telemetry_gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${IMGUI_DIR}/misc/fonts/Roboto-Medium.ttf
        ${CMAKE_CURRENT_BINARY_DIR}/Roboto-Medium.ttf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/signals.yaml
        ${CMAKE_CURRENT_BINARY_DIR}/signals.yaml
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/scripts
        ${CMAKE_CURRENT_BINARY_DIR}/scripts
)

# Link libraries - Windows only
target_link_libraries(telemetry_gui PRIVATE
    mingw32
    ${SDL2_LIBRARIES}
    opengl32
    ws2_32
    wsock32
    imm32
    winmm
    gdi32
    shcore
    yaml-cpp
    sol2::sol2
    ${LUA_LIBRARIES}
    sockpp-static
)

# Static linking for portability
set_target_properties(telemetry_gui PROPERTIES LINK_FLAGS "-static -static-libgcc -static-libstdc++")

# Test executables
add_executable(test_loader
    src/test_loader.cpp
    src/SignalConfigLoader.cpp
)
target_include_directories(test_loader PRIVATE src)
target_link_libraries(test_loader PRIVATE yaml-cpp)

add_executable(struct_size_test src/struct_size_test.cpp)
target_include_directories(struct_size_test PRIVATE src)

add_executable(mock_device src/mock_device.cpp)
target_include_directories(mock_device PRIVATE src)
target_link_libraries(mock_device PRIVATE ws2_32 wsock32)
# Static linking for portability
set_target_properties(mock_device PROPERTIES LINK_FLAGS "-static -static-libgcc -static-libstdc++")
