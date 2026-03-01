#include <stdio.h>

#include "FreeRTOS.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "task.h"

//
#include "bluetooth/bluetooth.h"
#include "core/core.h"
#include "git_commit.h"
#include "gpio/gpio.h"
#include "hwinfo/hwinfo.h"
#include "led/led.h"
#include "usb_device/usb_device.h"
#include "usb_host/usb_host.h"

bi_decl(bi_program_description(GIT_HASH_STRING));

int main() {
    stdio_init_all();
    InitUsbDevice();

    printf("Game Bub Dock\n");
    auto hw_version = GetHardwareVersion();
    printf("hardware version: %u.%u.%u.%u\n", hw_version.product, hw_version.major, hw_version.minor,
           hw_version.variant);
    printf("firmware version: %u.%u.%u\n", DOCK_FW_VERSION_MAJOR, DOCK_FW_VERSION_MINOR, DOCK_FW_VERSION_PATCH);
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
