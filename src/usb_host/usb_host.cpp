#include "usb_host/usb_host.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "FreeRTOS.h"
#include "core/core.h"
#include "handheld/handheld.h"
#include "host/hcd.h"
#include "pico/time.h"
#include "pio_usb.h"
#include "priorities.h"
#include "stream_buffer.h"
#include "task.h"
#include "tusb.h"

namespace {
constexpr size_t kStackSize = 8 * 1024;
constexpr size_t kRxBufferMaxLen = 64;
constexpr size_t kTxBufferLen = 1024;

/// USB state for a handheld device
struct HandheldDeviceState {
    uint8_t address;
    uint8_t cdc_index;

    uint8_t rx_buffer[kRxBufferMaxLen];
    size_t rx_buffer_len = 0;
};

static std::optional<HandheldDeviceState> handheld_device{};

StreamBufferHandle_t tx_buffer;
}  // namespace

void usb_host_task(void*) {
    // Initialize USB host stack
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = PIN_USB_DP;
    pio_cfg.pinout = PIO_USB_PINOUT_DPDM;
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    tuh_init(BOARD_TUH_RHPORT);

    while (1) {
        // TODO: figure out the best way to interleave device acceses and tuh_task without busy-looping.
        tuh_task_ext(5, false);

        // Send data to CDC
        while (!xStreamBufferIsEmpty(tx_buffer)) {
            uint8_t buf[64];
            size_t len = xStreamBufferReceive(tx_buffer, buf, sizeof(buf), 0);
            if (handheld_device.has_value()) {
                uint8_t idx = handheld_device->cdc_index;
                tuh_cdc_write(idx, buf, len);
                tuh_cdc_write_flush(idx);
            }
        }
    }
}

void InitUsbHost() {
    // Create USB task, pin it to core 1.
    TaskHandle_t task_handle;
    xTaskCreate(usb_host_task, "usbh", kStackSize, NULL, static_cast<UBaseType_t>(TaskPriority::kUsbHost),
                &task_handle);
    vTaskCoreAffinitySet(task_handle, 1 << 1);

    tx_buffer = xStreamBufferCreate(kTxBufferLen, 0);
}

void UsbWriteHandheldData(uint8_t* data, size_t len) {
    xStreamBufferSend(tx_buffer, data, len, portMAX_DELAY);
}

void tuh_mount_cb(uint8_t dev_addr) {
    uint16_t vid = 0;
    uint16_t pid = 0;
    tuh_vid_pid_get(dev_addr, &vid, &pid);
    printf("Mounted USB device address=%d vid=%x pid=%x\n", dev_addr, vid, pid);
}

void tuh_umount_cb(uint8_t dev_addr) {
    printf("Unmounted USB device address=%d\n", dev_addr);
}

void tuh_cdc_rx_cb(uint8_t idx) {
    // Ignore non-handheld data.
    if (!handheld_device.has_value() || handheld_device->cdc_index != idx) {
        return;
    }
    auto& state = *handheld_device;

    uint8_t buffer[64];
    uint32_t available = tuh_cdc_read(idx, &buffer, sizeof(buffer));

    for (uint32_t i = 0; i < available; i++) {
        uint8_t data = buffer[i];

        // End of line, pass it to event loop.
        if (data == '\n' && state.rx_buffer_len > 0) {
            if (state.rx_buffer[0] == '<') {
                // Send just the command response.
                Event event{};
                event.type = EventType::kHandheldRxData;
                event.handheld_rx_data.len = state.rx_buffer_len - 1;
                memcpy(event.handheld_rx_data.data, state.rx_buffer + 1, state.rx_buffer_len - 1);
                PostEvent(event);
            } else {
                // Not a command response (log?), print it.
                printf("| %.*s\n", state.rx_buffer_len, state.rx_buffer);
            }
            state.rx_buffer_len = 0;
            continue;
        }

        if (state.rx_buffer_len < kRxBufferMaxLen) {
            state.rx_buffer[state.rx_buffer_len] = data;
            state.rx_buffer_len++;
        }
    }
}

/// Return the port that the device is plugged into, or 0 if not plugged into the root hub.
static uint8_t GetDeviceHubPort(uint8_t device_address) {
    hcd_devtree_info_t devtree_info{};
    hcd_devtree_get_info(device_address, &devtree_info);
    uint8_t port = devtree_info.hub_port;

    // Now, see if the hub that the device is part of is itself part of the root hub.
    hcd_devtree_get_info(devtree_info.hub_addr, &devtree_info);
    if (devtree_info.hub_addr != 0) {
        return 0;
    }
    return port;
}

void tuh_cdc_mount_cb(uint8_t idx) {
    tuh_itf_info_t itf_info{};
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

    Event event{};
    event.type = EventType::kHandheldMount;
    PostEvent(event);
}

void tuh_cdc_umount_cb(uint8_t idx) {
    tuh_itf_info_t itf_info{};
    tuh_cdc_itf_get_info(idx, &itf_info);
    printf("USB CDC unmounted: address=%u, itf_num=%u\n", itf_info.daddr, itf_info.desc.bInterfaceNumber);

    if (handheld_device.has_value() && handheld_device->address == itf_info.daddr) {
        handheld_device.reset();

        Event event{};
        event.type = EventType::kHandheldUnmount;
        PostEvent(event);
    }
}
