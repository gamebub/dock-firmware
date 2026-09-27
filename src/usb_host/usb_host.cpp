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
constexpr size_t kXferBufferLen = 64;

/// USB state for a handheld device
struct HandheldDeviceState {
    uint8_t address = 0xFF;
    uint8_t cdc_index = 0xFF;

    uint8_t vendor_itf_num = 0xFF;
    uint8_t vendor_ep_out = 0xFF;
    uint8_t vendor_ep_in = 0xFF;

    uint8_t rx_buffer[kRxBufferMaxLen];
    size_t rx_buffer_len = 0;
};

static std::optional<HandheldDeviceState> handheld_device { };

static uint8_t configDescriptorBuf[256];

static uint8_t handheldOutPacketBuf[64];
static bool handheldOutPacketBusy = false;

} // namespace

static void control_xfer_complete_cb(tuh_xfer_t* xfer);
static void config_descriptor_xfer_complete_cb(tuh_xfer_t* xfer);
static void bulk_out_xfer_complete_cb(tuh_xfer_t* xfer);
static void pump_handheld_packet_queue(void);

void usb_host_task(void*)
{
    // Initialize USB host stack
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = PIN_USB_DP;
    pio_cfg.pinout = PIO_USB_PINOUT_DPDM;
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    tuh_init(BOARD_TUH_RHPORT);

    while (1) {
        tuh_task_ext(1, false);
        pump_handheld_packet_queue();
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
}

/// Return the port that the device is plugged into, or 0 if not plugged into the root hub.
static uint8_t GetDeviceHubPort(uint8_t device_address)
{
    hcd_devtree_info_t devtree_info { };
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

    // TODO: use a free-list instead of malloc?
    uint8_t* buffer = (uint8_t*)malloc(data.size());
    memcpy(buffer, data.data(), data.size());

    tusb_control_request_t setup = {
        .bmRequestType_bit = {
            .recipient = TUSB_REQ_RCPT_INTERFACE,
            .type = TUSB_REQ_TYPE_VENDOR,
            .direction = TUSB_DIR_OUT,
        },
        .bRequest = request,
        .wValue = value,
        .wIndex = handheld_device->vendor_itf_num,
        .wLength = (uint16_t)data.size(),
    };

    tuh_xfer_t xfer = {
        .daddr = handheld_device->address,
        .ep_addr = 0,
        .setup = &setup,
        .buffer = buffer,
        .complete_cb = control_xfer_complete_cb,
        .user_data = tag,
    };
    bool result = tuh_control_xfer(&xfer);
    if (!result) {
        free(buffer);
        rust_event_handheld_xfer_complete(
            request,
            false,
            tag,
            (const uint8_t*)4,
            0);
    }
}

void UsbHandheldControlIn(uint8_t request, uint16_t value, uintptr_t tag, uint16_t length)
{
    // TODO: use a free-list instead of malloc?
    uint8_t* buffer = (uint8_t*)malloc(length);

    tusb_control_request_t setup = {
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

    tuh_xfer_t xfer = {
        .daddr = handheld_device->address,
        .ep_addr = 0,
        .setup = &setup,
        .buffer = buffer,
        .complete_cb = control_xfer_complete_cb,
        .user_data = tag,
    };
    bool result = tuh_control_xfer(&xfer);
    if (!result) {
        free(buffer);
        log_error("Failed to start Control In");
    }
}

void tuh_mount_cb(uint8_t dev_addr)
{
    uint16_t vid = 0;
    uint16_t pid = 0;
    tuh_vid_pid_get(dev_addr, &vid, &pid);
    uint8_t port = GetDeviceHubPort(dev_addr);
    printf("Mounted USB device: port=%d addr=%d [%04X:%04X]\n", port, dev_addr, vid, pid);

    // Determine whether this is a handheld.
    if (port != Handheld::kHubPort) {
        // Not plugged into the main port.
        return;
    }
    if (!Handheld::CheckUsbId(vid, pid)) {
        return;
    }

    // Prepare handheld state.
    handheld_device.emplace();
    handheld_device->address = dev_addr;

    // Load descriptors and find interfaces / endpoints.
    tuh_descriptor_get_configuration(
        dev_addr,
        /* index= */ 0,
        configDescriptorBuf,
        sizeof(configDescriptorBuf),
        config_descriptor_xfer_complete_cb,
        /* user_data= */ 0);
}

void tuh_umount_cb(uint8_t dev_addr)
{
    printf("Unmounted USB device address=%d\n", dev_addr);
    if (handheld_device.has_value() && handheld_device->address == dev_addr) {
        handheld_device.reset();
        rust_event_handheld_unmount();
    }
}

void config_descriptor_xfer_complete_cb(tuh_xfer_t* xfer)
{
    if (xfer->result != XFER_RESULT_SUCCESS) {
        printf("Failed to retrieve config descriptor: %d\n", xfer->result);
        return;
    }
    if (!handheld_device.has_value()) {
        // This shouldn't happen.
        return;
    }

    const uint8_t* p_desc = xfer->buffer;
    const uint16_t total_len = xfer->actual_len;
    const uint8_t* p_end = p_desc + total_len;

    bool current_interface_is_vendor = false;

    // Iterate through all of the descriptors.
    while (p_desc < p_end) {
        uint8_t desc_type = tu_desc_type(p_desc);

        if (desc_type == TUSB_DESC_INTERFACE) {
            tusb_desc_interface_t* itf_desc = (tusb_desc_interface_t*)p_desc;

            // Check for the required vendor-specific interface.
            current_interface_is_vendor = false;
            if (itf_desc->bInterfaceClass == TUSB_CLASS_VENDOR_SPECIFIC && itf_desc->bInterfaceSubClass == 1 && itf_desc->bInterfaceProtocol == 0) {
                current_interface_is_vendor = true;
                handheld_device->vendor_itf_num = itf_desc->bInterfaceNumber;
            }
        } else if (desc_type == TUSB_DESC_ENDPOINT && current_interface_is_vendor) {
            tusb_desc_endpoint_t* ep_desc = (tusb_desc_endpoint_t*)p_desc;
            uint8_t ep_addr = ep_desc->bEndpointAddress;

            // Check for the required endpoints.
            if (ep_desc->bmAttributes.xfer == TUSB_XFER_BULK && tu_edpt_dir(ep_addr) == TUSB_DIR_OUT) {
                handheld_device->vendor_ep_out = ep_addr;
                tuh_edpt_open(xfer->daddr, ep_desc);
            }
            if (ep_desc->bmAttributes.xfer == TUSB_XFER_BULK && tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
                handheld_device->vendor_ep_in = ep_addr;
                tuh_edpt_open(xfer->daddr, ep_desc);
            }
        }

        p_desc = tu_desc_next(p_desc);
    }

    // Complete the mount.
    printf("Vendor interface: num=%d ep_out=%02x ep_in=%02x\n", handheld_device->vendor_itf_num, handheld_device->vendor_ep_out, handheld_device->vendor_ep_in);
    handheldOutPacketBusy = false;
    rust_event_handheld_mount();
}

void tuh_cdc_rx_cb(uint8_t idx)
{
    tuh_itf_info_t info;
    if (!tuh_cdc_itf_get_info(idx, &info)) {
        return;
    }

    // Ignore non-handheld data.
    if (!handheld_device.has_value() || handheld_device->address != info.daddr) {
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
    free(xfer->buffer);
}

static void bulk_out_xfer_complete_cb(tuh_xfer_t* xfer)
{
    handheldOutPacketBusy = false;
    if (xfer->result != XFER_RESULT_SUCCESS) {
        printf("bulk OUT failed: %d", xfer->result);
    }
    pump_handheld_packet_queue();
}

void pump_handheld_packet_queue()
{
    if (handheldOutPacketBusy || !handheld_device.has_value()) {
        return;
    }

    size_t packet_len = sizeof(handheldOutPacketBuf);
    bool has_packet = rust_handheld_pop_next_packet(handheldOutPacketBuf, &packet_len);
    if (!has_packet) {
        return;
    }

    handheldOutPacketBusy = true;
    tuh_xfer_t xfer = {
        .daddr = handheld_device->address,
        .ep_addr = handheld_device->vendor_ep_out,
        .buflen = packet_len,
        .buffer = handheldOutPacketBuf,
        .complete_cb = bulk_out_xfer_complete_cb,
        .user_data = 0,
    };
    tuh_edpt_xfer(&xfer);
}