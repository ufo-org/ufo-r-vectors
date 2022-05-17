#[macro_export]
macro_rules! opaque_c_type {
    ($wrapper_name:ident, $wrapped_type:ty) => {
        impl $wrapper_name {
            pub(crate) fn wrap(t: $wrapped_type) -> Self {
                $wrapper_name {
                    ptr: Box::into_raw(Box::new(t)).cast(),
                }
            }

            pub(crate) fn none() -> Self {
                $wrapper_name {
                    ptr: std::ptr::null_mut(),
                }
            }

            pub(crate) fn deref(&self) -> Option<&$wrapped_type> {
                if self.ptr.is_null() {
                    None
                } else {
                    Some(unsafe { &*self.ptr.cast() })
                }
            }

            #[allow(dead_code)]
            pub(crate) fn deref_mut(&self) -> Option<&mut $wrapped_type> {
                if self.ptr.is_null() {
                    None
                } else {
                    Some(unsafe { &mut *self.ptr.cast() })
                }
            }
        }

        impl Drop for $wrapper_name {
            fn drop(&mut self) {
                if !self.ptr.is_null() {
                    let mut the_ptr = std::ptr::null_mut();
                    unsafe {
                        std::ptr::swap(&mut the_ptr, &mut self.ptr);
                        assert!(!the_ptr.is_null());
                        Box::<$wrapped_type>::from_raw(the_ptr.cast());
                    }
                }
            }
        }
    };
}
