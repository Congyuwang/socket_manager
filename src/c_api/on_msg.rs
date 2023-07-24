use crate::c_api::async_ffi::waker::CWaker;
use crate::c_api::conn_events::ConnMsg;
use crate::c_api::utils::parse_c_err_str;
use std::ffi::{c_char, c_long, c_void};
use std::ptr::null_mut;
use std::task::{Poll, Waker};

/// Callback function for receiving messages.
///
/// `callback_self` is feed to the first argument of the callback.
///
/// # Error Handling
/// Returns null_ptr on success, otherwise returns a pointer to a malloced
/// C string containing the error message (the c string should be freed by the
/// caller).
///
/// # Safety
/// The callback pointer must be valid before connection is closed!!
///
/// # Thread Safety
/// Must be thread safe!
#[repr(C)]
#[derive(Copy, Clone)]
pub struct OnMsgObj {
    this: *mut c_void,
}

#[link(name = "socket_manager")]
extern "C" {
    /// Callback function for receiving messages.
    /// Return positive number for `Ready`,
    /// and negative number for `Pending`.
    pub(crate) fn socket_manager_extern_on_msg(
        this: OnMsgObj,
        msg: ConnMsg,
        waker: CWaker,
        err: *mut *mut c_char,
    ) -> c_long;
}

impl OnMsgObj {
    pub fn call_inner(
        &self,
        conn_msg: crate::Msg<'_>,
        waker: Waker,
    ) -> Poll<Result<usize, String>> {
        let len = conn_msg.bytes.len();
        let conn_msg = ConnMsg {
            bytes: conn_msg.bytes.as_ptr() as *const c_char,
            len,
        };
        // takes the ownership of the waker
        let waker = CWaker::from_waker(waker);
        unsafe {
            let mut err: *mut c_char = null_mut();
            let cb_result = socket_manager_extern_on_msg(*self, conn_msg, waker, &mut err);
            if let Err(e) = parse_c_err_str(err) {
                tracing::error!("Error thrown in OnMsg callback: {e}");
                Poll::Ready(Err(e))
            } else if cb_result > 0 {
                assert!(cb_result <= len as c_long);
                Poll::Ready(Ok(cb_result as usize))
            } else {
                Poll::Pending
            }
        }
    }
}

impl FnMut<(crate::Msg<'_>, Waker)> for OnMsgObj {
    extern "rust-call" fn call_mut(&mut self, args: (crate::Msg<'_>, Waker)) -> Self::Output {
        self.call_inner(args.0, args.1)
    }
}

impl FnOnce<(crate::Msg<'_>, Waker)> for OnMsgObj {
    type Output = Poll<Result<usize, String>>;

    extern "rust-call" fn call_once(self, args: (crate::Msg<'_>, Waker)) -> Self::Output {
        self.call_inner(args.0, args.1)
    }
}

impl Fn<(crate::Msg<'_>, Waker)> for OnMsgObj {
    extern "rust-call" fn call(&self, args: (crate::Msg<'_>, Waker)) -> Self::Output {
        self.call_inner(args.0, args.1)
    }
}

unsafe impl Send for OnMsgObj {}

unsafe impl Sync for OnMsgObj {}
