use alloc::collections::vec_deque::VecDeque;
use freertos_rust::{Duration, Mutex};

use crate::{
    gamepad::{Gamepad, GamepadData, GamepadId},
    led::{self, LedSession},
    sys, usb_host,
    util::InitCell,
};

const REQ_GET_INFO: u8 = 0;
const REQ_DOCK_BEGIN: u8 = 3;
const REQ_GAMEPAD_CONNECT: u8 = 4;
const REQ_GAMEPAD_DISCONNECT: u8 = 5;
const REQ_GAMEPAD_DATA: u8 = 6;

pub(crate) static TX_PACKET_QUEUE: InitCell<PacketQueue> = InitCell::new();

/// Docked handheld state manager
pub struct Handheld {
    state: HandheldState,
    docked_led: Option<LedSession>,
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

impl Handheld {
    pub fn new() -> Self {
        TX_PACKET_QUEUE.get().reset();

        Handheld {
            state: HandheldState::Idle,
            docked_led: None,
        }
    }

    pub fn active(&self) -> bool {
        self.state == HandheldState::Active
    }

    pub fn handle_mount(&mut self) {
        assert!(self.state == HandheldState::Idle);
        self.state = HandheldState::WaitForGetInfo;
        usb_host::handheld_control_in(REQ_GET_INFO, 0, 0, 16);
    }

    pub fn handle_unmount(&mut self) {
        TX_PACKET_QUEUE.get().reset();
        unsafe { sys::GpioSetHdmiActive(false) };
    }

    pub fn handle_gamepad_connected(&mut self, gamepad: &Gamepad) {
        // 4 byte: slot
        // 4 byte: reserved
        // 8 byte: gamepad device ID
        // 32 byte: model name (\0 terminated)
        let mut buf = [0u8; 48];
        buf[0..4].copy_from_slice(&gamepad.id.0.to_le_bytes());
        buf[8..16].copy_from_slice(&gamepad.device_id);
        // TODO gamepad model name

        let packet = Packet::new(REQ_GAMEPAD_CONNECT, &buf);
        TX_PACKET_QUEUE.get().push_back(packet);
    }

    pub fn handle_gamepad_disconnected(&mut self, id: GamepadId) {
        let mut buf = [0u8; 4];
        buf[0..4].copy_from_slice(&id.0.to_le_bytes());

        let packet = Packet::new(REQ_GAMEPAD_DISCONNECT, &buf);
        TX_PACKET_QUEUE.get().push_back(packet);
    }

    pub fn handle_gamepad_data(&mut self, id: GamepadId, data: &GamepadData) {
        let mut buf = [0u8; 20];
        buf[0..4].copy_from_slice(&id.0.to_le_bytes());
        buf[4..20].copy_from_slice(unsafe {
            core::slice::from_raw_parts(
                data as *const _ as *const u8,
                core::mem::size_of::<GamepadData>(),
            )
        });

        let packet = Packet::new(REQ_GAMEPAD_DATA, &buf);
        TX_PACKET_QUEUE.get().push_back(packet);
    }

    pub fn handle_xfer_complete(&mut self, result: usb_host::HandheldXferResult) {
        match (self.state, result.request) {
            (HandheldState::WaitForGetInfo, REQ_GET_INFO) => {
                let data = match result.data() {
                    Ok(data) if data.len() >= 16 => data,
                    _ => {
                        log::error!("GetInfo error");
                        self.state = HandheldState::Error;
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
                let mut data = [0u8; 32];
                data[4..8].copy_from_slice(&crate::info::SerialNumber::get().0.to_le_bytes());
                data[8..12]
                    .copy_from_slice(&crate::info::HardwareVersion::get().as_u32().to_le_bytes());
                data[12..16]
                    .copy_from_slice(&crate::info::FirmwareVersion::get().as_u32().to_le_bytes());
                // Human-readable firmware version
                let fw_version = env!("CARGO_PKG_VERSION");
                data[16..32][..fw_version.len().min(16)].copy_from_slice(fw_version.as_bytes());

                usb_host::handheld_control_out(REQ_DOCK_BEGIN, 0, 0, &data);
                self.state = HandheldState::WaitForDockBegin;
            }
            (HandheldState::WaitForDockBegin, REQ_DOCK_BEGIN) => {
                if !result.data().is_ok() {
                    log::error!("Dock begin failure");
                    self.state = HandheldState::Error;
                    return;
                }
                log::info!("Dock begin");

                unsafe { sys::GpioSetHdmiActive(true) };
                self.state = HandheldState::Active;
                self.docked_led = Some(led::begin(led::LedState::DockActive));
            }
            _ => {
                if !result.data().is_ok() {
                    log::warn!("Xfer (req={}) failed", result.request);
                }
            }
        }
    }
}

pub struct Packet {
    id: u8,
    payload: [u8; 60],
    payload_len: u8,
}

impl Packet {
    pub fn new(id: u8, payload: &[u8]) -> Packet {
        assert!(payload.len() <= 60);
        let mut buffer = [0u8; 60];
        buffer[..payload.len()].copy_from_slice(payload);
        Packet {
            id,
            payload: buffer,
            payload_len: payload.len() as u8,
        }
    }

    pub fn payload(&self) -> &[u8] {
        &self.payload[..self.payload_len as usize]
    }

    pub fn encode_to(&self, buf: &mut [u8]) -> usize {
        assert!(buf.len() >= 64);
        let len = 4 + (self.payload_len as usize);
        buf[0] = self.id;
        buf[1] = 0;
        buf[2] = 0;
        buf[3] = 0;
        buf[4..len].copy_from_slice(self.payload());
        len
    }
}

pub struct PacketQueue {
    packets: Mutex<VecDeque<Packet>>,
}

impl PacketQueue {
    pub fn new() -> Self {
        PacketQueue {
            packets: Mutex::new(VecDeque::new()).unwrap(),
        }
    }

    pub fn pop_front(&self) -> Option<Packet> {
        let mut queue = self.packets.lock(Duration::infinite()).unwrap();
        queue.pop_front()
    }

    pub fn push_back(&self, packet: Packet) {
        let mut queue = self.packets.lock(Duration::infinite()).unwrap();

        // If the last packet pushed is a REQ_GAMEPAD_DATA packet, just replace it.
        if packet.id == REQ_GAMEPAD_DATA {
            if let Some(back) = queue.back_mut()
                && back.id == REQ_GAMEPAD_DATA
            {
                *back = packet;
                log::warn!("Dropping gamepad data");
                return;
            }
        }

        queue.push_back(packet);
    }

    pub fn reset(&self) {
        let mut queue = self.packets.lock(Duration::infinite()).unwrap();
        queue.clear();
    }
}

pub fn init() {
    TX_PACKET_QUEUE.set(PacketQueue::new());
}
