use crate::{
    engine,
    gamepad::{self, Gamepad, GamepadData, GamepadId},
    led,
    usb_host::HandheldXferResult,
};

#[unsafe(no_mangle)]
pub extern "C" fn rust_gamepad_allocate_id() -> u32 {
    gamepad::GamepadId::allocate().as_u32()
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_event_gamepad_connected(
    id: u32,
    kind: u32,
    device_id_ptr: *const u8,
    wired: bool,
) {
    let device_id = unsafe { core::slice::from_raw_parts(device_id_ptr, 8) };
    let gp = Gamepad {
        id: GamepadId(id),
        kind,
        device_id: device_id.try_into().unwrap(),
        wired,
    };
    engine::send(engine::Message::GamepadConnected(gp));
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_event_gamepad_data(id: u32, data: GamepadData) {
    engine::send(engine::Message::GamepadData(GamepadId(id), data));
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_event_gamepad_disconnected(id: u32) {
    engine::send(engine::Message::GamepadDisconnected(GamepadId(id)));
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_event_handheld_mount() {
    engine::send(engine::Message::HandheldMount);
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_event_handheld_unmount() {
    engine::send(engine::Message::HandheldUnmount);
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_event_handheld_xfer_complete(
    request: u8,
    success: bool,
    tag: usize,
    data: *const u8,
    data_len: usize,
) {
    let data = unsafe { core::slice::from_raw_parts(data, data_len) };
    let result = HandheldXferResult::new(request, tag, success, data);
    engine::send(engine::Message::HandheldXferComplete(result));
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_info_get_for_usb(buffer: *mut u8, len: usize) {
    let buffer = unsafe { core::slice::from_raw_parts_mut(buffer, len) };
    crate::info::get_info_for_usb(buffer);
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_info_serial_number() -> u32 {
    crate::info::SerialNumber::get().0
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_info_hardware_version() -> u32 {
    crate::info::HardwareVersion::get().as_u32()
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_info_chip_id() -> u64 {
    crate::info::get_chip_id()
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_led_set_dfu() {
    led::set(led::LedState::Dfu);
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_gpio_button_isr() {
    crate::gpio::button_isr();
}
