#pragma once

#include <cstdint>

struct HardwareVersion {
    uint8_t product;
    uint8_t major;
    uint8_t minor;
    uint8_t variant;

    uint32_t value() {
        return (static_cast<uint32_t>(product) << 24) | (static_cast<uint32_t>(major) << 16) |
               (static_cast<uint32_t>(minor) << 8) | (static_cast<uint32_t>(variant) << 0);
    }
};

HardwareVersion GetHardwareVersion();

uint32_t GetSerialNumber();
