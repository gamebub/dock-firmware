use core::{
    sync::atomic::{AtomicBool, AtomicU32, Ordering},
    time::Duration as CoreDuration,
};

use freertos_rust::{Duration, FreeRtosUtils, InterruptContext, Timer};

use crate::{engine, util::InitCell};

const DURATION_DEBOUNCE: CoreDuration = CoreDuration::from_millis(10);
const DURATION_SHORT: CoreDuration = CoreDuration::from_millis(1000);
const DURATION_LONG: CoreDuration = CoreDuration::from_millis(3000);

static TIMER_DEBOUNCE: InitCell<Timer> = InitCell::new();
static TIMER_LONG: InitCell<Timer> = InitCell::new();

static BUTTON_PRESSED: AtomicBool = AtomicBool::new(false);
static BUTTON_TIME: AtomicU32 = AtomicU32::new(0);

fn task_debounce(_: &Timer) {
    let pressed = button_pressed();
    let last_pressed = BUTTON_PRESSED.load(Ordering::Relaxed);

    if pressed != last_pressed {
        if last_pressed && !pressed {
            // Button released
            let elapsed = FreeRtosUtils::get_tick_count() - BUTTON_TIME.load(Ordering::Relaxed);
            if Duration::ticks(elapsed).to_ms() < DURATION_SHORT.as_millis() as u32 {
                engine::send(engine::Message::ButtonShortPress);
            }
        }

        BUTTON_TIME.store(FreeRtosUtils::get_tick_count(), Ordering::Relaxed);
        BUTTON_PRESSED.store(pressed, Ordering::Relaxed);
    }
}

fn task_long(_: &Timer) {
    if button_pressed() {
        // Button is still pressed after this time.
        engine::send(engine::Message::ButtonLongPress);
    }
}

fn button_pressed() -> bool {
    unsafe { crate::sys::GpioPollButton() }
}

pub fn button_isr() {
    let (Some(debounce), Some(long)) = (TIMER_DEBOUNCE.maybe_get(), TIMER_LONG.maybe_get()) else {
        return;
    };

    let mut context = InterruptContext::new();
    debounce.start_from_isr(&mut context).unwrap();
    long.start_from_isr(&mut context).unwrap();
}

pub fn init() {
    TIMER_DEBOUNCE.set(
        Timer::new(Duration::ms(DURATION_DEBOUNCE.as_millis() as u32))
            .set_auto_reload(false)
            .set_name("button debounce")
            .create(task_debounce)
            .unwrap(),
    );
    TIMER_LONG.set(
        Timer::new(Duration::ms(DURATION_LONG.as_millis() as u32))
            .set_auto_reload(false)
            .set_name("button long")
            .create(task_long)
            .unwrap(),
    );
}
