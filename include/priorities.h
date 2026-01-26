#pragma once

#include <cstdint>

enum class TaskPriority : uint32_t {
    kMin = 1,
    kCore = 2,
    kLed = 3,
    kBluetooth = 4,
    kUsbHost = 5,
    kUsbDevice = 6,
};
