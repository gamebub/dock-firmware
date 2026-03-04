use crate::led;
use crate::sys;

pub struct BluetoothPairingSession(#[allow(unused)] led::LedSession);

impl BluetoothPairingSession {
    pub fn begin() -> Self {
        unsafe { sys::BluetoothEnablePairing(true) };
        BluetoothPairingSession(led::begin(led::LedState::BluetoothPairing))
    }
}

impl Drop for BluetoothPairingSession {
    fn drop(&mut self) {
        unsafe { sys::BluetoothEnablePairing(false) };
    }
}
