use std::ffi::c_void;
use std::task::{RawWaker, RawWakerVTable, Waker};

/// This is a C compatible version of `std::task::Waker`,
/// which is used to wake the `receiver` task.
/// But it can be used to wake any async rust task.
///
/// # Safety
/// Do not use this struct directly.
/// Properly wrap it in c++ class.
///
/// This struct is equivalent to a raw pointer.
/// Manager with care.
#[repr(C)]
pub struct RecvWaker {
    data: *const c_void,
    vtable: *const c_void,
}

/// Call the waker to wake the `receiver` task.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_recv_waker_wake(waker: &RecvWaker) {
    waker.wake_by_ref();
}

/// Release the waker.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_recv_waker_free(waker: RecvWaker) {
    drop(waker.into_waker());
}

impl RecvWaker {
    /// Take ownership of the waker.
    pub(crate) fn from_waker(waker: Waker) -> Self {
        let raw_waker = waker.as_raw();
        let c_waker = Self {
            data: raw_waker.data() as *const c_void,
            vtable: raw_waker.vtable() as *const RawWakerVTable as *const c_void,
        };
        // do not drop the waker.
        std::mem::forget(waker);
        c_waker
    }

    /// Do Not restore ownership of the waker.
    unsafe fn wake_by_ref(&self) {
        let raw_waker = RawWaker::new(
            self.data as *const (),
            &*(self.vtable as *const RawWakerVTable),
        );
        let waker = Waker::from_raw(raw_waker);
        waker.wake_by_ref();
        // do not drop the waker.
        std::mem::forget(waker);
    }

    /// Restore ownership of the waker.
    unsafe fn into_waker(self) -> Waker {
        let raw_waker = RawWaker::new(
            self.data as *const (),
            &*(self.vtable as *const RawWakerVTable),
        );
        Waker::from_raw(raw_waker)
    }
}
