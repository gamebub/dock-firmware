#![allow(unused)]

unsafe extern "C" {
    pub unsafe fn puts(str: *const u8);
    pub unsafe fn panic(fmt: *const u8, ...) -> !;
}

unsafe extern "C" {
    pub unsafe fn BluetoothEnablePairing(enable: bool);

    pub unsafe fn SetHdmiActive(active: bool);

    pub unsafe fn SetLedState(state: u32);
    pub unsafe fn UnsetLedState(state: u32);

    pub unsafe fn UsbHandheldControlOut2(
        request: u8,
        value: u16,
        tag: usize,
        data: *const u8,
        data_len: usize,
    );
    pub unsafe fn UsbHandheldControlIn(request: u8, value: u16, tag: usize, length: u16);
}
