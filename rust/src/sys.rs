#![allow(unused)]

unsafe extern "C" {
    pub unsafe fn puts(str: *const u8);
    pub unsafe fn panic(fmt: *const u8, ...) -> !;
}

unsafe extern "C" {
    pub unsafe fn BluetoothEnablePairing(enable: bool);

    pub unsafe fn SetLedState(state: u32);
    pub unsafe fn UnsetLedState(state: u32);
}
