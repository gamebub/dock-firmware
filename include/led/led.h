#pragma once

#include <cstdint>

struct LedColor {
    constexpr LedColor(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    LedColor scale(float by) {
        return LedColor(static_cast<uint8_t>((static_cast<float>(r) * by)),
                        static_cast<uint8_t>((static_cast<float>(g) * by)),
                        static_cast<uint8_t>((static_cast<float>(b) * by)));
    }
};

static constexpr LedColor kColorBlack(0, 0, 0);

enum class LedPattern {
    kOff,
    kSolid,
    kBlink,
    kBreathe,
};

struct LedBehavior {
    LedPattern pattern;
    LedColor color;
    uint32_t period_ms = 0;
    uint32_t repeat = 0;
};

enum class LedState : uint32_t {
    kNone,
    kStandby,
    kBluetoothPairing,
    kDockActive,
    kCount,
};

void InitLed();

/// Enable a led state (reference counted)
void SetLedState(LedState state);

/// Disable a led state (reference counted);
void UnsetLedState(LedState state);
