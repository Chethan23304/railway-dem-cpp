#pragma once
#include "StdTypes.hpp"

// Debounce types
enum class DebounceType {
    Counter,  // increment on PREFAILED, promote after threshold
    Time      // promote after N consecutive ticks
};

// Debounce result returned after each update
enum class DebounceResult {
    NoChange,  // counter incrementing, not yet triggered
    Failed,    // threshold reached -> report FAILED to DEM
    Passed     // counter reset -> report PASSED to DEM
};

// DebounceCfg: configuration for one signal (set once)
struct DebounceCfg {
    DebounceType type      = DebounceType::Counter;
    uint8_t      threshold = 3U;   // misses before FAILED
};

// DebounceState: runtime state for one signal
struct DebounceState {
    uint8_t  counter    = 0U;
    bool     failed     = false;
};

// Debounce: one instance per monitored signal
class Debounce {
public:
    // Create with config
    explicit Debounce(const DebounceCfg& cfg);

    // Call when signal reports PREFAILED (miss detected)
    // Returns Failed when threshold reached
    DebounceResult reportPrefailed();

    // Call when signal reports PASSED (received OK)
    // Returns Passed if previously failed, NoChange otherwise
    DebounceResult reportPassed();

    // Call when signal reports directly FAILED (no debounce needed)
    DebounceResult reportFailed();

    // Get current miss counter value
    uint8_t getCounter() const { return m_state.counter; }

    // Check if currently in failed state
    bool isFailed() const { return m_state.failed; }

    // Reset everything
    void reset();

private:
    DebounceCfg  m_cfg;
    DebounceState m_state;
};
