use crate::engine;

#[unsafe(no_mangle)]
pub extern "C" fn rust_event_button_short() {
    engine::send(engine::Message::ButtonShortPress);
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_event_button_long() {
    engine::send(engine::Message::ButtonLongPress);
}
