#include "core/core.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

#include "FreeRTOS.h"
#include "bluetooth/bluetooth.h"
#include "gpio/gpio.h"
#include "hwinfo/hwinfo.h"
#include "led/led.h"
#include "log/log.h"
#include "pico/mutex.h"
#include "priorities.h"
#include "queue.h"
#include "task.h"
#include "timers.h"
#include "usb_host/usb_host.h"

namespace {

constexpr uint8_t kRequestGetInfo = 0;
constexpr uint8_t kRequestDockBegin = 3;
constexpr uint8_t kRequestGamepadConnect = 4;
constexpr uint8_t kRequestGamepadDisconnect = 5;
constexpr uint8_t kRequestGamepadData = 6;

enum class State {
    /// Handheld is not connected
    Idle,
    /// Sent info request, waiting for response.
    WaitForGetInfo,
    /// Send dock begin, waiting for response.
    WaitForDockBegin,
    /// Docking is active.
    Active,
    /// Handheld is in an error state
    Error,
};

constexpr size_t kStackSize = 8 * 1024;
constexpr uint32_t kEventQueueLength = 32;
constexpr TickType_t kPingInterval = pdMS_TO_TICKS(500);
constexpr TickType_t kPingTimeout = pdMS_TO_TICKS(1000);

QueueHandle_t event_queue = nullptr;
State state = State::Idle;
std::vector<Gamepad> gamepads;
bool is_pairing = false;
static TimerHandle_t ping_interval_timer;
static TimerHandle_t ping_timeout_timer;

void WriteGamepadConnected(const Gamepad& gamepad) {
    // 4 byte: slot
    // 4 byte: reserved
    // 8 byte: gamepad device ID
    // 32 byte: model name (\0 terminated)
    alignas(4) std::array<uint8_t, 48> buffer{};
    *((uint32_t*)&buffer[0]) = gamepad.id;
    *((uint32_t*)&buffer[4]) = 0;
    memcpy(&buffer[8], gamepad.device_id, 8);
    const char* model = uni_gamepad_get_model_name(gamepad.gamepad_type);
    strncpy((char*)&buffer[16], model, 31);
    UsbHandheldControlOut(kRequestGamepadConnect, 0, 0, buffer);
}

void HandleHandheldXfer(bool success, uint8_t request, uintptr_t tag, std::span<const uint8_t> data) {
    (void)tag;
    switch (state) {
        case State::WaitForGetInfo: {
            if (request != kRequestGetInfo) {
                break;
            }
            if (success && data.size() >= 16) {
                // 0.. 4: reserved
                // 4.. 8: serial
                // 8.. 12: hw version
                // 12..16: fw version
                // Assumes little endian system
                uint32_t serial = *(uint32_t*)(&data[4]);
                uint32_t hw_version = *(uint32_t*)(&data[8]);
                uint32_t fw_version = *(uint32_t*)(&data[12]);
                log_info("Handheld Info:");
                log_info("  Serial: %08lX", serial);
                log_info("  HW:     %08lX", hw_version);
                log_info("  FW:     %08lX", fw_version);

                std::array<uint32_t, 4> buffer{};
                buffer[0] = 0;
                buffer[1] = GetSerialNumber();
                buffer[2] = GetHardwareVersion().value();
                buffer[3] =
                    (DOCK_FW_VERSION_MAJOR << 24) | (DOCK_FW_VERSION_MINOR << 16) | (DOCK_FW_VERSION_PATCH << 8);
                UsbHandheldControlOut(kRequestDockBegin, 0, 0, std::span((uint8_t*)buffer.data(), sizeof(buffer)));
                state = State::WaitForDockBegin;
            } else {
                log_error("get_info error");
                state = State::Error;
            }
            break;
        }
        case State::WaitForDockBegin: {
            if (request != kRequestDockBegin) {
                break;
            }
            if (success) {
                log_info("Dock begin success");
                // Send all already-connected gamepads
                for (const Gamepad& gamepad : gamepads) {
                    WriteGamepadConnected(gamepad);
                }
                state = State::Active;
                SetHdmiActive(true);
                SetLedState(LedState::kDockActive);
                xTimerReset(ping_timeout_timer, portMAX_DELAY);
                xTimerReset(ping_interval_timer, portMAX_DELAY);
            } else {
                log_error("Dock begin error");
                state = State::Error;
            }
            break;
        }
        case State::Active: {
            if (success) {
                xTimerReset(ping_timeout_timer, portMAX_DELAY);
            }
        }
        default:
            break;
    }
}

void HandleEvent(const Event& event) {
    switch (event.type) {
        case EventType::kHandheldMount: {
            if (state != State::Idle) {
                log_error("Unexpected handheld mount");
                break;
            }

            log_info("Handheld mount");
            UsbHandheldControlIn(kRequestGetInfo, /* value= */ 0, /* tag= */ 0, /* length= */ 16);
            state = State::WaitForGetInfo;
            break;
        }

        case EventType::kHandheldUnmount:
            log_info("Handheld unmount");
            state = State::Idle;
            UnsetLedState(LedState::kDockActive);
            SetHdmiActive(false);
            xTimerStop(ping_timeout_timer, portMAX_DELAY);
            break;

        case EventType::kHandheldXferComplete: {
            const auto& data = event.handheld_xfer;
            HandleHandheldXfer(data.success, data.request, data.tag, std::span(data.data).first(data.length));
            break;
        }

        case EventType::kHandheldPing: {
            if (state != State::Active) {
                break;
            }
            // TODO: re-add ping
            // char command[] = "\n>get_hwinfo\n";
            // UsbWriteHandheldData((uint8_t*)command, sizeof(command) - 1);
            // TODO: perhaps this should only happen if we're getting close to the timeout
            // (e.g. treat it as a watchdog)
            xTimerReset(ping_interval_timer, portMAX_DELAY);
            break;
        }

        case EventType::kGamepadConnected: {
            const auto& data = event.gamepad_connected;
            log_info("Gamepad connected: id=%lu", data.gamepad.id);
            gamepads.push_back(data.gamepad);

            if (state == State::Active) {
                WriteGamepadConnected(data.gamepad);
            }

            is_pairing = false;
            UnsetLedState(LedState::kBluetoothPairing);
            BluetoothEnablePairing(false);
            break;
        }

        case EventType::kGamepadDisconnected: {
            const auto& data = event.gamepad_disconnected;
            log_info("Gamepad disconnected: id=%lu", data.gamepad_id);
            auto entry_it = std::find_if(gamepads.begin(), gamepads.end(),
                                         [&](const auto& val) { return val.id == data.gamepad_id; });
            if (entry_it == gamepads.end()) {
                break;
            }
            gamepads.erase(entry_it);

            if (state == State::Active) {
                std::array<uint32_t, 1> buffer{};
                buffer[0] = data.gamepad_id;
                UsbHandheldControlOut(kRequestGamepadDisconnect, 0, 0,
                                      std::span((uint8_t*)buffer.data(), sizeof(buffer)));
            }
            break;
        }

        case EventType::kGamepadData: {
            const auto& gp = event.gamepad_data.data;
            // log_info("Gamepad: [%05lX] L(%6d %6d) R(%6d %6d) (Lz=%5u Rz=%5u)", gp.buttons, gp.lx, gp.ly, gp.rx,
            // gp.ry, gp.lz, gp.rz);
            if (state == State::Active) {
                // 4 byte: slot
                // 16 byte: gamepad data
                // Requires little endian

                alignas(4) std::array<uint8_t, 20> buffer{};
                *((uint32_t*)&buffer[0]) = event.gamepad_data.gamepad_id;
                memcpy(&buffer[4], &gp, 16);
                UsbHandheldControlOut(kRequestGamepadData, 0, 0, buffer);
            }
            break;
        }

        case EventType::kButtonShortPress: {
            log_info("Button short press");
            break;
        }

        case EventType::kButtonLongPress: {
            is_pairing = !is_pairing;
            if (is_pairing) {
                log_info("Enter pairing mode");
                SetLedState(LedState::kBluetoothPairing);
            } else {
                log_info("Exit pairing mode");
                UnsetLedState(LedState::kBluetoothPairing);
            }
            BluetoothEnablePairing(is_pairing);
            break;
        }

        default:
            break;
    }
}

void CoreTask(void*) {
    while (true) {
        Event event{};
        auto result = xQueueReceive(event_queue, &event, portMAX_DELAY);
        if (result == pdPASS) {
            HandleEvent(event);
        }
    }
}

void HandheldPingTask(TimerHandle_t) {
    Event event{};
    event.type = EventType::kHandheldPing;
    PostEvent(event);
}

void HandheldPingTimeoutTask(TimerHandle_t) {
    // TODO: re-add time out
    // log_warn("Handheld timed out");
    // Event event{};
    // event.type = EventType::kHandheldUnmount;
    // PostEvent(event);
}

}  // namespace

