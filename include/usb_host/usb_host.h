#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

void InitUsbHost();

/// Send an OUT control transfer to the handheld (if connected).
void UsbHandheldControlOut(uint8_t request, uint16_t value, uintptr_t tag, std::span<const uint8_t> data);

extern "C" void UsbHandheldControlOut2(uint8_t request, uint16_t value, uintptr_t tag, const uint8_t* data, size_t data_len);

/// Send an IN control transfer to the handheld (if connected).
extern "C" void UsbHandheldControlIn(uint8_t request, uint16_t value, uintptr_t tag, uint16_t length);