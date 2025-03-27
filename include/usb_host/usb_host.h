#pragma once

#include <cstddef>
#include <cstdint>

void InitUsbHost();

/// Send data to the handheld (if connected).
/// Note: can only be called from the main task.
void UsbWriteHandheldData(uint8_t* data, size_t len);
