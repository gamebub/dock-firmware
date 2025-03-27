#pragma once

#include <cstdint>

enum class EventType : uint32_t {
    /// A handheld device (maybe) mounted.
    kHandheldMount,
    /// The maybe-handheld unmounted.
    kHandheldUnmount,
    /// Response to a handheld request.
    kHandheldResponse,
};

struct Event {
    EventType type;
};

void InitCore();

/// Send an event to the core worker task.
void PostEvent(const Event& event);
