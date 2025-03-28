#pragma once

#include <cstddef>
#include <cstdint>

#include "controller/uni_gamepad.h"

enum class EventType : uint32_t {
    /// A handheld device (maybe) mounted.
    kHandheldMount,
    /// The maybe-handheld unmounted.
    kHandheldUnmount,
    /// Handheld RX data.
    kHandheldRxData,

    /// A gamepad has connected.
    kGamepadConnected,
    /// A gamepad has disconnected.
    kGamepadDisconnected,
    /// New gamepad data.
    kGamepadData,
};

struct Event {
    EventType type;

    union {
        struct {
            size_t len;
            uint8_t data[64];
        } handheld_rx_data;

        struct {
            uni_gamepad_t data;
        } gamepad_data;
    };
};

void InitCore();

/// Send an event to the core worker task.
void PostEvent(const Event& event);
