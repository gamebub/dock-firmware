#include <stdio.h>

#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

//
#include "bluetooth/bluetooth.h"
#include "usb_host/usb_host.h"

struct uni_platform* get_my_platform(void);

static uint32_t core1_stack[256];
void core1_main(void);

int main() {
    stdio_init_all();
    if (cyw43_arch_init()) {
        printf("Failed to initialize CYW43\n");
        return -1;
    }

    printf("Dock firmware\n");

    sleep_ms(10);
    multicore_reset_core1();
    multicore_launch_core1_with_stack(core1_main, core1_stack, sizeof(core1_stack));

    // Turn on the LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    // Start bluetooth on this core. Does not return.
    StartBluetooth();

    // Unreachable
    return 0;
}

void core1_main() {
    sleep_ms(10);
    printf("core1_main\n");

    StartUsbHost();
}
