//! A notifier is a c/c++ callback object called by rust
//! to notify c/c++ code that they should resume certain tasks.
use crate::c_api::extern_c::{
    socket_manager_extern_sender_waker_clone, socket_manager_extern_sender_waker_release,
    socket_manager_extern_sender_waker_wake,
};
use std::ffi::c_void;
use std::task::{RawWaker, RawWakerVTable, Waker};

/// The Notifier is constructed by the c/c++ code,
/// and passed to the rust code.
///
/// The rust code call the waker to notify c/c++ code
/// that they should resume sending messages.
///
/// The c/c++ code must carefully manage the lifetime of the waker.
/// to ensure that the waker is not dropped before the rust code
/// is done with it.
#[repr(C)]
#[derive(Copy, Clone)]
pub struct Notifier {
    pub(crate) this: *mut c_void,
}

impl Notifier {
    pub(crate) unsafe fn to_waker(&self) -> Waker {
        const MSG_SENDER_WAKER_VTABLE: RawWakerVTable = make_vtable();

        const fn make_vtable() -> RawWakerVTable {
            RawWakerVTable::new(
                |dat| unsafe {
                    let this = Notifier { this: dat as *mut c_void };
                    socket_manager_extern_sender_waker_clone(this);
                    RawWaker::new(dat, &MSG_SENDER_WAKER_VTABLE)
                },
                |dat| unsafe {
                    let this = Notifier { this: dat as *mut c_void };
                    socket_manager_extern_sender_waker_wake(this);
                    socket_manager_extern_sender_waker_release(this);
                },
                |dat| unsafe {
                    let this = Notifier { this: dat as *mut c_void };
                    socket_manager_extern_sender_waker_wake(this);
                },
                |dat| unsafe {
                    let this = Notifier { this: dat as *mut c_void };
                    socket_manager_extern_sender_waker_release(this);
                },
            )
        }

        let raw_waker = RawWaker::new(self.this as *const (), &MSG_SENDER_WAKER_VTABLE);
        Waker::from_raw(raw_waker)
    }
}
