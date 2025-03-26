#include <stdio.h>

#include "FreeRTOS.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "task.h"

//
#include "bluetooth/bluetooth.h"
#include "usb_host/usb_host.h"

int main() {
    stdio_init_all();

    printf("Dock firmware\n");

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