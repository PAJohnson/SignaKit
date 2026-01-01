cmake_minimum_required(VERSION 3.10)
project(TelemetryGui)

set(CMAKE_CXX_STANDARD 17)

# 1. Find System Packages
# 1. Find System Packages
if(EMSCRIPTEN)
    # Built-in support
elseif(MINGW OR WIN32)
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

if(NOT EMSCRIPTEN)
   find_package(OpenGL REQUIRED)
endif()

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

# 4. Create Executable
add_executable(telemetry_gui 
    src/main.cpp 
    src/SignalConfigLoader.cpp
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
)

# 5. Include Directories
target_include_directories(telemetry_gui PRIVATE 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${SDL2_INCLUDE_DIRS}
    src
)

# 6. Link Libraries
# 6. Copy resources to build directory for non-Emscripten builds
if(NOT EMSCRIPTEN)
    add_custom_command(TARGET telemetry_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${IMGUI_DIR}/misc/fonts/Roboto-Medium.ttf
            ${CMAKE_CURRENT_BINARY_DIR}/Roboto-Medium.ttf
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/signals.yaml
            ${CMAKE_CURRENT_BINARY_DIR}/signals.yaml
    )
endif()

# 7. Link Libraries
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_compile_options(telemetry_gui PRIVATE -sUSE_SDL=2)
    target_link_options(telemetry_gui PRIVATE
        -sUSE_SDL=2
        -sUSE_WEBGL2=1
        -sALLOW_MEMORY_GROWTH=1
        --preload-file ${CMAKE_SOURCE_DIR}/signals.yaml@signals.yaml
        --preload-file ${IMGUI_DIR}/misc/fonts/Roboto-Medium.ttf@fonts/Roboto-Medium.ttf
    )
    target_link_libraries(telemetry_gui PRIVATE yaml-cpp)
else()
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