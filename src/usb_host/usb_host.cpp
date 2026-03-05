#include "usb_host/usb_host.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "FreeRTOS.h"
#include "handheld/handheld.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "host/hcd.h"
#include "log/log.h"
#include "pico/time.h"
#include "pio_usb.h"
#include "priorities.h"
#include "queue.h"
#include "rust_api.h"
#include "task.h"
#include "tusb.h"
#include "xinput_host.h"

namespace {
constexpr size_t kStackSize = 8 * 1024;
constexpr size_t kRxBufferMaxLen = 128;
constexpr size_t kXferQueueLen = 16;
constexpr size_t kXferBufferLen = 64;

/// USB state for a handheld device
struct HandheldDeviceState {
    uint8_t address;
    uint8_t cdc_index;

    uint8_t rx_buffer[kRxBufferMaxLen];
    size_t rx_buffer_len = 0;
};

static std::optional<HandheldDeviceState> handheld_device {};

struct ControlXfer {
    uintptr_t tag;
    tusb_control_request_t setup;
    std::array<uint8_t, kXferBufferLen> buffer;
};

QueueHandle_t control_xfer_queue = nullptr;
std::array<uint8_t, kXferBufferLen> control_xfer_buffer;

} // namespace

static void control_xfer_complete_cb(tuh_xfer_t* xfer);

void usb_host_task(void*)
{
    // Initialize USB host stack
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = PIN_USB_DP;
    pio_cfg.pinout = PIO_USB_PINOUT_DPDM;
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    tuh_init(BOARD_TUH_RHPORT);

    while (1) {
        // TODO: figure out the best way to interleave device acceses and tuh_task without busy-looping.
        tuh_task_ext(1, false);

        // Send transfers.
        ControlXfer event {};
        while (true) {
            auto result = xQueuePeek(control_xfer_queue, &event, 0);
            if (result != pdPASS) {
                break;
            }
            if (handheld_device.has_value()) {
                // TODO: only copy if OUT
                memcpy(control_xfer_buffer.data(), event.buffer.data(), event.setup.wLength);
                tuh_xfer_t xfer = {
                    .daddr = handheld_device->address,
                    .ep_addr = 0,
                    .setup = &event.setup,
                    .buffer = control_xfer_buffer.data(),
                    .complete_cb = control_xfer_complete_cb,
                    .user_data = event.tag,
                };
                bool result = tuh_control_xfer(&xfer);

                // Remove it from the queue.
                // TODO: maybe only remove if it's a non-critical transfer?
                xQueueReceive(control_xfer_queue, nullptr, 0);

                if (!result) {
                    rust_event_handheld_xfer_complete(
                        event.setup.bRequest,
                        false,
                        event.tag,
                        (const uint8_t*)4,
                        0);
                }
            } else {
                // No handheld, clear the queue.
                xQueueReset(control_xfer_queue);
            }
        }
    }
}

void InitUsbHost()
{
    // Output 12 MHz clock to USB IC
    gpio_set_function(PIN_USB_CLK_OUT, GPIO_FUNC_GPCK);
    gpio_set_dir(PIN_USB_CLK_OUT, true);
    // Crystal is 12 MHz, so divide by 1.
    clock_gpio_init(PIN_USB_CLK_OUT, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, 1);

    // Take USB Hub chip out of reset
    gpio_init(PIN_USB_RESET_N);
    gpio_set_dir(PIN_USB_RESET_N, GPIO_OUT);
    gpio_put(PIN_USB_RESET_N, 1);

    // Enable VBUS
    gpio_init(PIN_USB_VBUS_EN);
    gpio_set_dir(PIN_USB_VBUS_EN, GPIO_OUT);
    gpio_put(PIN_USB_VBUS_EN, 1);

    // Create USB task, pin it to core 1.
    TaskHandle_t task_handle;
    xTaskCreate(usb_host_task, "usbh", kStackSize, NULL, static_cast<UBaseType_t>(TaskPriority::kUsbHost),
        &task_handle);
    vTaskCoreAffinitySet(task_handle, 1 << 1);

    control_xfer_queue = xQueueCreate(kXferQueueLen, sizeof(ControlXfer));
}

/// Return the port that the device is plugged into, or 0 if not plugged into the root hub.
static uint8_t GetDeviceHubPort(uint8_t device_address)
{
    hcd_devtree_info_t devtree_info {};
    hcd_devtree_get_info(device_address, &devtree_info);
    uint8_t port = devtree_info.hub_port;

    // Now, see if the hub that the device is part of is itself part of the root hub.
    hcd_devtree_get_info(devtree_info.hub_addr, &devtree_info);
    if (devtree_info.hub_addr != 0) {
        return 0;
    }
    return port;
}

void UsbHandheldControlOut2(uint8_t request, uint16_t value, uintptr_t tag, const uint8_t* data, size_t data_len)
{
    UsbHandheldControlOut(request, value, tag, std::span(data, data_len));
}

