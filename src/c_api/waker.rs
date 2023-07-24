use std::task::Waker;

pub struct CWaker {
    pub(crate) waker: Waker,
}

#[no_mangle]
pub unsafe extern "C" fn msg_waker_wake(waker: *mut CWaker) {
    (*waker).waker.wake_by_ref();
}

#[no_mangle]
pub unsafe extern "C" fn msg_waker_destroy(waker: *mut CWaker) {
    drop(Box::from_raw(waker));
}
