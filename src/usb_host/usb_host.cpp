#include "usb_host/usb_host.h"

#include <cstdint>

#include "pio_usb.h"
#include "tusb.h"

void cdc_app_task(void);

namespace {}

void StartUsbHost() {
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
    uint8_t buf[64 + 1];
    uint32_t const bufsize = sizeof(buf) - 1;

    // forward cdc interfaces -> console
    uint32_t count = tuh_cdc_read(idx, buf, bufsize);
    buf[count] = 0;

    printf("%s", (char*)buf);
}

void tuh_cdc_mount_cb(uint8_t idx) {
    tuh_itf_info_t itf_info{};
    tuh_cdc_itf_get_info(idx, &itf_info);

    printf("USB CDC mounted: address=%u itf_num=%u\n", itf_info.daddr, itf_info.desc.bInterfaceNumber);
}

void tuh_cdc_umount_cb(uint8_t idx) {
    tuh_itf_info_t itf_info{};
    tuh_cdc_itf_get_info(idx, &itf_info);
    printf("USB CDC unmounted: address=%u, itf_num=%u\n", itf_info.daddr, itf_info.desc.bInterfaceNumber);
}
