use std::fmt::Debug;
use std::sync::RwLockWriteGuard;

use anyhow::Result;

use libc::c_void;
use ufo_core::{UfoObject, WrappedUfoObject};

use super::*;

#[repr(C)]
pub struct UfoObj {
    ptr: *mut c_void,
}

opaque_c_type!(UfoObj, WrappedUfoObject);

impl UfoObj {
    fn with_ufo<F, T, E>(&self, f: F) -> Option<T>
    where
        F: FnOnce(RwLockWriteGuard<UfoObject>) -> Result<T, E>,
        E: Debug,
    {
        self.deref().map(|ufo| {
            let locked_ufo = ufo.write().expect("unable to lock UFO");
            f(locked_ufo).expect("Function call failed")
        })
    }

    #[no_mangle]
    pub unsafe extern "C" fn ufo_reset(&mut self) -> i32 {
        std::panic::catch_unwind(|| {
            self.with_ufo(|mut ufo| ufo.reset())
                .map(|w| w.wait())
                .map(|()| 0)
                .unwrap_or(-1)
        })
        .unwrap_or(-1)
    }

    #[no_mangle]
    pub extern "C" fn ufo_header_ptr(&self) -> *mut std::ffi::c_void {
        std::panic::catch_unwind(|| {
            self.with_ufo(|ufo| Ok::<*mut c_void, ()>(ufo.header_ptr()))
                .unwrap_or_else(|| std::ptr::null_mut())
        })
        .unwrap_or_else(|_| std::ptr::null_mut())
    }

    #[no_mangle]
    pub extern "C" fn ufo_body_ptr(&self) -> *mut std::ffi::c_void {
        std::panic::catch_unwind(|| {
            self.with_ufo(|ufo| Ok::<*mut c_void, ()>(ufo.body_ptr()))
                .unwrap_or_else(|| std::ptr::null_mut())
        })
        .unwrap_or_else(|_| std::ptr::null_mut())
    }

    #[no_mangle]
    pub extern "C" fn ufo_free(self) {
        std::panic::catch_unwind(|| {
            self.deref()
                .map(|ufo| {
                    ufo.write()
                        .expect("unable to lock UFO")
                        .free()
                        .expect("unable to free UFO")
                })
                .map(|w| w.wait())
                .unwrap_or(())
        })
        .unwrap_or(())
    }

    #[no_mangle]
    pub extern "C" fn ufo_is_error(&self) -> bool {
        self.deref().is_none()
    }
}