void InitCore() {
    event_queue = xQueueCreate(kEventQueueLength, sizeof(Event));

    // Setup ping timer.
    // TODO: remove once the USB bug is figured out, where repeatedly sending
    // USB CDC data prevents tinyusb from realizing the device was disconnected
    ping_timeout_timer =
        xTimerCreate("handheld_timeout", kPingTimeout, /* uxAutoReload= */ false, nullptr, HandheldPingTimeoutTask);
    ping_interval_timer =
        xTimerCreate("handheld_ping", kPingInterval, /* uxAutoReload= */ false, nullptr, HandheldPingTask);

    TaskHandle_t task_handle;
    xTaskCreate(CoreTask, "core", kStackSize, NULL, static_cast<UBaseType_t>(TaskPriority::kCore), &task_handle);
}

void PostEvent(const Event& event) {
    auto result = xQueueSendToBack(event_queue, &event,
                                   /* xTicksToWait= */ 0);
    if (result != pdPASS) {
        log_error("Failed to post event=%u", event.type);
    }
}

auto_init_mutex(gamepad_id_mutex);

uint32_t AssignGamepadId() {
    static uint32_t next_gamepad_id = 1;
    mutex_enter_blocking(&gamepad_id_mutex);
    uint32_t id = next_gamepad_id++;
    mutex_exit(&gamepad_id_mutex);
    return id;
}