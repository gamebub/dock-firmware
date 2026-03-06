#include "led/led.h"

#include "ws2812.pio.h"

namespace {
PIO led_pio;
uint led_pio_sm;
} // namespace

void InitLed()
{
    // Set up RGB led
    uint offset;
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &led_pio, &led_pio_sm, &offset,
        PIN_LED, 1, true);
    hard_assert(success);
    ws2812_program_init(led_pio, led_pio_sm, offset, PIN_LED, 800000, false);
}

void LedSetColor(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t c = ((uint32_t)(r) << 16) | ((uint32_t)(g) << 24) | ((uint32_t)(b) << 8);
    pio_sm_put_blocking(led_pio, led_pio_sm, c);
}