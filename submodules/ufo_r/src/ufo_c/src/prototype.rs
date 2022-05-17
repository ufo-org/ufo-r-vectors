pub type UfoPopulateData = *mut libc::c_void;
pub type UfoPopulateCallout =
    extern "C" fn(UfoPopulateData, usize, usize, *mut libc::c_uchar) -> i32;

#[repr(C)]
pub struct UfoParameters {
    pub header_size: usize,
    pub element_size: usize,
    pub element_ct: usize,
    pub min_load_ct: usize,
    pub read_only: bool,
    pub populate_data: UfoPopulateData,
    pub populate_fn: UfoPopulateCallout,
}
