use alloc::vec::Vec;
use freertos_rust::{Duration, Queue, Task};
use static_cell::StaticCell;

use crate::bluetooth::BluetoothPairingSession;
use crate::gamepad::{Gamepad, GamepadData, GamepadId};
use crate::util::InitCell;
use crate::{led, sys, usb_host};

static QUEUE: InitCell<Queue<Message>> = InitCell::new();
static ENGINE: StaticCell<Engine> = StaticCell::new();

const REQ_GET_INFO: u8 = 0;
const REQ_DOCK_BEGIN: u8 = 3;
const REQ_GAMEPAD_CONNECT: u8 = 4;
const REQ_GAMEPAD_DISCONNECT: u8 = 5;
const REQ_GAMEPAD_DATA: u8 = 6;

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

#[derive(Copy, Clone, PartialEq)]
enum HandheldState {
    /// Handheld is not connected.
    Idle,
    /// Sent info request, waiting for response.
    WaitForGetInfo,
    /// Sent dock begin, waiting for response.
    WaitForDockBegin,
    /// Docking is active.
    Active,
    /// Handheld is connected in an error state.
    Error,
}

/// Main state machine
struct Engine {
    bluetooth_pairing: Option<BluetoothPairingSession>,
    handheld: HandheldState,
    gamepads: Vec<Gamepad>,
}

impl Engine {
    fn new() -> Self {
        Engine {
            bluetooth_pairing: None,
            handheld: HandheldState::Idle,
            gamepads: Vec::new(),
        }
    }

    fn handle(&mut self, message: Message) {
        match message {
            Message::HandheldMount => {
                if self.handheld != HandheldState::Idle {
                    log::error!("Unexpected handheld mount");
                    return;
                }
                log::info!("Handheld mount");
                usb_host::handheld_control_in(REQ_GET_INFO, 0, 0, 16);
                self.handheld = HandheldState::WaitForGetInfo;
            }
            Message::HandheldUnmount => {
                log::info!("Handheld unmount");
                if self.handheld == HandheldState::Active {
                    led::unset(led::LedState::DockActive);
                }
                unsafe { sys::SetHdmiActive(false) };
                self.handheld = HandheldState::Idle;
            }
            Message::HandheldXferComplete(result) => self.handle_handheld_xfer(result),
            Message::GamepadConnected(g) => {
                log::info!("Gamepad connected: id={}", g.id.as_u32());
                self.gamepads.push(g);

                if self.handheld == HandheldState::Active {
                    self.write_gamepad_connected(self.gamepads.last().unwrap());
                }

                self.bluetooth_pairing = None;
            }
            Message::GamepadDisconnected(id) => {
                log::info!("Gamepad disconnected: id={}", id.as_u32());
                self.gamepads.retain(|x| x.id != id);

                if self.handheld == HandheldState::Active {
                    let mut buf = [0u8; 4];
                    buf[0..4].copy_from_slice(&id.0.to_le_bytes());
                    usb_host::handheld_control_out(REQ_GAMEPAD_DISCONNECT, 0, 0, &buf);
                }
            }
            Message::GamepadData(id, data) => {
                // TODO rate limiting
                if self.handheld == HandheldState::Active {
                    let mut buf = [0u8; 20];
                    buf[0..4].copy_from_slice(&id.0.to_le_bytes());
                    buf[4..20].copy_from_slice(unsafe {
                        core::slice::from_raw_parts(
                            (&data) as *const _ as *const u8,
                            core::mem::size_of::<GamepadData>(),
                        )
                    });
                    usb_host::handheld_control_out(REQ_GAMEPAD_DATA, 0, 0, &buf);
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

    fn handle_handheld_xfer(&mut self, result: usb_host::HandheldXferResult) {
        match (self.handheld, result.request) {
            (HandheldState::WaitForGetInfo, REQ_GET_INFO) => {
                let data = match result.data() {
                    Ok(data) if data.len() >= 16 => data,
                    _ => {
                        log::error!("GetInfo error");
                        self.handheld = HandheldState::Error;
                        return;
                    }
                };

                // Read the Handheld device info.
                let serial = u32::from_le_bytes(data[4..8].try_into().unwrap());
                let hw_version = u32::from_le_bytes(data[8..12].try_into().unwrap());
                let fw_version = u32::from_le_bytes(data[12..16].try_into().unwrap());
                log::info!("-> Serial: {:08X}", serial);
                log::info!("-> HW    : {:08X}", hw_version);
                log::info!("-> FW    : {:08X}", fw_version);

                // Send the Dock device info.
                let mut data = [0u8; 16];
                data[4..8].copy_from_slice(&crate::info::SerialNumber::get().0.to_le_bytes());
                data[8..12]
                    .copy_from_slice(&crate::info::HardwareVersion::get().as_u32().to_le_bytes());
                data[12..16]
                    .copy_from_slice(&crate::info::FirmwareVersion::get().as_u32().to_le_bytes());
                usb_host::handheld_control_out(REQ_DOCK_BEGIN, 0, 0, &data);
                self.handheld = HandheldState::WaitForDockBegin;
            }
            (HandheldState::WaitForDockBegin, REQ_DOCK_BEGIN) => {
                if !result.data().is_ok() {
                    log::error!("Dock begin failure");
                    self.handheld = HandheldState::Error;
                    return;
                }
                log::info!("Dock begin");

                for gamepad in &self.gamepads {
                    self.write_gamepad_connected(gamepad);
                }

                unsafe { sys::SetHdmiActive(true) };
                self.handheld = HandheldState::Active;
                led::set(led::LedState::DockActive);
            }
            _ => {
                if !result.data().is_ok() {
                    log::warn!("Xfer (req={}) failed", result.request);
                }
            }
        }
    }

    fn write_gamepad_connected(&self, gamepad: &Gamepad) {
        // 4 byte: slot
        // 4 byte: reserved
        // 8 byte: gamepad device ID
        // 32 byte: model name (\0 terminated)
        let mut buf = [0u8; 48];
        buf[0..4].copy_from_slice(&gamepad.id.0.to_le_bytes());
        buf[8..16].copy_from_slice(&gamepad.device_id);
        // TODO gamepad model name
        usb_host::handheld_control_out(REQ_GAMEPAD_CONNECT, 0, 0, &buf);
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