void UsbHandheldControlOut(uint8_t request, uint16_t value, uintptr_t tag, std::span<const uint8_t> data)
{
    if (data.size() > kXferBufferLen) {
        log_error("Control Out too large");
        return;
    }

    ControlXfer xfer = {};
    xfer.tag = tag;
    xfer.setup = {
        .bmRequestType_bit = {
            .recipient = TUSB_REQ_RCPT_INTERFACE,
            .type = TUSB_REQ_TYPE_VENDOR,
            .direction = TUSB_DIR_OUT,
        },
        .bRequest = request,
        .wValue = value,
        .wIndex = 0, // TODO: read descriptor to find correct interface index
        .wLength = (uint16_t)data.size(),
    };
    memcpy(xfer.buffer.data(), data.data(), data.size());

    auto result = xQueueSendToBack(control_xfer_queue, &xfer, /* xTicksToWait= */ 0);
    if (result != pdPASS) {
        log_error("Failed to post Control Out");
    }
}

void UsbHandheldControlIn(uint8_t request, uint16_t value, uintptr_t tag, uint16_t length)
{
    ControlXfer xfer = {};
    xfer.tag = tag;
    xfer.setup = {
        .bmRequestType_bit = {
            .recipient = TUSB_REQ_RCPT_INTERFACE,
            .type = TUSB_REQ_TYPE_VENDOR,
            .direction = TUSB_DIR_IN,
        },
        .bRequest = request,
        .wValue = value,
        .wIndex = 0,
        .wLength = length,
    };
    auto result = xQueueSendToBack(control_xfer_queue, &xfer, /* xTicksToWait= */ 0);
    if (result != pdPASS) {
        log_error("Failed to post Control In");
    }
}

void tuh_mount_cb(uint8_t dev_addr)
{
    uint16_t vid = 0;
    uint16_t pid = 0;
    tuh_vid_pid_get(dev_addr, &vid, &pid);
    uint8_t port = GetDeviceHubPort(dev_addr);
    printf("Mounted USB device: port=%d addr=%d [%04X:%04X]\n", port, dev_addr, vid, pid);
}

void tuh_umount_cb(uint8_t dev_addr)
{
    printf("Unmounted USB device address=%d\n", dev_addr);
}

void tuh_cdc_rx_cb(uint8_t idx)
{
    // Ignore non-handheld data.
    if (!handheld_device.has_value() || handheld_device->cdc_index != idx) {
        return;
    }
    auto& state = *handheld_device;

    uint8_t buffer[64];
    uint32_t available = tuh_cdc_read(idx, &buffer, sizeof(buffer));

    for (uint32_t i = 0; i < available; i++) {
        uint8_t data = buffer[i];

        // End of line, print it.
        if (data == '\n' && state.rx_buffer_len > 0) {
            printf("| %.*s\n", state.rx_buffer_len, state.rx_buffer);
            state.rx_buffer_len = 0;
            continue;
        }

        if (state.rx_buffer_len < kRxBufferMaxLen) {
            state.rx_buffer[state.rx_buffer_len] = data;
            state.rx_buffer_len++;
        }
    }
}

void tuh_cdc_mount_cb(uint8_t idx)
{
    tuh_itf_info_t itf_info {};
    tuh_cdc_itf_get_info(idx, &itf_info);

    printf("USB CDC mounted: address=%u itf_num=%u\n", itf_info.daddr, itf_info.desc.bInterfaceNumber);

    // Determine whether this is a handheld
    if (GetDeviceHubPort(itf_info.daddr) != Handheld::kHubPort) {
        // Not plugged into the main port.
        return;
    }
    uint16_t vid = 0;
    uint16_t pid = 0;
    if (!tuh_vid_pid_get(itf_info.daddr, &vid, &pid)) {
        return;
    }
    if (!Handheld::CheckUsbId(vid, pid)) {
        return;
    }

    handheld_device.emplace();
    handheld_device->address = itf_info.daddr;
    handheld_device->cdc_index = idx;
    rust_event_handheld_mount();
}

void tuh_cdc_umount_cb(uint8_t idx)
{
    tuh_itf_info_t itf_info {};
    tuh_cdc_itf_get_info(idx, &itf_info);
    printf("USB CDC unmounted: address=%u, itf_num=%u\n", itf_info.daddr, itf_info.desc.bInterfaceNumber);

    if (handheld_device.has_value() && handheld_device->address == itf_info.daddr) {
        handheld_device.reset();
        rust_event_handheld_unmount();
    }
}

usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count)
{
    *driver_count = 1;
    return &usbh_xinput_driver;
}

static void control_xfer_complete_cb(tuh_xfer_t* xfer)
{
    rust_event_handheld_xfer_complete(
        xfer->setup->bRequest,
        xfer->result == XFER_RESULT_SUCCESS,
        xfer->user_data,
        xfer->buffer,
        xfer->actual_len);
}