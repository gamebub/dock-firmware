use alloc::vec::Vec;
use freertos_rust::{Duration, Queue, Task};
use static_cell::StaticCell;

use crate::bluetooth::BluetoothPairingSession;
use crate::gamepad::{Gamepad, GamepadData, GamepadId};
use crate::handheld::Handheld;
use crate::usb_host;
use crate::util::InitCell;

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
    bluetooth_pairing: Option<BluetoothPairingSession>,
    handheld: Option<Handheld>,
    gamepads: Vec<Gamepad>,
}

impl Engine {
    fn new() -> Self {
        Engine {
            bluetooth_pairing: None,
            handheld: None,
            gamepads: Vec::new(),
        }
    }

    fn handle(&mut self, message: Message) {
        match message {
            Message::HandheldMount => {
                if self.handheld.is_some() {
                    log::error!("Unexpected handheld mount");
                    return;
                }
                log::info!("Handheld mount");
                let mut handheld = Handheld::new();
                handheld.handle_mount();
                self.handheld = Some(handheld);
            }
            Message::HandheldUnmount => {
                log::info!("Handheld unmount");
                if let Some(handheld) = self.handheld.as_mut() {
                    handheld.handle_unmount();
                }
                self.handheld = None;
            }
            Message::HandheldXferComplete(result) => {
                if let Some(handheld) = self.handheld.as_mut() {
                    let was_active = handheld.active();
                    handheld.handle_xfer_complete(result);

                    if handheld.active() && !was_active {
                        for gamepad in &self.gamepads {
                            handheld.handle_gamepad_connected(gamepad);
                        }
                    }
                }
            }
            Message::GamepadConnected(g) => {
                log::info!("Gamepad connected: id={}", g.id.as_u32());
                if !g.wired {
                    self.bluetooth_pairing = None;
                }
                self.gamepads.push(g);
                if let Some(handheld) = self.handheld.as_mut() {
                    handheld.handle_gamepad_connected(self.gamepads.last().unwrap());
                }
            }
            Message::GamepadDisconnected(id) => {
                log::info!("Gamepad disconnected: id={}", id.as_u32());
                self.gamepads.retain(|x| x.id != id);

                if let Some(handheld) = self.handheld.as_mut() {
                    handheld.handle_gamepad_disconnected(id);
                }
            }
            Message::GamepadData(id, data) => {
                if let Some(handheld) = self.handheld.as_mut() {
                    handheld.handle_gamepad_data(id, &data);
                }
            }
            Message::ButtonShortPress => log::info!("Button short press"),
            Message::ButtonLongPress => {
                self.bluetooth_pairing = match self.bluetooth_pairing {
                    Some(_) => None,
                    None => Some(BluetoothPairingSession::begin()),
                };
                log::info!(
                    "Bluetooth pairing active: {}",
                    self.bluetooth_pairing.is_some()
                );
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
