#include <stdio.h>

#include "FreeRTOS.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "task.h"

//
#include "bluetooth/bluetooth.h"
#include "git_commit.h"
#include "gpio/gpio.h"
#include "led/led.h"
#include "rust_api.h"
#include "usb_device/usb_device.h"
#include "usb_host/usb_host.h"

bi_decl(bi_program_description(GIT_HASH_STRING));

int main()
{
    stdio_init_all();
    InitUsbDevice();

    rust_init();

    InitLed();
    InitUsbHost();
    InitBluetooth();
    InitGpio();

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

extern "C" const uint8_t* GetGitCommitHash()
{
    return GIT_HASH_BYTES;
}
