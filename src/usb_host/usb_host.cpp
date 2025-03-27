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
#include "task.h"
#include "tusb.h"

void cdc_app_task(void);

namespace {
constexpr size_t kStackSize = 8 * 1024;
constexpr size_t kRxBufferMaxLen = 64;

/// USB state for a handheld device
struct HandheldDeviceState {
    uint8_t address;
    uint8_t cdc_index;

    uint8_t rx_buffer[kRxBufferMaxLen];
    size_t rx_buffer_len = 0;
};

static std::optional<HandheldDeviceState> handheld_device{};
}  // namespace

void usb_host_task(void*) {
    // Initialize USB host stack
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = PIN_USB_DP;
    pio_cfg.pinout = PIO_USB_PINOUT_DPDM;
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    tuh_init(BOARD_TUH_RHPORT);

    while (1) {
        tuh_task();
        cdc_app_task();
    }
}

void InitUsbHost() {
    // Create USB task, pin it to core 1.
    TaskHandle_t task_handle;
    xTaskCreate(usb_host_task, "usbh", kStackSize, NULL, static_cast<UBaseType_t>(TaskPriority::kUsbHost),
                &task_handle);
    vTaskCoreAffinitySet(task_handle, 1 << 1);
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

void cdc_app_task(void) {
    uint8_t buf[64 + 1];
    memset(buf, 0, sizeof(buf));
    uint32_t count = 0;

    // Ping every 3000ms.
    static uint32_t next_time = 0;
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if (current_time >= next_time) {
        count = snprintf((char*)buf, sizeof(buf), "\n>get_hwinfo\n");
        next_time += 3000;
    }

    // loop over all mounted interfaces
    for (uint8_t idx = 0; idx < CFG_TUH_CDC; idx++) {
        if (tuh_cdc_mounted(idx)) {
            if (count) {
                tuh_cdc_write(idx, buf, count);
                tuh_cdc_write_flush(idx);
            }
        }
    }
}

void tuh_cdc_rx_cb(uint8_t idx) {
    // Ignore non-handheld data.
    if (!handheld_device.has_value() || handheld_device->cdc_index != idx) {
        return;
    }
    auto& state = *handheld_device;

    while (tuh_cdc_read_available(idx) > 0) {
        uint8_t data;
        tuh_cdc_read(idx, &data, 1);

        // End of line, pass it to event loop.
        if (data == '\n') {
            Event event{};
            event.type = EventType::kHandheldRxData;
            event.handheld_rx_data.len = state.rx_buffer_len;
            memcpy(event.handheld_rx_data.data, state.rx_buffer, state.rx_buffer_len);
            PostEvent(event);

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
