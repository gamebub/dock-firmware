#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

void InitUsbHost();

/// Send an OUT control transfer to the handheld (if connected).
void UsbHandheldControlOut(uint8_t request, uint16_t value, uintptr_t tag, std::span<uint8_t> data);

/// Send an IN control transfer to the handheld (if connected).
void UsbHandheldControlIn(uint8_t request, uint16_t value, uintptr_t tag, uint16_t length);