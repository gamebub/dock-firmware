use core::fmt::{Display, Formatter};

const OTP_DATA_BASE: usize = 0x4013_0000;
const OTP_DATA_LEN: usize = 8192;

fn read_otp_entry(page: usize, row: usize) -> u32 {
    let offset = (page * 0x40 + row) * 2;
    assert!(offset < OTP_DATA_LEN);
    let ptr = OTP_DATA_BASE + offset;
    unsafe { core::ptr::read_volatile(ptr as *const u32) }
}

#[derive(Copy, Clone)]
pub struct SerialNumber(pub u32);

impl SerialNumber {
    pub fn get() -> SerialNumber {
        SerialNumber(read_otp_entry(3, 4))
    }
}

impl Display for SerialNumber {
    fn fmt(&self, f: &mut Formatter) -> core::fmt::Result {
        write!(f, "{:08X}", self.0)
    }
}

#[derive(Copy, Clone)]
pub struct HardwareVersion(u32);

impl HardwareVersion {
    pub fn get() -> HardwareVersion {
        HardwareVersion(read_otp_entry(3, 0))
    }

    pub fn as_u32(&self) -> u32 {
        self.0
    }
}

#[derive(Copy, Clone)]
pub struct FirmwareVersion(u32);

impl FirmwareVersion {
    pub fn get() -> Self {
        // TODO
        FirmwareVersion(0)
    }

    pub fn as_u32(&self) -> u32 {
        self.0
    }
}

/// Fill in the buffer for a GetInfo USB control request.
pub fn get_info_for_usb(buf: &mut [u8]) {
    assert!(buf.len() == 24);
    buf.fill(0);
    buf[4..8].copy_from_slice(&SerialNumber::get().0.to_le_bytes());
    buf[8..12].copy_from_slice(&HardwareVersion::get().as_u32().to_le_bytes());
    buf[12..16].copy_from_slice(&FirmwareVersion::get().as_u32().to_le_bytes());
    // TODO set firmware commit hash from 16..24
}
