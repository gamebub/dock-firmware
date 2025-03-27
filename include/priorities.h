#pragma once

#include <cstdint>

enum class TaskPriority : uint32_t {
    kMin = 1,
    kCore = 2,
    kBluetooth = 3,
    kUsbHost = 4,
};
