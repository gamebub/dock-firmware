#include "usb_host/hid.h"

#include <array>
#include <optional>

#include "core/core.h"
#include "hid_drivers/generic.h"
#include "log/log.h"
#include "tusb.h"

namespace {
constexpr size_t kMaxGamepads = CFG_TUH_HID;
}  // namespace

static std::array<std::optional<UsbHidGamepad>, kMaxGamepads> gamepads;

bool IsHidGamepad(std::span<const uint8_t> descriptor) {
    // TODO
    return true;
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    log_info("USB HID device mount dev_addr=%u instance=%u", dev_addr, instance);

    // Find an empty slot.
    UsbHidGamepad* gamepad = nullptr;
    for (auto& gp : gamepads) {
        if (!gp.has_value()) {
            gamepad = &gp.emplace();
            break;
        }
    }
    if (gamepad == nullptr) {
        log_warn("No USB HID slot available");
        return;
    }

    // Initialize UsbHidGamepad struct.
    gamepad->gamepad_id = AssignGamepadId();
    gamepad->dev_addr = dev_addr;
    gamepad->instance = instance;

    // Find a driver.
    std::span<const uint8_t> descriptor(desc_report, static_cast<size_t>(desc_len));
    uint16_t vid;
    uint16_t pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);
    std::unique_ptr<UsbHidDriver> driver = nullptr;

    if (driver == nullptr) {
        if (!IsHidGamepad(descriptor)) {
            log_info("Not USB HID gamepad");
            return;
        }
        driver = std::make_unique<GenericHidDriver>(*gamepad);
    }
    gamepad->driver = std::move(driver);

    // Initialize driver
    gamepad->driver->Initialize(descriptor);

    // Send connection event.
    Event event;
    event.type = EventType::kGamepadConnected;
    event.gamepad_connected.gamepad.id = gamepad->gamepad_id;
    event.gamepad_connected.gamepad.gamepad_type = gamepad->driver->GetGamepadType();
    event.gamepad_connected.gamepad.wired = true;
    memset(event.gamepad_connected.gamepad.device_id, 0, sizeof(event.gamepad_connected.gamepad.device_id));
    PostEvent(event);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    log_info("USB HID device unmount dev_addr=%u instance=%u", dev_addr, instance);

    for (auto& gamepad : gamepads) {
        if (!gamepad.has_value()) {
            continue;
        }
        if (!(gamepad->dev_addr == dev_addr && gamepad->instance == instance)) {
            continue;
        }

        Event event;
        event.type = EventType::kGamepadDisconnected;
        event.gamepad_disconnected.gamepad_id = gamepad->gamepad_id;
        PostEvent(event);

        gamepad.reset();
        break;
    }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report_ptr, uint16_t len) {
    std::span<const uint8_t> report(report_ptr, static_cast<size_t>(len));
    if (report.empty()) {
        return;
    }

    for (auto& gamepad : gamepads) {
        if (!gamepad.has_value()) {
            continue;
        }
        if (!(gamepad->dev_addr == dev_addr && gamepad->instance == instance)) {
            continue;
        }

        gamepad->driver->OnReportReceived(report);
        break;
    }
}

void UsbHidDriver::RequestReport() {
    if (!tuh_hid_receive_report(gamepad_.dev_addr, gamepad_.instance)) {
        log_error("tuh_hid_receive_report failed");
    }
}

void UsbHidDriver::ReportData(GamepadData data) {
    Event event;
    event.type = EventType::kGamepadData;
    event.gamepad_data.gamepad_id = gamepad_.gamepad_id;
    event.gamepad_data.data = data;
    PostEvent(event);
}
