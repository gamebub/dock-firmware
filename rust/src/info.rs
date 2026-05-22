use core::fmt::{Display, Formatter};

use crate::sys;

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

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct FirmwareVersion {
    pub major: u8,
    pub minor: u8,
    pub patch: u8,
    pub pre: u8,
}

impl FirmwareVersion {
    pub fn get() -> Self {
        FirmwareVersion {
            major: env!("DOCK_FW_VERSION_MAJOR").parse().unwrap_or(0),
            minor: env!("DOCK_FW_VERSION_MINOR").parse().unwrap_or(0),
            patch: env!("DOCK_FW_VERSION_PATCH").parse().unwrap_or(0),
            pre: 0,
        }
    }

    pub fn as_u32(&self) -> u32 {
        (self.pre as u32)
            | (self.patch as u32) << 8
            | (self.major as u32) << 16
            | (self.minor as u32) << 24
    }
}

impl Display for FirmwareVersion {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "{}.{}.{}", self.major, self.minor, self.patch)
    }
}

pub fn get_git_commit() -> &'static [u8] {
    unsafe {
        let ptr = sys::GetGitCommitHash();
        core::slice::from_raw_parts(ptr, 20)
    }
}

/// Fill in the buffer for a GetInfo USB control request.
pub fn get_info_for_usb(buf: &mut [u8]) {
    assert!(buf.len() == 24);
    buf.fill(0);
    buf[4..8].copy_from_slice(&SerialNumber::get().0.to_le_bytes());
    buf[8..12].copy_from_slice(&HardwareVersion::get().as_u32().to_le_bytes());
    buf[12..16].copy_from_slice(&FirmwareVersion::get().as_u32().to_le_bytes());
    buf[16..24].copy_from_slice(&get_git_commit()[0..8]);
}

pub fn get_chip_id() -> u64 {
    let lo = read_otp_entry(0, 0);
    let hi = read_otp_entry(0, 2);
    ((hi as u64) << 32) | (lo as u64)
}
