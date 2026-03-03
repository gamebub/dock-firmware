use freertos_rust::{Duration, Queue, Task};
use static_cell::StaticCell;

use crate::gamepad::{Gamepad, GamepadData, GamepadId};
use crate::util::InitCell;
use crate::{led, sys, usb_host};

static QUEUE: InitCell<Queue<Message>> = InitCell::new();
static ENGINE: StaticCell<Engine> = StaticCell::new();

pub enum Message {
    /// A possibly handheld device has mounted.
    HandheldMount,
    /// The handheld device has disconnected.
    HandheldUnmount,
    /// Handheld control transfer complete
    HandheldXferComplete(usb_host::HandheldXferResult),

    /// A gamepad has connected
    GamepadConnected(Gamepad),
    /// A gamepad has disconnected
    GamepadDisconnected(GamepadId),
    /// New gamepad data is received
    GamepadData(GamepadId, GamepadData),

    ButtonShortPress,
    ButtonLongPress,
}

/// Main state machine
struct Engine {
    bluetooth_pairing: bool,
}

impl Engine {
    fn new() -> Self {
        Engine {
            bluetooth_pairing: false,
        }
    }

    fn handle(&mut self, message: Message) {
        match message {
            Message::HandheldMount => log::info!("Handheld mount"),
            Message::HandheldUnmount => log::info!("Handheld unmount"),
            Message::HandheldXferComplete(_) => (),
            Message::GamepadConnected(g) => {
                log::info!("Gamepad connected: id={}", g.id.as_u32());
                if self.bluetooth_pairing {
                    self.bluetooth_pairing = false;
                    led::unset(led::LedState::BluetoothPairing);
                    unsafe { sys::BluetoothEnablePairing(false) };
                }
            }
            Message::GamepadDisconnected(id) => {
                log::info!("Gamepad disconnected: id={}", id.as_u32());
            }
            Message::GamepadData(id, data) => {
                log::info!("Gamepad data: {:?}", data);
            }
            Message::ButtonShortPress => log::info!("Button short press"),
            Message::ButtonLongPress => {
                self.bluetooth_pairing = !self.bluetooth_pairing;
                if self.bluetooth_pairing {
                    log::info!("Enter pairing mode");
                    led::set(led::LedState::BluetoothPairing);
                } else {
                    log::info!("Exit pairing mode");
                    led::unset(led::LedState::BluetoothPairing);
                }
                unsafe { sys::BluetoothEnablePairing(self.bluetooth_pairing) };
            }
        }
    }
}

fn task(_task: Task) {
    let state = ENGINE.init(Engine::new());
    loop {
        let message = QUEUE.get().receive(Duration::infinite()).unwrap();
        state.handle(message);
    }
}

pub fn send(message: Message) {
    if let Err(_) = QUEUE.get().send(message, Duration::zero()) {
        log::warn!("Failed to post message");
    }
}

pub fn start_task() {
    QUEUE.set(Queue::new(32).unwrap());

    Task::new().name("engine").start(task).unwrap();
}
