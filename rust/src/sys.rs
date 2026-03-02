#[allow(unused)]
unsafe extern "C" {
    pub unsafe fn puts(str: *const u8);
    pub unsafe fn panic(fmt: *const u8, ...) -> !;
}
