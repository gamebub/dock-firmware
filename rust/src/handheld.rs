use crate::{
    gamepad::{Gamepad, GamepadData, GamepadId},
    led::{self, LedSession},
    sys, usb_host,
};

const REQ_GET_INFO: u8 = 0;
const REQ_DOCK_BEGIN: u8 = 3;
const REQ_GAMEPAD_CONNECT: u8 = 4;
const REQ_GAMEPAD_DISCONNECT: u8 = 5;
const REQ_GAMEPAD_DATA: u8 = 6;

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
        usb_host::handheld_control_out(REQ_GAMEPAD_CONNECT, 0, 0, &buf);
    }

    pub fn handle_gamepad_disconnected(&mut self, id: GamepadId) {
        let mut buf = [0u8; 4];
        buf[0..4].copy_from_slice(&id.0.to_le_bytes());
        usb_host::handheld_control_out(REQ_GAMEPAD_DISCONNECT, 0, 0, &buf);
    }

    pub fn handle_gamepad_data(&mut self, id: GamepadId, data: &GamepadData) {
        // TODO: rate limiting
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
                let mut data = [0u8; 16];
                data[4..8].copy_from_slice(&crate::info::SerialNumber::get().0.to_le_bytes());
                data[8..12]
                    .copy_from_slice(&crate::info::HardwareVersion::get().as_u32().to_le_bytes());
                data[12..16]
                    .copy_from_slice(&crate::info::FirmwareVersion::get().as_u32().to_le_bytes());
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
