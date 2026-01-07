# Future Tasks

## Statically Linking LuaSocket

### Goal
Integrate the `luasocket` library directly into the `signakit` executable via static linking. This solves the distribution problem (single .exe) and resolves the "module not found" or symbol resolution errors common when mixing static LuaJIT with external DLLs on Windows.

### Implementation Plan

#### 1. Build System (`CMakeLists.txt`)
We need to fetch `luasocket` and compile its C sources as a static library, ensuring it links against our Lua includes.

- [ ] Add `FetchContent` block for `luasocket` (using a CMake-friendly fork or defining targets manually).
- [ ] Define a static library target `lua_socket_static` comprised of `luasocket` sources (e.g., `luasocket.c`, `io.c`, `buffer.c`, `tcp.c`, `udp.c`, etc.).
- [ ] Link `lua_socket_static` to `signakit`.

#### 2. Source Code (`src/LuaScriptManager.hpp`)
We must manually initialize the statically linked module so `require("socket")` finds it.

- [ ] Declare the C entry point: `extern "C" int luaopen_socket_core(lua_State *L);`
- [ ] Inside `initializeLuaState()`:
    - Preload the library into Lua's `package.preload` table.

### Verification
- [ ] Create `scripts/test_socket.lua` to verify `require("socket")`.
- [ ] Run app and check logs.

## Interactive Lua Terminal

### Goal
Add a built-in terminal interface to the GUI that allows users to execute Lua commands interactively. This enables realtime debugging, variable inspection, and testing of new `on_packet` callbacks without restarting the application.

### Implementation Plan

#### 1. Core Logic (`src/LuaScriptManager.hpp`)
Modify the script manager to handle console I/O and command execution.

- [ ] Add `consoleOutput` buffer (vector of strings) and mutex for thread safety.
- [ ] Intercept Lua `print` / logging to divert output to both stdout and the console buffer.
- [ ] Implement `executeConsoleCommand(cmd)` to safely run Lua chunks from the UI.
- [ ] Expose API for the UI to read logs and submit commands.

#### 2. UI State (`src/ui_state.hpp`)
Track the visibility of the new window.

- [ ] Add `bool showLuaConsole` to `UIPlotState`.

#### 3. Rendering (`src/console_rendering.hpp` & `src/plot_rendering.hpp`)
Create the visual components for the terminal.

- [ ] Create `src/console_rendering.hpp` with `RenderLuaConsole(...)`.
- [ ] Implement a scrolling log view and an input text field.
- [ ] Add "Clear" and "Auto-scroll" features.
- [ ] Add "Lua Console" menu item in `src/plot_rendering.hpp` to toggle visibility.

#### 4. Integration (`src/main.cpp`)
Connect the new rendering logic to the main loop.

- [ ] Include `console_rendering.hpp`.
- [ ] Call `RenderLuaConsole` in the main loop when enabled.

### Verification
- [ ] Verify `print()` output appears in the console window.
- [ ] Verify executing `x = 10` persists state for subsequent commands.

## Async Lua Architecture (Coroutines + C++ I/O Workers)

### Goal
Refactor the Lua integration to support "pseudo-blocking" I/O using coroutines within a single Lua state. This allows scripts to share data and libraries while preventing I/O operations from stalling the 60fps GUI.

### Implementation Plan

#### 1. C++ Core (LuaUDPSocket & Scheduler)
Refactor `LuaUDPSocket` to handle its own background threading if needed, and add a coroutine scheduler to the `LuaScriptManager`.

- [ ] Add a `std::vector<sol::thread>` (or `sol::coroutine`) to track active "background" tasks in `LuaScriptManager.hpp`.
- [ ] Implement a `scheduler` that iterates through these tasks every frame and calls `resume()`.
- [ ] Add a `spawn(function)` Lua function to start a new coroutine.
- [ ] Wrap `LuaUDPSocket::receive` in a Lua-side helper (or C++ binding) that calls `yield()` if no data is available and the user requested a "blocking" read.

#### 2. main.cpp Integration
- [ ] Ensure `executeFrameCallbacks` also runs the coroutine scheduler.

#### 3. Lua API
Provide the necessary primitives for asynchronous scripting.

- [ ] `yield()`: Passes control back to the GUI.
- [ ] `spawn(func)`: Run a function as a persistent coroutine.
- [ ] `sleep(seconds)`: A non-blocking sleep that yields until time has passed.

#### 4. Lua Data Scripts
Update scripts to use the new asynchronous patterns.

- [ ] Refactor `DataSource.lua` to use a `while true` loop inside a `spawn()`ed coroutine.
- [ ] Change the loop to use a "blocking" `receive()` that internally yields, making the code look synchronous but behave asynchronously.

### Verification
- [ ] Create `scripts/test/async_test.lua` to verify concurrent execution.
- [ ] Verify GUI remains at 60fps while scripts are "blocking".
- [ ] Verify data sharing between scripts remains functional.
