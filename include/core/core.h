#pragma once

#include <cstddef>
#include <cstdint>

#include "controller/uni_controller_type.h"
#include "controller/uni_gamepad.h"

struct Gamepad {
    uint32_t id;
    uni_controller_type_t gamepad_type;
};

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
            Gamepad gamepad;
        } gamepad_connected;

        struct {
            uint32_t gamepad_id;
        } gamepad_disconnected;

        struct {
            uint32_t gamepad_id;
            uni_gamepad_t data;
        } gamepad_data;
    };
};

void InitCore();

/// Send an event to the core worker task.
void PostEvent(const Event& event);

/// Make a new gamepad ID.
/// Safe to call from any thread.
uint32_t AssignGamepadId();
