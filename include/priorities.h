#pragma once

#include <cstdint>

enum class TaskPriority : uint32_t {
    kMin = 1,
    kBluetooth = 2,
    kUsbHost = 3,
};
