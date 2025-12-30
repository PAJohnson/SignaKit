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
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.28.5
    )
    FetchContent_MakeAvailable(SDL2)
    set(SDL2_LIBRARIES SDL2::SDL2main SDL2::SDL2)
else()
    find_package(SDL2 REQUIRED)
endif()

if(NOT EMSCRIPTEN)
   find_package(OpenGL REQUIRED)
endif()

include(FetchContent)
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG master
)
FetchContent_MakeAvailable(yaml-cpp)

# 2. ImGui Sources
# Note: We need core files + the specific backends for SDL2 and OpenGL3
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)
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
set(IMPLOT_DIR ${CMAKE_SOURCE_DIR}/implot)
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
# 6. Link Libraries
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_compile_options(telemetry_gui PRIVATE -sUSE_SDL=2)
    target_link_options(telemetry_gui PRIVATE
        -sUSE_SDL=2
        -sUSE_WEBGL2=1
        -sALLOW_MEMORY_GROWTH=1
        --preload-file ${CMAKE_SOURCE_DIR}/signals.yaml@signals.yaml
        --preload-file ${CMAKE_SOURCE_DIR}/imgui/misc/fonts/Roboto-Medium.ttf@fonts/Roboto-Medium.ttf
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

# 7. Test Executable
add_executable(test_loader 
    src/test_loader.cpp 
    src/SignalConfigLoader.cpp
)
target_include_directories(test_loader PRIVATE src)
target_link_libraries(test_loader PRIVATE yaml-cpp)