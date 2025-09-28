#include "usb_device/usb_device.h"

#include <cstdint>

#include "FreeRTOS.h"
#include "pico/stdio.h"
#include "pico/stdio/driver.h"
#include "pico/time.h"
#include "priorities.h"
#include "task.h"
#include "tusb.h"

namespace {
constexpr size_t kStackSize = 8 * 1024;
constexpr size_t kStdoutTimeoutUs = 500000;

SemaphoreHandle_t mutex;

bool stdio_usb_connected(void) {
    return tud_cdc_connected();
}

void stdio_usb_out_chars(const char* buf, int length) {
    static uint64_t last_avail_time;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (stdio_usb_connected()) {
        for (int i = 0; i < length;) {
            int n = length - i;
            int avail = (int)tud_cdc_write_available();
            if (n > avail) n = avail;
            if (n) {
                int n2 = (int)tud_cdc_write(buf + i, (uint32_t)n);
                tud_task_ext(0, false);
                tud_cdc_write_flush();
                i += n2;
                last_avail_time = time_us_64();
            } else {
                tud_task_ext(0, false);
                tud_cdc_write_flush();
                if (!stdio_usb_connected() ||
                    (!tud_cdc_write_available() && time_us_64() > last_avail_time + kStdoutTimeoutUs)) {
                    break;
                }
            }
        }
    } else {
        last_avail_time = 0;
    }
    xSemaphoreGive(mutex);
}

void stdio_usb_out_flush(void) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    do {
        tud_task_ext(0, false);
    } while (tud_cdc_write_flush());
    xSemaphoreGive(mutex);
}

int stdio_usb_in_chars(char* buf, int length) {
    (void)buf;
    (void)length;
    return 0;
}

stdio_driver_t stdio_usb = {
    .out_chars = stdio_usb_out_chars,
    .out_flush = stdio_usb_out_flush,
    .in_chars = stdio_usb_in_chars,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = PICO_STDIO_DEFAULT_CRLF,
#endif
};

}  // namespace

void usb_device_task(void*) {
    while (true) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        tud_task_ext(0, false);
        tud_cdc_write_flush();
        xSemaphoreGive(mutex);
        vTaskDelay(5);
    }
}

void InitUsbDevice() {
    mutex = xSemaphoreCreateMutex();
    tud_init(0);

    stdio_set_driver_enabled(&stdio_usb, true);

    // Create USB task, pin it to core 0.
    TaskHandle_t task_handle;
    xTaskCreate(usb_device_task, "usbd", kStackSize, NULL, static_cast<UBaseType_t>(TaskPriority::kUsbDevice),
                &task_handle);
    vTaskCoreAffinitySet(task_handle, 1 << 0);
}

void tud_cdc_rx_cb(uint8_t itf) {
    (void)itf;

    char buf[64];
    uint32_t count = tud_cdc_read(buf, sizeof(buf));
    (void)count;
}