#include "core/core.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

#include "FreeRTOS.h"
#include "pico/mutex.h"
#include "priorities.h"
#include "queue.h"
#include "task.h"
#include "usb_host/usb_host.h"

#define log_info(message, ...) printf("[INF] " message "\n", ##__VA_ARGS__)
#define log_warn(message, ...) printf("[WRN] " message "\n", ##__VA_ARGS__)
#define log_error(message, ...) printf("[ERR] " message "\n", ##__VA_ARGS__)

namespace {

enum class State {
    /// Handheld is not connected
    Idle,
    /// Sent info request, waiting for response.
    WaitForHwInfo,
    /// Send dock begin, waiting for response.
    WaitForDockBegin,
    /// Docking is active.
    Active,
    /// Handheld is in an error state
    Error,
};

struct GamepadData {
    // From bit 0:
    // (A B X Y) (Up Down Right Left) (System Select Start Capture(?)) (L1 R1 L2 R2 L3 R3)
    uint32_t buttons;
    int16_t lx;
    int16_t ly;
    int16_t lz;
    int16_t rx;
    int16_t ry;
    int16_t rz;
};

constexpr size_t kStackSize = 8 * 1024;
constexpr uint32_t kEventQueueLength = 32;

QueueHandle_t event_queue = nullptr;
State state = State::Idle;
std::vector<Gamepad> gamepads;

void WriteGamepadConnected(const Gamepad& gamepad) {
    char buffer[64];
    int len = sprintf(buffer, ">gamepad_connect,%lu\n", gamepad.id);
    UsbWriteHandheldData((uint8_t*)buffer, len);
    // TODO pass other info (type, name, etc.)
}

void HandleHandheldResponse(const std::string_view response) {
    switch (state) {
        case State::WaitForHwInfo: {
            if (response.starts_with("ok")) {
                log_info("Got hw_info: %.*s", response.size(), response.data());
                char command[] = ">dock_begin\n";
                UsbWriteHandheldData((uint8_t*)command, sizeof(command) - 1);
                state = State::WaitForDockBegin;
            } else {
                log_error("hw_info error");
                state = State::Error;
            }
            break;
        }
        case State::WaitForDockBegin: {
            if (response.starts_with("ok")) {
                log_info("Dock begin success");
                // Send all already-connected gamepads
                for (const Gamepad& gamepad : gamepads) {
                    WriteGamepadConnected(gamepad);
                }
                state = State::Active;
            } else {
                log_error("Dock begin error");
                state = State::Error;
            }
            break;
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
            char command[] = "\n>get_hwinfo\n";
            UsbWriteHandheldData((uint8_t*)command, sizeof(command) - 1);
            state = State::WaitForHwInfo;
            break;
        }

        case EventType::kHandheldUnmount:
            log_info("Handheld unmount");
            state = State::Idle;
            break;

        case EventType::kHandheldRxData: {
            const auto& data = event.handheld_rx_data;
            HandleHandheldResponse(std::string_view((char*)&data.data, data.len));
            break;
        }

        case EventType::kGamepadConnected: {
            const auto& data = event.gamepad_connected;
            log_info("Gamepad connected: id=%lu", data.gamepad.id);
            gamepads.push_back(data.gamepad);

            if (state == State::Active) {
                WriteGamepadConnected(data.gamepad);
            }
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
                char buffer[64];
                int len = sprintf(buffer, ">gamepad_disconnect,%lu\n", data.gamepad_id);
                UsbWriteHandheldData((uint8_t*)buffer, len);
            }
            break;
        }

        case EventType::kGamepadData: {
            const auto& data = event.gamepad_data.data;
            // buttons: 32 bits: (A B X Y) (Up Down Right Left) (System Select Start Capture(?)) (L1 R1 L2 R2 L3 R3)
            GamepadData gp;
            gp.buttons = ((data.buttons & 0b1111) << 0) | ((data.dpad & 0b1111) << 4) |
                         ((data.misc_buttons & 0b1111) << 8) | (((data.buttons & 0b1111110000) >> 4) << 12);
            gp.lx = (int16_t)(data.axis_x << 6);
            gp.ly = (int16_t)(data.axis_y << 6);
            gp.lz = (int16_t)(data.brake << 5);
            gp.rx = (int16_t)(data.axis_rx << 6);
            gp.ry = (int16_t)(data.axis_ry << 6);
            gp.rz = (int16_t)(data.throttle << 5);
            // log_info("Gamepad: [%05lX] (%6d %6d) (%6d %6d) (%6d %6d)", gp.buttons, gp.lx, gp.ly, gp.lz, gp.rx, gp.ry,
            //          gp.rz);
            if (state == State::Active) {
                // Requires little endian
                char buffer[64];
                char* x = buffer;

                x += sprintf(x, ">gamepad_data,%lu,", event.gamepad_data.gamepad_id);
                for (size_t i = 0; i < sizeof(gp); i++) {
                    x += sprintf(x, "%02x", ((uint8_t*)(&gp))[i]);
                }
                x += sprintf(x, "\n");
                int len = x - buffer;
                UsbWriteHandheldData((uint8_t*)buffer, len);
            }
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

}  // namespace

void InitCore() {
    event_queue = xQueueCreate(kEventQueueLength, sizeof(Event));

    TaskHandle_t task_handle;
    xTaskCreate(CoreTask, "core", kStackSize, NULL, static_cast<UBaseType_t>(TaskPriority::kCore), &task_handle);
}

void PostEvent(const Event& event) {
    auto result = xQueueSendToBack(event_queue, &event,
                                   /* xTicksToWait= */ 0);
    if (result != pdPASS) {
        log_error("Failed to post event");
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