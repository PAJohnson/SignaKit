## Lua Scripting

### ✅ COMPLETED: Tier 1 - Signal Transforms (2026-01-02)
- ✅ Integrated sol2 library for C++/Lua bindings
- ✅ Created LuaScriptManager for script lifecycle management
- ✅ Implemented signal transform system - Lua scripts can create derived signals from existing telemetry
- ✅ Auto-load scripts from `scripts/` directory at startup
- ✅ GUI controls for loading/reloading scripts with hot-reload support
- ✅ Exposed signal access API: `get_signal()`, `get_signal_history()`, `signal_exists()`
- ✅ Transform registration API: `register_transform(name, function)`
- ✅ Created example scripts: vector magnitude, unit conversion, filtering, power calculation
- ✅ Full documentation in `docs/LuaScripting.md` and `scripts/README.md`
- ✅ Error handling and display in Scripts menu

**Status**: Tier 1 implementation complete and production-ready!

See `docs/LuaScripting.md` for complete API reference and examples.

### 🔄 TODO: Tier 2 - Frame Callbacks & Monitoring
- Frame-based callbacks (execute every GUI render frame)
- Event-based alerting when signals meet conditions
- Statistics accumulation and custom logging
- Access to GUI state and user inputs

### 🔄 TODO: Tier 3 - GUI Control Elements
- Dynamic UI element creation from Lua
- Button callbacks with custom actions
- Text input boxes for configuration
- Access to text box contents from Lua callbacks
- Support for "Send Config" style workflows
- Network/serial packet transmission from Lua

### 🔄 TODO: Tier 4 - Timers & Async Operations
- Timer registration (one-shot and cyclic)
- Heartbeat/keep-alive signal support
- Timer callbacks with access to GUI state
- Integration with network transmission for periodic packets