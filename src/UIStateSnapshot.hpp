#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include "plot_types.hpp"

// -------------------------------------------------------------------------
// UI State Snapshot (Read-Only)
// -------------------------------------------------------------------------
// This is a lightweight, read-only snapshot of UI state that Lua threads
// can safely access without locks. The main thread updates this snapshot
// once per frame, and Lua threads read from it.
//
// Design: Use atomic pointer swap to allow lock-free reads
// -------------------------------------------------------------------------

struct UIStateSnapshot {
    // Control element states (compact representation)
    struct ButtonState {
        std::string title;
        bool clicked;
    };

    struct ToggleState {
        std::string title;
        bool state;
    };

    struct TextInputState {
        std::string title;
        std::string text;
    };

    // Snapshot data
    std::vector<ButtonState> buttons;
    std::vector<ToggleState> toggles;
    std::vector<TextInputState> textInputs;

    // Fast lookup maps (built during snapshot creation)
    std::unordered_map<std::string, bool> buttonClickedMap;
    std::unordered_map<std::string, bool> toggleStateMap;
    std::unordered_map<std::string, std::string> textInputMap;

    // Helper functions for fast lookup
    bool getButtonClicked(const std::string& title) const {
        auto it = buttonClickedMap.find(title);
        return (it != buttonClickedMap.end()) ? it->second : false;
    }

    bool getToggleState(const std::string& title) const {
        auto it = toggleStateMap.find(title);

        // Debug: Print map contents on first few calls
        static int debug_count = 0;
        if (debug_count < 5) {
            printf("[UIStateSnapshot] getToggleState('%s') - map has %zu entries:\n", title.c_str(), toggleStateMap.size());
            for (const auto& [key, value] : toggleStateMap) {
                printf("  '%s' = %s\n", key.c_str(), value ? "true" : "false");
            }
            debug_count++;
        }

        return (it != toggleStateMap.end()) ? it->second : false;
    }

    std::string getTextInput(const std::string& title) const {
        auto it = textInputMap.find(title);
        return (it != textInputMap.end()) ? it->second : std::string();
    }

    // Build lookup maps after populating vectors
    void buildMaps() {
        buttonClickedMap.clear();
        toggleStateMap.clear();
        textInputMap.clear();

        for (const auto& btn : buttons) {
            buttonClickedMap[btn.title] = btn.clicked;
        }
        for (const auto& tog : toggles) {
            toggleStateMap[tog.title] = tog.state;
        }
        for (const auto& txt : textInputs) {
            textInputMap[txt.title] = txt.text;
        }
    }
};

// -------------------------------------------------------------------------
// Atomic Snapshot Pointer
// -------------------------------------------------------------------------
// Main thread writes to one snapshot while Lua threads read from another
// Double-buffering pattern with atomic pointer swap
// -------------------------------------------------------------------------
class AtomicSnapshotPtr {
private:
    std::atomic<UIStateSnapshot*> current_snapshot{nullptr};
    UIStateSnapshot snapshot_a;
    UIStateSnapshot snapshot_b;
    UIStateSnapshot* write_target{&snapshot_a};

public:
    AtomicSnapshotPtr() {
        current_snapshot.store(&snapshot_b, std::memory_order_release);
    }

    // Main thread: Get writable snapshot for updates
    UIStateSnapshot* getWriteSnapshot() {
        return write_target;
    }

    // Main thread: Publish the updated snapshot (atomic swap)
    void publish() {
        UIStateSnapshot* old_snapshot = current_snapshot.load(std::memory_order_acquire);
        current_snapshot.store(write_target, std::memory_order_release);
        write_target = old_snapshot;  // Next write goes to old read buffer
    }

    // Lua threads: Get current snapshot for reading (lock-free)
    const UIStateSnapshot* getReadSnapshot() const {
        return current_snapshot.load(std::memory_order_acquire);
    }
};
