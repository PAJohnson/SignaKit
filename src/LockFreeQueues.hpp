#pragma once

#include <atomic>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>

// -------------------------------------------------------------------------
// Lock-Free SPSC (Single Producer Single Consumer) Queue
// Based on classic ring buffer with atomic head/tail pointers
// -------------------------------------------------------------------------

template<typename T, size_t SIZE>
class SPSCQueue {
private:
    struct alignas(64) Node {
        T data;
        std::atomic<bool> valid{false};
    };

    std::vector<Node> buffer;
    alignas(64) std::atomic<size_t> head{0};  // Consumer reads from head
    alignas(64) std::atomic<size_t> tail{0};  // Producer writes to tail

public:
    SPSCQueue() : buffer(SIZE) {}

    // Producer: Try to push an item (returns false if queue is full)
    bool push(const T& item) {
        size_t current_tail = tail.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) % SIZE;

        // Check if queue is full
        if (next_tail == head.load(std::memory_order_acquire)) {
            return false;
        }

        buffer[current_tail].data = item;
        buffer[current_tail].valid.store(true, std::memory_order_release);
        tail.store(next_tail, std::memory_order_release);
        return true;
    }

    // Consumer: Try to pop an item (returns false if queue is empty)
    bool pop(T& item) {
        size_t current_head = head.load(std::memory_order_relaxed);

        // Check if queue is empty
        if (current_head == tail.load(std::memory_order_acquire)) {
            return false;
        }

        if (!buffer[current_head].valid.load(std::memory_order_acquire)) {
            return false;
        }

        item = buffer[current_head].data;
        buffer[current_head].valid.store(false, std::memory_order_release);
        head.store((current_head + 1) % SIZE, std::memory_order_release);
        return true;
    }

    // Check if queue is empty (approximate)
    bool empty() const {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }

    // Get approximate size
    size_t size() const {
        size_t h = head.load(std::memory_order_acquire);
        size_t t = tail.load(std::memory_order_acquire);
        if (t >= h) {
            return t - h;
        } else {
            return SIZE - h + t;
        }
    }
};

// -------------------------------------------------------------------------
// Signal Update Message
// -------------------------------------------------------------------------
struct SignalUpdate {
    int signal_id;       // Pre-cached signal ID for O(1) lookup
    double timestamp;
    double value;

    SignalUpdate() : signal_id(-1), timestamp(0.0), value(0.0) {}
    SignalUpdate(int id, double t, double v) : signal_id(id), timestamp(t), value(v) {}
};

// -------------------------------------------------------------------------
// UI Event Message
// -------------------------------------------------------------------------
enum class UIEventType : uint8_t {
    SET_TOGGLE_STATE,
    SET_TEXT_INPUT,
    // Add more as needed
};

struct UIEvent {
    UIEventType type;
    char title[64];      // Fixed-size for lock-free compatibility
    bool boolValue;      // For toggle state
    char textValue[256]; // For text input

    UIEvent() : type(UIEventType::SET_TOGGLE_STATE), boolValue(false) {
        title[0] = '\0';
        textValue[0] = '\0';
    }
};

// -------------------------------------------------------------------------
// Signal Queue (for Lua thread → Main thread communication)
// Large queue size to handle bursts
// -------------------------------------------------------------------------
constexpr size_t SIGNAL_QUEUE_SIZE = 65536;
using SignalQueue = SPSCQueue<SignalUpdate, SIGNAL_QUEUE_SIZE>;

// -------------------------------------------------------------------------
// Event Queue (for Main thread → Lua thread communication)
// Smaller queue since UI events are less frequent
// -------------------------------------------------------------------------
constexpr size_t EVENT_QUEUE_SIZE = 1024;
using EventQueue = SPSCQueue<UIEvent, EVENT_QUEUE_SIZE>;

// -------------------------------------------------------------------------
// Signal ID Registry (Thread-Safe)
// Maps signal names to integer IDs for fast queue operations
// -------------------------------------------------------------------------
class SignalIDRegistry {
private:
    std::vector<std::string> id_to_name;
    std::unordered_map<std::string, int> name_to_id;
    mutable std::mutex mutex;

public:
    // Get or create a signal ID (thread-safe)
    int get_or_create(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex);

        auto it = name_to_id.find(name);
        if (it != name_to_id.end()) {
            return it->second;
        }

        int new_id = static_cast<int>(id_to_name.size());
        id_to_name.push_back(name);
        name_to_id[name] = new_id;
        return new_id;
    }

    // Get signal name by ID (thread-safe, for main thread)
    std::string get_name(int id) const {
        std::lock_guard<std::mutex> lock(mutex);
        if (id >= 0 && id < static_cast<int>(id_to_name.size())) {
            return id_to_name[id];
        }
        return "";
    }

    // Check if ID exists
    bool has_id(int id) const {
        std::lock_guard<std::mutex> lock(mutex);
        return id >= 0 && id < static_cast<int>(id_to_name.size());
    }

    // Clear all mappings
    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        id_to_name.clear();
        name_to_id.clear();
    }
};
