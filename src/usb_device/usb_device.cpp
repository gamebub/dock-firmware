#include "usb_device/usb_device.h"

#include <array>
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
constexpr size_t kBufferCapacity = 2048;

SemaphoreHandle_t mutex;
size_t tx_buffer_count = 0;
size_t tx_buffer_head = 0;
std::array<char, kBufferCapacity> tx_buffer = {};

void stdio_usb_out_chars(const char* buf, int length) {
    xSemaphoreTake(mutex, portMAX_DELAY);

    // Write to the buffer.
    // Current behavior: if it fills up, drop data.
    // TODO use memcpy
    size_t write_len = static_cast<size_t>(length);
    if (write_len > (tx_buffer.size() - tx_buffer_count)) {
        write_len = tx_buffer.size() - tx_buffer_count;
    }

    size_t tx_buffer_tail = (tx_buffer_head + tx_buffer_count) % tx_buffer.size();
    for (size_t i = 0; i < write_len; i++) {
        tx_buffer[tx_buffer_tail++] = buf[i];
        if (tx_buffer_tail >= tx_buffer.size()) {
            tx_buffer_tail = 0;
        }
    }
    tx_buffer_count += write_len;
    xSemaphoreGive(mutex);
}

void stdio_usb_out_flush(void) {
    // Nothing to do, it'll be flushed in the task.
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
    uint64_t connection_time = 0;
    bool is_connected = false;

    while (true) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        tud_task_ext(0, false);

        if (tud_cdc_connected()) {
            if (!is_connected) {
                is_connected = true;
                connection_time = time_us_64();
            }
        } else {
            is_connected = false;
        }

        // Wait for at least 50us after connection to flush
        if (is_connected && time_us_64() > connection_time + 50000) {
            // Try flushing the buffer as much as possible
            // TODO write a while chunk, not byte-by-byte
            while (tx_buffer_count > 0) {
                if (tud_cdc_write_available() > 0) {
                    tud_cdc_write(tx_buffer.data() + tx_buffer_head, 1);
                    tx_buffer_count--;
                    tx_buffer_head++;
                    if (tx_buffer_head >= tx_buffer.size()) {
                        tx_buffer_head = 0;
                    }
                } else {
                    break;
                }
            }
            tud_cdc_write_flush();
        }

        xSemaphoreGive(mutex);
        vTaskDelay(5);
    }
}

void InitUsbDevice() {
    mutex = xSemaphoreCreateMutex();

    tud_cdc_configure_t cdc_config = TUD_CDC_CONFIGURE_DEFAULT();
    cdc_config.tx_overwritabe_if_not_connected = 1;
    cdc_config.tx_persistent = 1;
    cdc_config.rx_persistent = 1;
    tud_cdc_configure(&cdc_config);
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