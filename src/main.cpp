#include <stdio.h>

#include "FreeRTOS.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "task.h"

//
#include "bluetooth/bluetooth.h"
#include "core/core.h"
#include "usb_host/usb_host.h"

int main() {
    stdio_init_all();

    printf("Game Bub Dock\n");
    printf("hardware version: %s\n", DOCK_HW_VERSION);
    printf("software version: %s\n", DOCK_SW_VERSION);
    printf("serial number: %s\n", DOCK_SERIAL_NUM);

    InitCore();
    InitUsbHost();
    InitBluetooth();

    // Start FreeRTOS (doesn't return).
    vTaskStartScheduler();
    return 0;
}

void vApplicationStackOverflowHook(TaskHandle_t Task, char *pcTaskName) {
    panic("Stack overflow in task %s\n", *pcTaskName);
}

void vApplicationMallocFailedHook(void) {
    panic("Malloc failed\n");
}