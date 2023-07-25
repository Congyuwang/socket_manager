//! A notifier is a c/c++ callback object called by rust
//! to notify c/c++ code that they should resume certain tasks.
use std::ffi::c_void;
use std::task::{RawWaker, RawWakerVTable, Waker};

/// The Notifier is constructed by the c/c++ code,
/// and passed to the rust code.
///
/// # Task Resume
/// When `wake` callback is called by rust, the c/c++ task
/// should resume its execution.
///
/// # Lifetime Management.
/// The Notifier has `clone` and `release` callbacks.
/// Say a Notifier start with ref_count = 1,
/// and when `clone` is called, increment its ref_count,
/// and when `release` is called, decrement its ref_count.
///
/// The notifier can be released when its ref_count falls back to 1.
///
/// The c/c++ code must carefully manage the lifetime of the waker.
/// to ensure that the waker is not dropped before the rust code
/// is done with it.
#[repr(C)]
#[derive(Copy, Clone)]
pub struct Notifier {
    pub(crate) this: *mut c_void,
}

#[link(name = "socket_manager")]
extern "C" {
    /// Waker for the try_send method.
    pub(crate) fn socket_manager_extern_notifier_wake(this: Notifier);

    /// Decrement ref count of the waker.
    pub(crate) fn socket_manager_extern_notifier_release(this: Notifier);

    /// Increment ref count of the waker.
    pub(crate) fn socket_manager_extern_notifier_clone(this: Notifier);
}

impl Notifier {
    #[inline]
    pub(crate) unsafe fn to_waker(self) -> Waker {
        const MSG_SENDER_WAKER_VTABLE: RawWakerVTable = make_vtable();

        const fn make_vtable() -> RawWakerVTable {
            RawWakerVTable::new(
                |dat| unsafe {
                    let this = Notifier {
                        this: dat as *mut c_void,
                    };
                    socket_manager_extern_notifier_clone(this);
                    RawWaker::new(dat, &MSG_SENDER_WAKER_VTABLE)
                },
                |dat| unsafe {
                    let this = Notifier {
                        this: dat as *mut c_void,
                    };
                    socket_manager_extern_notifier_wake(this);
                    socket_manager_extern_notifier_release(this);
                },
                |dat| unsafe {
                    let this = Notifier {
                        this: dat as *mut c_void,
                    };
                    socket_manager_extern_notifier_wake(this);
                },
                |dat| unsafe {
                    let this = Notifier {
                        this: dat as *mut c_void,
                    };
                    socket_manager_extern_notifier_release(this);
                },
            )
        }

        let raw_waker = RawWaker::new(self.this as *const (), &MSG_SENDER_WAKER_VTABLE);
        // creating a new waker also increment the ref-count, though
        // which could be decremented immediately if its a temp value.
        socket_manager_extern_notifier_clone(self);
        Waker::from_raw(raw_waker)
    }
}
