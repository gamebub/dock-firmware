#include "led/led.h"

#include <cmath>
#include <cstdint>

#include "FreeRTOS.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "timers.h"

static TimerHandle_t led_timer;
static volatile LedBehavior led_behavior = LedBehavior::kOff;

void LedTimerTask(TimerHandle_t) {
    static uint32_t time_base = 0;
    static LedBehavior last_behavior = LedBehavior::kOff;

    LedBehavior behavior = led_behavior;
    uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());

    if (behavior != last_behavior) {
        // Reset time base for animation
        time_base = now;
        last_behavior = behavior;
    }
    uint32_t elapsed = now - time_base;

    uint8_t level = 0;
    switch (led_behavior) {
        case LedBehavior::kOff: {
            level = 0;
            break;
        }
        case LedBehavior::kOn: {
            level = 255;
            break;
        }
        case LedBehavior::kBlinkSlow: {
            level = (elapsed & 0x200) ? 255 : 0;
            break;
        }
        case LedBehavior::kBlinkFast: {
            level = (elapsed & 0x80) ? 255 : 0;
            break;
        }
        case LedBehavior::kBreatheSlow: {
            level = (uint8_t)(255 * 0.5f * (sinf((float)elapsed / 512.0f) + 1.0f));
            break;
        }
        case LedBehavior::kBreatheFast: {
            level = (uint8_t)(255 * 0.5f * (sinf((float)elapsed / 128.0f) + 1.0f));
            break;
        }
        default:
            break;
    }

    // Non-linear PWM to perceive linear brightness.
    pwm_set_gpio_level(PIN_LED, level * level);
}

void InitLed() {
    // Set up LED: PWM wraps at 2**16-1.
    gpio_set_function(PIN_LED, GPIO_FUNC_PWM);
    uint32_t slice_num = pwm_gpio_to_slice_num(PIN_LED);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f);
    pwm_init(slice_num, &config, true);

    // Set up 100 Hz LED timer
    led_timer = xTimerCreate("led", pdMS_TO_TICKS(10), /* uxAutoReload= */ true, nullptr, LedTimerTask);
    xTimerStart(led_timer, 0);
}

void SetLedBehavior(LedBehavior behavior) {
    // Aligned 32-bit writes should be atomic.
    led_behavior = behavior;
}