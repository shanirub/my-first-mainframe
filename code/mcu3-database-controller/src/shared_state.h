#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// shared_state.h — MCU #3 inter-task shared state
//
// Owns the SharedState struct definition and extern declarations for the
// global instance and its associated mutex.
//
// Consumers: OLED task (reader), Logic task (writer, Phase 4+)
// Protection: displayMutex — take before read or write, give immediately after.
//
// Rule: never access SharedState fields without holding displayMutex.
// ─────────────────────────────────────────────────────────────────────────────
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// SharedState — live MCU #3 operational data, updated by Logic task.
// Read by OLED task every 500ms under displayMutex.
struct SharedState {
    uint32_t readCount;       // cumulative DB_READ operations handled
    uint32_t writeCount;      // cumulative DB_WRITE operations handled
    char     lastAccount[9];  // last account number seen (8 digits + '\0')
    char     lastError[24];   // last error string, empty if none
};

// Defined in main.cpp — include this header to access from any translation unit.
extern SharedState      gSharedState;
extern SemaphoreHandle_t gDisplayMutex;