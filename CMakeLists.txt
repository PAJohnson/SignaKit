cmake_minimum_required(VERSION 3.10)
project(TelemetryGui)

set(CMAKE_CXX_STANDARD 17)

# Enable Link Time Optimization (LTO/IPO) for smaller binaries
# Only enable for Release builds to avoid debug build issues
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

# 1. Find System Packages
if(MINGW OR WIN32)
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
else()
    find_package(SDL2 REQUIRED)
endif()

find_package(OpenGL REQUIRED)

include(FetchContent)

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

# 2. ImGui Sources
# Note: We need core files + the specific backends for SDL2 and OpenGL3
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# 3. ImPlot Sources
set(IMPLOT_SOURCES
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
    ${IMPLOT_DIR}/implot_demo.cpp
)

# 4. ImGuiFileDialog Sources
set(IMGUIFILEDIALOG_SOURCES
    ${IMGUIFILEDIALOG_DIR}/ImGuiFileDialog.cpp
)

# 5. Create Executable
add_executable(telemetry_gui
    src/main.cpp
    src/SignalConfigLoader.cpp
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
    ${IMGUIFILEDIALOG_SOURCES}
)

# 6. Include Directories
target_include_directories(telemetry_gui PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${IMGUIFILEDIALOG_DIR}
    ${SDL2_INCLUDE_DIRS}
    src
)

# 6. Copy resources to build directory
add_custom_command(TARGET telemetry_gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${IMGUI_DIR}/misc/fonts/Roboto-Medium.ttf
        ${CMAKE_CURRENT_BINARY_DIR}/Roboto-Medium.ttf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/signals.yaml
        ${CMAKE_CURRENT_BINARY_DIR}/signals.yaml
)

# 7. Link Libraries
if(MINGW OR WIN32)
    # Windows / MinGW
    target_link_libraries(telemetry_gui PRIVATE
        mingw32
        ${SDL2_LIBRARIES}
        opengl32
        ws2_32
        wsock32
        imm32
        winmm
        gdi32
        yaml-cpp
    )
    # Static linking for portability
    set_target_properties(telemetry_gui PROPERTIES LINK_FLAGS "-static -static-libgcc -static-libstdc++")
else()
    # Linux / Native
    target_link_libraries(telemetry_gui PRIVATE
        ${SDL2_LIBRARIES}
        ${OPENGL_LIBRARIES}
        yaml-cpp
        dl
    )
endif()

# 8. Test Executables
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
if(MINGW OR WIN32)
    target_link_libraries(mock_device PRIVATE ws2_32 wsock32)
    # Static linking for portability
    set_target_properties(mock_device PROPERTIES LINK_FLAGS "-static -static-libgcc -static-libstdc++")
endif()