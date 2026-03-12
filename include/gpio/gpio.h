#pragma once

void InitGpio();

extern "C" void GpioSetHdmiActive(bool active);
extern "C" bool GpioPollButton();
