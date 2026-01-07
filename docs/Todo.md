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
- [ ] Verify error messages are displayed correctly for invalid syntax.
