#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "controller/uni_controller_type.h"
#include "controller/uni_gamepad.h"

struct Gamepad {
    uint32_t id;
    uni_controller_type_t gamepad_type;
    uint8_t device_id[8];
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
    /// Handheld xfer complete
    kHandheldXferComplete,
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
            uint8_t request;
            bool success;
            uint16_t length;
            uintptr_t tag;
            std::array<uint8_t, 64> data;
        } handheld_xfer;

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
