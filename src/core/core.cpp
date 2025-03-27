#include "core/core.h"

#include <cstdint>
#include <cstdio>

#include "FreeRTOS.h"
#include "priorities.h"
#include "queue.h"
#include "task.h"
#include "usb_host/usb_host.h"

namespace {
constexpr size_t kStackSize = 8 * 1024;
constexpr uint32_t kEventQueueLength = 32;

QueueHandle_t event_queue = nullptr;

void HandleEvent(const Event& event) {
    switch (event.type) {
        case EventType::kHandheldMount: {
            printf("** Handheld mount\n");

            char data[] = "\n>get_hwinfo\n";
            UsbWriteHandheldData((uint8_t*)data, sizeof(data));
            break;
        }
        case EventType::kHandheldUnmount:
            printf("** Handheld unmount\n");
            break;
        case EventType::kHandheldRxData: {
            printf("response: %.*s\n", event.handheld_rx_data.len, event.handheld_rx_data.data);
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
        printf("Failed to post event\n");
    }
}