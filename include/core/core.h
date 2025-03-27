#pragma once

#include <cstddef>
#include <cstdint>

enum class EventType : uint32_t {
    /// A handheld device (maybe) mounted.
    kHandheldMount,
    /// The maybe-handheld unmounted.
    kHandheldUnmount,
    /// Handheld RX data.
    kHandheldRxData,
};

struct Event {
    EventType type;

    union {
        struct {
            size_t len;
            uint8_t data[64];
        } handheld_rx_data;
    };
};

void InitCore();

/// Send an event to the core worker task.
void PostEvent(const Event& event);
