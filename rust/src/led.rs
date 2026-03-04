use crate::sys;

#[allow(unused)]
#[repr(u32)]
#[derive(Copy, Clone, Debug)]
pub enum LedState {
    None = 0,
    Standby = 1,
    BluetoothPairing = 2,
    DockActive = 3,
    Dfu = 4,
}

/// Begin an LED session that ends automatically when dropped.
pub fn begin(state: LedState) -> LedSession {
    set(state);
    LedSession(state)
}

pub fn set(state: LedState) {
    unsafe { sys::SetLedState(state as u32) };
}

pub fn unset(state: LedState) {
    unsafe { sys::UnsetLedState(state as u32) };
}

pub struct LedSession(LedState);

impl Drop for LedSession {
    fn drop(&mut self) {
        unset(self.0)
    }
}
