#include "led/led.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "FreeRTOS.h"
#include "hardware/pwm.h"
#include "log/log.h"
#include "pico/stdlib.h"
#include "priorities.h"
#include "queue.h"
#include "task.h"
#include "ws2812.pio.h"

namespace {

struct StateEntry {
    LedState state;
    LedBehavior behavior;
    /// Priority, higher value is higher priority.
    uint32_t priority;
    /// Whether this is a self-terminating event that represents an event that occured.
    bool transient = false;
};

constexpr size_t kStackSize = 2 * 1024;
constexpr uint32_t kEventQueueLength = 8;
constexpr float kLedGamma = 2.2f;

// State to behavior mapping, should be sorted from highest to lowest priority.
constexpr auto kStateMap = std::to_array<StateEntry>({
    StateEntry{
        .state = LedState::kDfu,
        .behavior =
            LedBehavior{
                .pattern = LedPattern::kSolid,
                .color = LedColor(128, 0, 128),
            },
        .priority = 4,
    },
    StateEntry{
        .state = LedState::kBluetoothPairing,
        .behavior =
            LedBehavior{
                .pattern = LedPattern::kBlink,
                .color = LedColor(0, 0, 255),
                .period_ms = 500,
            },
        .priority = 3,
    },
    StateEntry{
        .state = LedState::kDockActive,
        .behavior =
            LedBehavior{
                .pattern = LedPattern::kSolid,
                .color = LedColor(150, 150, 150),
            },
        .priority = 2,
    },
    StateEntry{
        .state = LedState::kStandby,
        .behavior =
            LedBehavior{
                .pattern = LedPattern::kSolid,
                .color = LedColor(32, 0, 0),
            },
        .priority = 1,
    },
});

QueueHandle_t event_queue = nullptr;
std::array<uint32_t, static_cast<size_t>(LedState::kCount)> states{};
LedState active_state = LedState::kNone;

PIO led_pio;
uint led_pio_sm;

struct LedEvent {
    LedState state;
    bool set;
};

void LedSetColor(LedColor color, float scale = 1.0f) {
    float r = powf((color.r / 255.0f * scale), kLedGamma);
    float g = powf((color.g / 255.0f * scale), kLedGamma);
    float b = powf((color.b / 255.0f * scale), kLedGamma);

    uint32_t c = ((uint32_t)(r * 255.0f) << 16) | ((uint32_t)(g * 255.0f) << 24) | ((uint32_t)(b * 255.0f) << 8);
    pio_sm_put_blocking(led_pio, led_pio_sm, c);
}

const StateEntry* GetEntryForState(LedState state) {
    for (auto& e : kStateMap) {
        if (e.state == state) {
            return &e;
        }
    }
    return nullptr;
}

void FindNewState() {
    active_state = LedState::kNone;
    for (auto entry : kStateMap) {
        auto& count = states[static_cast<size_t>(entry.state)];
        if (count) {
            active_state = entry.state;
            return;
        }
    }
}

// Handle an led state update, returns true if the active state changed.
bool HandleNewLedState(LedState state, bool set) {
    auto entry = GetEntryForState(state);
    if (!entry) {
        return false;
    }
    auto current_entry = GetEntryForState(active_state);
    auto& state_count = states[static_cast<size_t>(state)];

    if (entry->transient && set) {
        // Transient events are not queued.
        if (current_entry == nullptr || current_entry->priority < entry->priority) {
            active_state = state;
            return true;
        }
    } else if (!entry->transient) {
        if (set) {
            state_count++;
            if (state_count == 1) {
                // newly set state!
                if (current_entry == nullptr || current_entry->priority < entry->priority) {
                    active_state = state;
                    return true;
                }
            }
        } else {
            if (state_count == 0) {
                log_error("led state underflow, state=%lu", static_cast<uint32_t>(state));
                return false;
            }
            state_count--;
            if (state_count == 0 && active_state == state) {
                // No longer active state!
                FindNewState();
                return true;
            }
        }
    }
    return false;
}

// Sleep and watch for events, returning true if woken up early.
bool LedTaskSleep(uint32_t ms) {
    TickType_t deadline = xTaskGetTickCount() + (ms / portTICK_PERIOD_MS);

    while (true) {
        TickType_t now = xTaskGetTickCount();
        if (now >= deadline) {
            return false;
        }
        TickType_t remaining = deadline - now;

        LedEvent event{};
        auto result = xQueueReceive(event_queue, &event, remaining);
        if (result == pdPASS) {
            bool new_state = HandleNewLedState(event.state, event.set);
            if (new_state) {
                return true;
            }
        }
    }
}

// Indefinitely wait for next event.
void LedWaitForEvent() {
    LedEvent event{};
    auto result = xQueueReceive(event_queue, &event, portMAX_DELAY);
    if (result == pdPASS) {
        HandleNewLedState(event.state, event.set);
    }
}

// Run a behavior. Returns true if it terminates normally.
bool RunBehavior(LedBehavior behavior) {
    switch (behavior.pattern) {
        case LedPattern::kOff: {
            LedSetColor(LedColor(0, 0, 0));
            LedWaitForEvent();
            return false;  // always interrupted
        }
        case LedPattern::kSolid: {
            LedSetColor(behavior.color);
            LedWaitForEvent();
            return false;  // always interrupted
        }
        case LedPattern::kBlink: {
            while (true) {
                LedSetColor(behavior.color);
                if (LedTaskSleep(behavior.period_ms / 2)) {
                    return false;
                }
                LedSetColor(LedColor(0, 0, 0));
                if (LedTaskSleep(behavior.period_ms / 2)) {
                    return false;
                }

                if (behavior.repeat == 0) {
                    continue;
                } else if (behavior.repeat == 1) {
                    return true;
                }
                behavior.repeat--;
            }
        }
        case LedPattern::kBreathe: {
            while (true) {
                auto start = xTaskGetTickCount();
                while (true) {
                    float elapsed_ms = static_cast<float>((xTaskGetTickCount() - start)) * portTICK_PERIOD_MS;
                    float t = elapsed_ms / behavior.period_ms;
                    if (t >= 1.0) {
                        LedSetColor(LedColor(0, 0, 0));
                        break;
                    }
                    float intensity = (-cosf(t * 2.0f * std::numbers::pi) + 1.0f) / 2.0f;
                    LedSetColor(behavior.color, intensity);
                    if (LedTaskSleep(/* ms= */ 50)) {
                        return false;
                    }

                    if (behavior.repeat == 0) {
                        continue;
                    } else if (behavior.repeat == 1) {
                        return true;
                    }
                    behavior.repeat--;
                }
            }
        }
    }

    return true;
}

void LedTask(void*) {
    while (true) {
        if (active_state == LedState::kNone) {
            // Wait for a state.
            LedWaitForEvent();
            continue;
        } else {
            auto entry = GetEntryForState(active_state);
            if (!entry) {
                // This shouldn't happen.
                active_state = LedState::kNone;
                continue;
            }

            bool terminated = RunBehavior(entry->behavior);
            if (terminated) {
                FindNewState();
            }
        }
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
    // Set up RGB led
    uint offset;
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &led_pio, &led_pio_sm, &offset,
                                                                    PIN_LED, 1, true);
    hard_assert(success);
    ws2812_program_init(led_pio, led_pio_sm, offset, PIN_LED, 800000, false);

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