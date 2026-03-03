#![allow(unused)]

unsafe extern "C" {
    pub unsafe fn puts(str: *const u8);
    pub unsafe fn panic(fmt: *const u8, ...) -> !;
}

pub const LED_STATE_NONE: u32 = 0;
pub const LED_STATE_STANDBY: u32 = 1;
pub const LED_STATE_BLUETOOTH_PAIRING: u32 = 2;
pub const LED_STATE_DOCK_ACTIVE: u32 = 3;
pub const LED_STATE_DFU: u32 = 4;

unsafe extern "C" {
    pub unsafe fn BluetoothEnablePairing(enable: bool);

    pub unsafe fn SetLedState(state: u32);
    pub unsafe fn UnsetLedState(state: u32);
}
