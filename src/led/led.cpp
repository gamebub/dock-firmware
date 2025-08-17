#include "led/led.h"

#include <array>
#include <cmath>
#include <cstdint>

#include "FreeRTOS.h"
#include "hardware/pwm.h"
#include "log/log.h"
#include "pico/stdlib.h"
#include "priorities.h"
#include "queue.h"
#include "task.h"

namespace {

constexpr size_t kStackSize = 2 * 1024;
constexpr uint32_t kEventQueueLength = 8;

QueueHandle_t event_queue = nullptr;

struct LedEvent {
    LedState state;
    bool set;
};

void LedSetColor(LedColor color) {
    // TODO handle RGB
}

void LedTask(void*) {
    while (1) {
        LedSetColor(LedColor(255, 0, 0));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        LedSetColor(LedColor(0, 255, 0));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        LedSetColor(LedColor(0, 0, 255));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void PostLedEvent(LedState state, bool set) {
    LedEvent event{
        .state = state,
        .set = set,
    };

    auto result = xQueueSendToBack(event_queue, &event, /* xTicksToWait= */ 0);
    if (result != pdPASS) {
        log_error("Failed to post led event");
    }
}

}  // namespace

void InitLed() {
    // TODO: set up RGB led

    // Set up LED event queue
    event_queue = xQueueCreate(kEventQueueLength, sizeof(LedEvent));

    TaskHandle_t task_handle;
    xTaskCreate(LedTask, "led", kStackSize, NULL, static_cast<UBaseType_t>(TaskPriority::kLed), &task_handle);
}

void SetLedState(LedState state) {
    PostLedEvent(state, true);
}

void UnsetLedState(LedState state) {
    PostLedEvent(state, false);
}