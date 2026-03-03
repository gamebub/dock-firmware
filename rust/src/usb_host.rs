use crate::sys;

pub struct HandheldXferResult {
    pub request: u8,
    pub tag: usize,
    data: [u8; 64],
    success: bool,
    data_len: u16,
}

impl HandheldXferResult {
    pub fn new(request: u8, tag: usize, success: bool, data: &[u8]) -> Self {
        let mut buf = [0u8; 64];
        let len = data.len().min(buf.len());
        (&mut buf[..len]).copy_from_slice(&data[..len]);

        HandheldXferResult {
            request,
            tag,
            success,
            data: buf,
            data_len: len as u16,
        }
    }

    pub fn data(&self) -> Result<&[u8], ()> {
        if self.success {
            Ok(&self.data[..self.data_len as usize])
        } else {
            Err(())
        }
    }
}

pub fn handheld_control_in(request: u8, value: u16, tag: usize, length: usize) {
    unsafe {
        sys::UsbHandheldControlIn(request, value, tag, length as u16);
    }
}

pub fn handheld_control_out(request: u8, value: u16, tag: usize, data: &[u8]) {
    unsafe {
        sys::UsbHandheldControlOut2(request, value, tag, data.as_ptr(), data.len());
    }
}
