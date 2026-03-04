use core::sync::atomic::{AtomicU32, Ordering};

static NEXT_ID: AtomicU32 = AtomicU32::new(1);

#[allow(unused)]
pub mod buttons {
    pub const A: u32 = 1 << 0;
    pub const B: u32 = 1 << 1;
    pub const X: u32 = 1 << 2;
    pub const Y: u32 = 1 << 3;
    pub const UP: u32 = 1 << 4;
    pub const DOWN: u32 = 1 << 5;
    pub const RIGHT: u32 = 1 << 6;
    pub const LEFT: u32 = 1 << 7;
    pub const SYSTEM: u32 = 1 << 8;
    pub const SELECT: u32 = 1 << 9;
    pub const START: u32 = 1 << 10;
    pub const CAPTURE: u32 = 1 << 11;
    pub const L1: u32 = 1 << 12;
    pub const R1: u32 = 1 << 13;
    pub const L2: u32 = 1 << 14;
    pub const R2: u32 = 1 << 15;
    pub const L3: u32 = 1 << 16;
    pub const R3: u32 = 1 << 17;
}

#[derive(Debug)]
pub struct Gamepad {
    pub id: GamepadId,
    pub kind: u32,
    pub device_id: [u8; 8],
    pub wired: bool,
}

#[repr(C, packed)]
#[derive(Copy, Clone, Debug)]
pub struct GamepadData {
    buttons: u32,
    lx: i16,
    ly: i16,
    lz: u16,
    rx: i16,
    ry: i16,
    rz: u16,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct GamepadId(pub u32);

impl GamepadId {
    pub fn allocate() -> Self {
        Self(NEXT_ID.fetch_add(1, Ordering::SeqCst))
    }

    pub fn as_u32(self) -> u32 {
        self.0
    }
}
