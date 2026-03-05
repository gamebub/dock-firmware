#include <array>
#include <optional>

#include "host/usbh.h"
#include "log/log.h"
#include "rust_api.h"
#include "tusb_config.h"
#include "xinput_host.h"

#include "controller/uni_controller_type.h"

namespace {
constexpr size_t kMaxGamepads = CFG_TUH_XINPUT;

/// Threshold for reporting trigger buttons as digital L2/R2 press.
constexpr uint8_t kTriggerThreshold = 8;
}  // namespace

struct XinputGamepad {
    /// Gamepad slot ID
    uint32_t gamepad_id;

    /// USB device address
    uint8_t dev_addr;

    /// tuh_xinput instance
    uint8_t instance;

    /// tuh_xinput interface
    xinputh_interface_t const* xid_itf;
};

static std::array<std::optional<XinputGamepad>, kMaxGamepads> gamepads;

static void ReportEvent(uint32_t gamepad_id, xinputh_interface_t const* xid_itf) {
    const xinput_gamepad_t* p = &xid_itf->pad;

    GamepadData data;
    // ABXY
    data.buttons = (p->wButtons & 0xF000) >> 12;
    // DPAD
    data.buttons |= (p->wButtons & XINPUT_GAMEPAD_DPAD_UP) ? (1 << 4) : 0;
    data.buttons |= (p->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) ? (1 << 5) : 0;
    data.buttons |= (p->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ? (1 << 6) : 0;
    data.buttons |= (p->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) ? (1 << 7) : 0;
    // System, Select, Start, (Capture)
    data.buttons |= (p->wButtons & XINPUT_GAMEPAD_GUIDE) ? (1 << 8) : 0;
    data.buttons |= (p->wButtons & XINPUT_GAMEPAD_BACK) ? (1 << 9) : 0;
    data.buttons |= (p->wButtons & XINPUT_GAMEPAD_START) ? (1 << 10) : 0;
    data.buttons |= (p->wButtons & XINPUT_GAMEPAD_SHARE) ? (1 << 11) : 0;
    // L1 R1 L2 R2 L3 R3
    data.buttons |= (p->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? (1 << 12) : 0;
    data.buttons |= (p->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? (1 << 13) : 0;
    data.buttons |= (p->bLeftTrigger > kTriggerThreshold) ? (1 << 14) : 0;
    data.buttons |= (p->bRightTrigger > kTriggerThreshold) ? (1 << 15) : 0;
    data.buttons |= (p->wButtons & XINPUT_GAMEPAD_LEFT_THUMB) ? (1 << 16) : 0;
    data.buttons |= (p->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) ? (1 << 17) : 0;
    // Analog sticks and triggers
    data.lx = p->sThumbLX;
    data.ly = (p->sThumbLY == INT16_MIN) ? INT16_MAX : -p->sThumbLY;
    data.rx = p->sThumbRX;
    data.ry = (p->sThumbRY == INT16_MIN) ? INT16_MAX : -p->sThumbRY;
    data.lz = static_cast<uint16_t>(p->bLeftTrigger) << 8;
    data.rz = static_cast<uint16_t>(p->bRightTrigger) << 8;
    
    rust_event_gamepad_data(gamepad_id, data);
}

void tuh_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance, xinputh_interface_t const* xid_itf,
                                   uint16_t len) {
    static_cast<void>(len);
    if (xid_itf->last_xfer_result == XFER_RESULT_SUCCESS && xid_itf->connected && xid_itf->new_pad_data) {
        // Report event
        for (auto& gamepad : gamepads) {
            if (!gamepad.has_value()) {
                continue;
            }
            if (gamepad->dev_addr == dev_addr && gamepad->instance == instance) {
                ReportEvent(gamepad->gamepad_id, xid_itf);
                break;
            }
        }
    }
    tuh_xinput_receive_report(dev_addr, instance);
}

void tuh_xinput_mount_cb(uint8_t dev_addr, uint8_t instance, const xinputh_interface_t* xinput_itf) {
    log_info("USB xinput mounted: dev_addr=%02x instance=%u", dev_addr, instance);

    // If this is a Xbox 360 Wireless controller we need to wait for a connection packet
    // on the in pipe before setting LEDs etc. So just start getting data until a controller is connected.
    if (xinput_itf->type == XBOX360_WIRELESS && xinput_itf->connected == false) {
        tuh_xinput_receive_report(dev_addr, instance);
        return;
    }

    tuh_xinput_set_led(dev_addr, instance, 0, true);
    tuh_xinput_set_led(dev_addr, instance, 1, true);
    tuh_xinput_set_rumble(dev_addr, instance, 0, 0, true);
    tuh_xinput_receive_report(dev_addr, instance);

    uint32_t gamepad_id = rust_gamepad_allocate_id();

    // Find an empty slot
    for (auto& gamepad : gamepads) {
        if (gamepad.has_value()) {
            continue;
        }
        gamepad = XinputGamepad{
            .gamepad_id = gamepad_id,
            .dev_addr = dev_addr,
            .instance = instance,
            .xid_itf = xinput_itf,
        };

        uni_controller_type_t gamepad_type = k_eControllerType_Unknown;
        switch (xinput_itf->type) {
            case XBOXONE:
                gamepad_type = k_eControllerType_XBoxOneController;
                break;
            case XBOX360_WIRELESS:
            case XBOX360_WIRED:
                gamepad_type = k_eControllerType_XBox360Controller;
                break;
            case XBOXOG:
                // Not strictly true, but close
                gamepad_type = k_eControllerType_XBox360Controller;
                break;
            default:
                break;
        }

        uint8_t device_id[8];
        memset(device_id, 0, sizeof(device_id));
        rust_event_gamepad_connected(
            gamepad_id,
            (uint32_t)gamepad_type,
            device_id,
            true
        );
        return;
    }
    log_warn("No xinput slot available");
}

void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance) {
    log_info("USB xinput unmounted: dev_addr=%02x instance=%u", dev_addr, instance);

    for (auto& gamepad : gamepads) {
        if (!gamepad.has_value()) {
            continue;
        }
        if (!(gamepad->dev_addr == dev_addr && gamepad->instance == instance)) {
            continue;
        }

        rust_event_gamepad_disconnected(gamepad->gamepad_id);

        gamepad.reset();
        break;
    }
}
