#pragma once

#include <cstdint>

enum class TaskPriority : uint32_t {
    kMin = 1,
    kUsbDevice = 2,
    kCore = 3,
    kLed = 4,
    kBluetooth = 5,
    kUsbHost = 6,
};
