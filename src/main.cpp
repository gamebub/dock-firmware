#include <stdio.h>

#include "FreeRTOS.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "task.h"

//
#include "bluetooth/bluetooth.h"
#include "core/core.h"
#include "gpio/gpio.h"
#include "hwinfo/hwinfo.h"
#include "led/led.h"
#include "usb_device/usb_device.h"
#include "usb_host/usb_host.h"

static inline void put_pixel(PIO pio, uint sm, uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

int main() {
    stdio_init_all();
    InitUsbDevice();

    printf("Game Bub Dock\n");
    auto hw_version = GetHardwareVersion();
    printf("hardware version: %u.%u.%u.%u\n", hw_version.product, hw_version.major, hw_version.minor,
           hw_version.variant);
    printf("software version: %s\n", DOCK_SW_VERSION);
    printf("serial number: %08lX\n", GetSerialNumber());

    InitLed();
    InitCore();
    InitUsbHost();
    InitBluetooth();
    InitGpio();

    SetLedState(LedState::kStandby);

    // Start FreeRTOS (doesn't return).
    vTaskStartScheduler();
    return 0;
}

void vApplicationStackOverflowHook(TaskHandle_t /* task */, char* pcTaskName) {
    panic("Stack overflow in task %s\n", *pcTaskName);
}

void vApplicationMallocFailedHook(void) {
    panic("Malloc failed\n");
}