use enum_map::{Enum, EnumMap};
use freertos_rust::{Duration, Queue, Task};
use static_cell::StaticCell;

use crate::{sys, util::InitCell};

#[allow(unused)]
#[repr(u32)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Enum)]
pub enum LedState {
    None = 0,
    Standby = 1,
    BluetoothPairing = 2,
    DockActive = 3,
    Dfu = 4,
}

#[derive(Copy, Clone)]
#[repr(C)]
/// LED color: (r, g, b)
struct LedColor(u8, u8, u8);

#[allow(unused)]
#[derive(Copy, Clone)]
enum LedPattern {
    Off,
    Solid,
    Blink,
    Breathe,
}

#[derive(Copy, Clone)]
struct LedBehavior {
    pattern: LedPattern,
    color: LedColor,
    period_ms: Option<u32>,
    repeat: Option<u32>,
}

/// LED states and their behaviors, from highest to lowest priority.
static STATE_MAP: [(LedState, LedBehavior); 4] = [
    (
        LedState::Dfu,
        LedBehavior {
            pattern: LedPattern::Solid,
            color: LedColor(128, 0, 128),
            period_ms: None,
            repeat: None,
        },
    ),
    (
        LedState::BluetoothPairing,
        LedBehavior {
            pattern: LedPattern::Blink,
            color: LedColor(0, 0, 128),
            period_ms: Some(500),
            repeat: None,
        },
    ),
    (
        LedState::DockActive,
        LedBehavior {
            pattern: LedPattern::Solid,
            color: LedColor(128, 128, 128),
            period_ms: None,
            repeat: None,
        },
    ),
    (
        LedState::Standby,
        LedBehavior {
            pattern: LedPattern::Solid,
            // Minimum brightness, with gamma correction (maps to 1)
            color: LedColor(24, 0, 0),
            period_ms: None,
            repeat: None,
        },
    ),
];
static QUEUE: InitCell<Queue<LedMessage>> = InitCell::new();
static CONTROLLER: StaticCell<LedController> = StaticCell::new();

const LED_GAMMA: f32 = 2.2;

struct LedMessage(LedState, bool);

struct LedController {
    queue: &'static Queue<LedMessage>,
    state: LedState,
    counts: EnumMap<LedState, u32>,
}

impl LedController {
    fn new(queue: &'static Queue<LedMessage>) -> Self {
        LedController {
            queue,
            state: LedState::None,
            counts: EnumMap::default(),
        }
    }

    fn run(&mut self) {
        loop {
            if self.state == LedState::None {
                self.wait_for_message(Duration::infinite());
                continue;
            }

            let Some(entry) = STATE_MAP.iter().find(|x| x.0 == self.state) else {
                self.state = LedState::None;
                continue;
            };

            let terminated = self.do_behavior(&entry.1);
            if terminated {
                self.compute_new_state();
            }
        }
    }

    /// Run the given behavior.
    ///
    /// Returns true if the behavior ended normally.
    fn do_behavior(&mut self, behavior: &LedBehavior) -> bool {
        match behavior.pattern {
            LedPattern::Off => {
                self.set_color(LedColor(0, 0, 0), 1.0);
                self.wait_for_message(Duration::infinite());
                return false;
            }
            LedPattern::Solid => {
                self.set_color(behavior.color, 1.0);
                self.wait_for_message(Duration::infinite());
                return false;
            }
            LedPattern::Blink => {
                let half_period = Duration::ms(behavior.period_ms.unwrap_or_default() / 2);
                let mut loops = behavior.repeat.unwrap_or(0);
                loop {
                    self.set_color(behavior.color, 1.0);
                    if self.wait_for_message(half_period) {
                        // Technically, this could have been interrupted by a message that didn't
                        // change the curent state do anything, and we need to wait for longer.
                        // In practice, it probably doesn't really matter.
                        return false;
                    }
                    self.set_color(LedColor(0, 0, 0), 1.0);
                    if self.wait_for_message(half_period) {
                        return false;
                    }

                    if loops > 0 {
                        loops -= 1;
                        if loops == 0 {
                            return true;
                        }
                    }
                }
            }
            LedPattern::Breathe => todo!(),
        }
    }

    fn wait_for_message(&mut self, timeout: Duration) -> bool {
        let result = self.queue.receive(timeout);
        if let Ok(message) = result {
            self.handle_message(message)
        } else {
            false
        }
    }

    /// Handle an LED message, return true if the active state changed.
    fn handle_message(&mut self, message: LedMessage) -> bool {
        let LedMessage(state, set) = message;
        let entry = STATE_MAP.iter().find(|x| x.0 == state);
        let Some(entry) = entry else {
            return false;
        };

        let priority = STATE_MAP.iter().position(|x| x.0 == state).unwrap();
        let current_priority = STATE_MAP.iter().position(|x| x.0 == self.state);

        let transient = entry.1.repeat.is_some();

        if transient && set {
            // Transient events are not queued.
            if current_priority.is_none() || priority < current_priority.unwrap() {
                self.state = state;
                return true;
            }
        } else if !transient {
            if set {
                self.counts[state] += 1;
                if self.counts[state] == 1 {
                    // Newly set state
                    if current_priority.is_none() || priority < current_priority.unwrap() {
                        self.state = state;
                        return true;
                    }
                }
            } else {
                if self.counts[state] == 0 {
                    log::error!("Led state underflow: {:?}", state);
                    return false;
                }
                self.counts[state] -= 1;
                if self.counts[state] == 0 && self.state == state {
                    // No longer active state!
                    self.compute_new_state();
                    return true;
                }
            }
        }

        false
    }

    fn compute_new_state(&mut self) {
        self.state = LedState::None;
        for (state, _) in STATE_MAP {
            if self.counts[state] > 0 {
                self.state = state;
                return;
            }
        }
    }

    fn set_color(&self, color: LedColor, scale: f32) {
        let r = (libm::powf(color.0 as f32 / 255f32 * scale, LED_GAMMA) * 255f32) as u8;
        let g = (libm::powf(color.1 as f32 / 255f32 * scale, LED_GAMMA) * 255f32) as u8;
        let b = (libm::powf(color.2 as f32 / 255f32 * scale, LED_GAMMA) * 255f32) as u8;
        unsafe { sys::LedSetColor(r, g, b) };
    }
}

/// Begin an LED session that ends automatically when dropped.
pub fn begin(state: LedState) -> LedSession {
    set(state);
    LedSession(state)
}

pub fn set(state: LedState) {
    let _ = QUEUE
        .get()
        .send(LedMessage(state, true), Duration::infinite());
}

pub fn unset(state: LedState) {
    let _ = QUEUE
        .get()
        .send(LedMessage(state, false), Duration::infinite());
}

pub struct LedSession(LedState);

impl Drop for LedSession {
    fn drop(&mut self) {
        unset(self.0)
    }
}

fn task(_task: Task) {
    let controller = CONTROLLER.init(LedController::new(QUEUE.get()));
    controller.run();
}

pub fn start_task() {
    QUEUE.set(Queue::new(8).unwrap());
    Task::new().name("led").start(task).unwrap();
}
