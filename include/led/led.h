#pragma once

#include <cstdint>

enum class LedBehavior : uint32_t {
    kOff,
    kOn,
    kBlinkSlow,
    kBlinkFast,
    kBreatheSlow,
    kBreatheFast,
};

void InitLed();

/// Set the current indicator LED behavior
void SetLedBehavior(LedBehavior behavior);
