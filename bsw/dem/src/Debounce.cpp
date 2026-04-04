#include "Debounce.hpp"
#include <cstdio>

Debounce::Debounce(const DebounceCfg& cfg) : m_cfg(cfg) {
    m_state = DebounceState{};
}

DebounceResult Debounce::reportPrefailed() {
    if (m_state.failed) return DebounceResult::NoChange;
    m_state.counter++;
    printf("[Debounce] PREFAILED counter=%d/%d\n",
           m_state.counter, m_cfg.threshold);
    if (m_state.counter >= m_cfg.threshold) {
        m_state.failed  = true;
        m_state.counter = m_cfg.threshold;
        printf("[Debounce] -> FAILED (threshold reached)\n");
        return DebounceResult::Failed;
    }
    return DebounceResult::NoChange;
}

DebounceResult Debounce::reportPassed() {
    bool wasFailed = m_state.failed;
    m_state.counter = 0;
    m_state.failed  = false;
    if (wasFailed) {
        printf("[Debounce] -> PASSED (recovered)\n");
        return DebounceResult::Passed;
    }
    return DebounceResult::NoChange;
}

DebounceResult Debounce::reportFailed() {
    m_state.failed  = true;
    m_state.counter = m_cfg.threshold;
    printf("[Debounce] -> FAILED (direct)\n");
    return DebounceResult::Failed;
}

void Debounce::reset() {
    m_state = DebounceState{};
    printf("[Debounce] Reset\n");
}
