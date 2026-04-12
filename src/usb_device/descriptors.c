/*
 * This file is based on a file originally part of the
 * MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2019 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "tusb.h"

uint32_t rust_info_serial_number(void);
uint32_t rust_info_hardware_version(void);

#define USBD_VID (0x1209) // pid.codes
#define USBD_PID (0xB011) // Game Bub Dock
#define USBD_MANUFACTURER "Second Bedroom"
#define USBD_PRODUCT "Game Bub Dock"

#define USBD_DESC_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + 9)

#define USBD_CONFIGURATION_DESCRIPTOR_ATTRIBUTE (0)
#define USBD_MAX_POWER_MA (500)

#define USBD_ITF_CDC (0) // needs 2 interfaces
#define USBD_ITF_VENDOR (2)
#define USBD_ITF_MAX (3)

#define USBD_CDC_EP_CMD (0x81)
#define USBD_CDC_EP_OUT (0x02)
#define USBD_CDC_EP_IN (0x82)
#define USBD_CDC_CMD_MAX_SIZE (8)
#define USBD_CDC_IN_OUT_MAX_SIZE (64)

#define USBD_STR_0 (0x00)
#define USBD_STR_MANUF (0x01)
#define USBD_STR_PRODUCT (0x02)
#define USBD_STR_SERIAL (0x03)
#define USBD_STR_CDC (0x04)
#define USBD_STR_VENDOR_CONTROL (0x05)

// Note: descriptors returned from callbacks must exist long enough for transfer to complete

static tusb_desc_device_t usbd_desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
// On Windows, if bcdUSB = 0x210 then a Microsoft OS 2.0 descriptor is required, else the device won't be detected
// This is only needed for driverless access to the reset interface - the CDC interface doesn't require these
// descriptors for driverless access, but will still not work if bcdUSB = 0x210 and no descriptor is provided. Therefore
// always use bcdUSB = 0x200 if the Microsoft OS 2.0 descriptor isn't enabled
#if PICO_STDIO_USB_RESET_INTERFACE_SUPPORT_MS_OS_20_DESCRIPTOR
    .bcdUSB = 0x0210,
#else
    .bcdUSB = 0x0200,
#endif
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USBD_VID,
    .idProduct = USBD_PID,
    .bcdDevice = 0x0000,
    .iManufacturer = USBD_STR_MANUF,
    .iProduct = USBD_STR_PRODUCT,
    .iSerialNumber = USBD_STR_SERIAL,
    .bNumConfigurations = 1,
};

static const uint8_t usbd_desc_cfg[USBD_DESC_LEN] = {
    TUD_CONFIG_DESCRIPTOR(1, USBD_ITF_MAX, USBD_STR_0, USBD_DESC_LEN, USBD_CONFIGURATION_DESCRIPTOR_ATTRIBUTE,
        USBD_MAX_POWER_MA),

    TUD_CDC_DESCRIPTOR(USBD_ITF_CDC, USBD_STR_CDC, USBD_CDC_EP_CMD, USBD_CDC_CMD_MAX_SIZE, USBD_CDC_EP_OUT,
        USBD_CDC_EP_IN, USBD_CDC_IN_OUT_MAX_SIZE),

    // Vendor interface
    9,
    TUSB_DESC_INTERFACE,
    USBD_ITF_VENDOR,
    0,
    0,
    TUSB_CLASS_VENDOR_SPECIFIC,
    /* subclass */ 1,
    /* protocol */ 0,
    USBD_STR_VENDOR_CONTROL,
};

static char usbd_serial_str[8 + 1];

static const char* const usbd_desc_str[] = {
    [USBD_STR_MANUF] = USBD_MANUFACTURER,
    [USBD_STR_PRODUCT] = USBD_PRODUCT,
    [USBD_STR_SERIAL] = usbd_serial_str,
    [USBD_STR_CDC] = "Board CDC",
    [USBD_STR_VENDOR_CONTROL] = "Game Bub Control",
};

const uint8_t* tud_descriptor_device_cb(void)
{
    // Set device version.
    uint16_t bcdDevice = 0;
    bcdDevice |= ((rust_info_hardware_version() >> 16) & 0xFF) << 8; // hw major
    bcdDevice |= ((rust_info_hardware_version() >> 8) & 0xF) << 4; // hw minor
    bcdDevice |= 0x00; // mode: normal

    return (const uint8_t*)&usbd_desc_device;
}

const uint8_t* tud_descriptor_configuration_cb(__unused uint8_t index)
{
    return usbd_desc_cfg;
}

const uint16_t* tud_descriptor_string_cb(uint8_t index, __unused uint16_t langid)
{
#ifndef USBD_DESC_STR_MAX
#define USBD_DESC_STR_MAX (20)
#elif USBD_DESC_STR_MAX > 127
#error USBD_DESC_STR_MAX too high (max is 127).
#elif USBD_DESC_STR_MAX < 17
#error USBD_DESC_STR_MAX too low (min is 17).
#endif
    static uint16_t desc_str[USBD_DESC_STR_MAX];

    // Assign the SN.
    uint32_t serial_number = rust_info_serial_number();
    snprintf(usbd_serial_str, sizeof(usbd_serial_str), "%08lX", serial_number);

    uint8_t len;
    if (index == 0) {
        desc_str[1] = 0x0409; // supported language is English
        len = 1;
    } else {
        if (index >= sizeof(usbd_desc_str) / sizeof(usbd_desc_str[0])) {
            return NULL;
        }
        const char* str = usbd_desc_str[index];
        for (len = 0; len < USBD_DESC_STR_MAX - 1 && str[len]; ++len) {
            desc_str[1 + len] = str[len];
        }
    }

    // first byte is length (including header), second byte is string type
    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));

    return desc_str;
}
