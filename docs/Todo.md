# Future Tasks

## Statically Linking LuaSocket

### Goal
Integrate the `luasocket` library directly into the `telemetry_gui` executable via static linking. This solves the distribution problem (single .exe) and resolves the "module not found" or symbol resolution errors common when mixing static LuaJIT with external DLLs on Windows.

### Implementation Plan

#### 1. Build System (`CMakeLists.txt`)
We need to fetch `luasocket` and compile its C sources as a static library, ensuring it links against our Lua includes.

- [ ] Add `FetchContent` block for `luasocket` (using a CMake-friendly fork or defining targets manually).
- [ ] Define a static library target `lua_socket_static` comprised of `luasocket` sources (e.g., `luasocket.c`, `io.c`, `buffer.c`, `tcp.c`, `udp.c`, etc.).
- [ ] Link `lua_socket_static` to `telemetry_gui`.

#### 2. Source Code (`src/LuaScriptManager.hpp`)
We must manually initialize the statically linked module so `require("socket")` finds it.

- [ ] Declare the C entry point: `extern "C" int luaopen_socket_core(lua_State *L);`
- [ ] Inside `initializeLuaState()`:
    - Preload the library into Lua's `package.preload` table.

### Verification
- [ ] Create `scripts/test_socket.lua` to verify `require("socket")`.
- [ ] Run app and check logs.
