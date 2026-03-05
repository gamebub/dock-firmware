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
#include "git_commit.h"
#include "gpio/gpio.h"
#include "led/led.h"
#include "priorities.h"
#include "usb_device/usb_device.h"
#include "usb_host/usb_host.h"

bi_decl(bi_program_description(GIT_HASH_STRING));

extern "C" {
void rust_init(void);
}

int main()
{
    stdio_init_all();
    InitUsbDevice();

    rust_init();

    printf("firmware version: %u.%u.%u\n", DOCK_FW_VERSION_MAJOR, DOCK_FW_VERSION_MINOR, DOCK_FW_VERSION_PATCH);

    InitLed();
    InitUsbHost();
    InitBluetooth();
    InitGpio();

    SetLedState(LedState::kStandby);

    // Start FreeRTOS (doesn't return).
    vTaskStartScheduler();
    return 0;
}

void vApplicationStackOverflowHook(TaskHandle_t /* task */, char* pcTaskName)
{
    panic("Stack overflow in task %s\n", *pcTaskName);
}

void vApplicationMallocFailedHook(void)
{
    panic("Malloc failed\n");
}
