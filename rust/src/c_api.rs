use crate::{
    engine,
    gamepad::{self, Gamepad, GamepadData, GamepadId},
};

#[unsafe(no_mangle)]
pub extern "C" fn rust_event_button_short() {
    engine::send(engine::Message::ButtonShortPress);
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_event_button_long() {
    engine::send(engine::Message::ButtonLongPress);
}

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
