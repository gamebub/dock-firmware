#pragma once

#include <cstdint>

void InitLed();
extern "C" void LedSetColor(uint8_t r, uint8_t g, uint8_t b);