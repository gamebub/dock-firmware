#include "gpio/gpio.h"

#include <cstdio>

#include "FreeRTOS.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "timers.h"
#include "rust_api.h"

namespace {
constexpr TickType_t kDebounceInterval = pdMS_TO_TICKS(10);
constexpr TickType_t kShortPressTime = pdMS_TO_TICKS(1000);
constexpr TickType_t kLongPressTime = pdMS_TO_TICKS(3000);
}  // namespace

static TimerHandle_t button_debounce_timer;
static TimerHandle_t button_long_press_timer;
static bool last_button_state = false;
static TickType_t last_button_time = 0;

bool PollButton() {
    return !gpio_get(PIN_BUTTON);
}

void ButtonDebounceTask(TimerHandle_t) {
    bool button_state = PollButton();
    if (button_state != last_button_state) {
        // Detect a button release
        if (last_button_state && !button_state) {
            TickType_t elapsed = xTaskGetTickCount() - last_button_time;
            if (elapsed <= kShortPressTime) {
                rust_event_button_short();
            }
        }

        last_button_state = button_state;
        last_button_time = xTaskGetTickCount();
    }
}

void ButtonLongPressTask(TimerHandle_t) {
    if (PollButton()) {
        // Button is still pressed after this time.
        rust_event_button_long();
    }
}

void GpioCallback(unsigned int gpio, uint32_t events) {
    static_cast<void>(events);
    if (gpio == PIN_BUTTON) {
        xTimerResetFromISR(button_debounce_timer, nullptr);
        xTimerResetFromISR(button_long_press_timer, nullptr);
    }
}

void InitGpio() {
    // Configure the button and the button interrupt.
    gpio_init(PIN_BUTTON);
    gpio_pull_up(PIN_BUTTON);
    gpio_set_irq_enabled_with_callback(PIN_BUTTON, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &GpioCallback);

    // Enable HDMI chip
    gpio_init(PIN_HDMI_OE_N);
    gpio_init(PIN_HDMI_VBIAS);
    gpio_init(PIN_HDMI_5V_EN);
    gpio_set_dir(PIN_HDMI_OE_N, GPIO_OUT);
    gpio_set_dir(PIN_HDMI_VBIAS, GPIO_OUT);
    gpio_set_dir(PIN_HDMI_5V_EN, GPIO_OUT);
    SetHdmiActive(false);

    // Setup debounce timer.
    button_debounce_timer =
        xTimerCreate("button_debounce", kDebounceInterval, /* uxAutoReload= */ false, nullptr, ButtonDebounceTask);
    button_long_press_timer =
        xTimerCreate("button_longpress", kLongPressTime, /* uxAutoReload= */ false, nullptr, ButtonLongPressTask);
}

void SetHdmiActive(bool active) {
    // nOE 1: inactive, 0: active
    gpio_put(PIN_HDMI_OE_N, !active);
    // VBIAS 1: ties to VDD (active). VBIAS 0: ties to ground (inactive).
    gpio_put(PIN_HDMI_VBIAS, active);
    // HDMI 5V EN: 1 active
    gpio_put(PIN_HDMI_5V_EN, active);
}