#pragma once

#include "core/core.h"
#include <cstdbool>
#include <cstdint>

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
}
