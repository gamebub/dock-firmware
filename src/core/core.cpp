#include "core/core.h"

#include <cstdint>
#include <cstdio>
#include <string_view>

#include "FreeRTOS.h"
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

constexpr size_t kStackSize = 8 * 1024;
constexpr uint32_t kEventQueueLength = 32;

QueueHandle_t event_queue = nullptr;
State state = State::Idle;

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
            HandleHandheldResponse(std::string_view((char*)&event.handheld_rx_data.data, event.handheld_rx_data.len));
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