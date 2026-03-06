#pragma once

#include <cstdbool>
#include <cstdint>

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

extern "C" {
void rust_event_button_short(void);
void rust_event_button_long(void);

uint32_t rust_gamepad_allocate_id(void);

void rust_event_gamepad_connected(uint32_t id, uint32_t kind, const uint8_t* device_id_ptr, bool wired);
void rust_event_gamepad_data(uint32_t id, GamepadData data);
void rust_event_gamepad_disconnected(uint32_t id);

void rust_event_handheld_mount(void);
void rust_event_handheld_unmount(void);
void rust_event_handheld_xfer_complete(uint8_t request, bool success, uintptr_t tag, const uint8_t* data, size_t data_len);

void rust_info_get_for_usb(uint8_t* buffer, size_t len);
uint32_t rust_info_serial_number();

void rust_led_set_dfu(void);
}
