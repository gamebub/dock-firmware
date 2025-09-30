#pragma once

#include <cstddef>
#include <cstdint>

#include "controller/uni_controller_type.h"
#include "controller/uni_gamepad.h"

struct Gamepad {
    uint32_t id;
    uni_controller_type_t gamepad_type;
    char device_id[18];
    bool wired;
};

struct GamepadData {
    // From bit 0:
    // (A B X Y) (Up Down Right Left) (System Select Start Capture(?)) (L1 R1 L2 R2 L3 R3)
    uint32_t buttons;
    int16_t lx;
    int16_t ly;
    uint16_t lz;
    int16_t rx;
    int16_t ry;
    uint16_t rz;
};

enum class EventType : uint32_t {
    /// A handheld device (maybe) mounted.
    kHandheldMount,
    /// The maybe-handheld unmounted.
    kHandheldUnmount,
    /// Handheld RX data.
    kHandheldRxData,
    /// Handheld ping
    kHandheldPing,

    /// A gamepad has connected.
    kGamepadConnected,
    /// A gamepad has disconnected.
    kGamepadDisconnected,
    /// New gamepad data.
    kGamepadData,

    /// The dock button was pressed and released.
    kButtonShortPress,
    /// The dock button was long pressed.
    kButtonLongPress,
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
            GamepadData data;
        } gamepad_data;
    };
};

void InitCore();

/// Send an event to the core worker task.
void PostEvent(const Event& event);

/// Make a new gamepad ID.
/// Safe to call from any thread.
uint32_t AssignGamepadId();
