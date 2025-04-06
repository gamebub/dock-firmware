#include "log/log.h"
#include "tusb.h"

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    log_info("USB HID device mount dev_addr=%u instance=%u", dev_addr, instance);
    log_info("vid = %04x, pid = %04x", vid, pid);

    // TODO parse descriptor
    log_info("descriptor len=%u", desc_len);

    // Request the next report.
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        log_error("tuh_hid_receive_report failed");
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    log_info("USB HID device unmount dev_addr=%u instance=%u", dev_addr, instance);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    if (len == 0) {
        return;
    }

    char buffer[3 * 20];
    char* x = buffer;
    for (int i = 0; i < len && i < 20; i++) {
        x += sprintf(x, "%02x ", report[i]);
    }
    log_info("USB HID report len=%u [%s]", len, buffer);

    // Request next report
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        log_error("tuh_hid_receive_report failed");
    }
}
