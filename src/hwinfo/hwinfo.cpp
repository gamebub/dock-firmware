#include "hwinfo/hwinfo.h"

#include <cstddef>

#include "hardware/regs/addressmap.h"

namespace {
uint32_t kPage = 3;
uint32_t kRowHwVersion = 0;
uint32_t kRowSerialNumber = 4;

uint32_t ReadOtpEntry(size_t page, size_t row) {
    return *(uint32_t*)(OTP_DATA_BASE + ((page * 0x40 + row) * 2));
}
}  // namespace

HardwareVersion GetHardwareVersion() {
    uint32_t value = ReadOtpEntry(kPage, kRowHwVersion);
    return HardwareVersion{
        .product = static_cast<uint8_t>((value >> 24) & 0xFF),
        .major = static_cast<uint8_t>((value >> 16) & 0xFF),
        .minor = static_cast<uint8_t>((value >> 8) & 0xFF),
        .variant = static_cast<uint8_t>((value >> 0) & 0xFF),
    };
}

uint32_t GetSerialNumber() {
    return ReadOtpEntry(kPage, kRowSerialNumber);
}