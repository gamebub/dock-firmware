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

struct FirmwareVersion {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;

    uint32_t value() {
        return (static_cast<uint32_t>(major) << 24) | (static_cast<uint32_t>(minor) << 16) |
               (static_cast<uint32_t>(patch) << 8);
    }
};

HardwareVersion GetHardwareVersion();

extern "C" uint32_t GetSerialNumber();

FirmwareVersion GetFirmwareVersion();
