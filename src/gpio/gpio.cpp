#include "gpio/gpio.h"

#include "hardware/gpio.h"
#include "rust_api.h"

bool GpioPollButton()
{
    return !gpio_get(PIN_BUTTON);
}

void GpioCallback(unsigned int gpio, uint32_t events)
{
    static_cast<void>(events);
    if (gpio == PIN_BUTTON) {
        rust_gpio_button_isr();
    }
}

void InitGpio()
{
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
    GpioSetHdmiActive(false);
}

void GpioSetHdmiActive(bool active)
{
    // nOE 1: inactive, 0: active
    gpio_put(PIN_HDMI_OE_N, !active);
    // VBIAS 1: ties to VDD (active). VBIAS 0: ties to ground (inactive).
    gpio_put(PIN_HDMI_VBIAS, active);
    // HDMI 5V EN: 1 active
    gpio_put(PIN_HDMI_5V_EN, active);
}