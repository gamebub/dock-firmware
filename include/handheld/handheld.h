#pragma once

#include <cstdbool>
#include <cstdint>

class Handheld {
   public:
    /// Port number (within the hub) of the main plug
    constexpr static uint8_t kHubPort = 3;

    /// Check whether the USB VID and PID corresponds to a device.
    static bool CheckUsbId(uint16_t vid, uint16_t pid) { return vid == 0x1209 && pid == 0xB010; }
};
