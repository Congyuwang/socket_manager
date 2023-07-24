use crate::c_api::callbacks::{OnConnObj, OnMsgObj, WakerObj};
use crate::c_api::structs::{ConnMsg, ConnStates};
use crate::c_api::waker::CWaker;
use libc::c_int;
use std::ffi::c_char;

#[link(name = "socket_manager")]
extern "C" {
    /// Callback function for receiving messages.
    /// Return positive number for `Ready`,
    /// and negative number for `Pending`.
    pub(crate) fn socket_manager_extern_on_msg(
        this: OnMsgObj,
        msg: ConnMsg,
        waker: *mut CWaker,
        err: *mut *mut c_char,
    ) -> c_int;

    /// Callback function for connection state changes.
    pub(crate) fn socket_manager_extern_on_conn(
        this: OnConnObj,
        conn: ConnStates,
        err: *mut *mut c_char,
    );

    /// Waker for the try_send method.
    pub(crate) fn socket_manager_extern_sender_waker_wake(this: WakerObj);

    /// Decrement ref count of the waker.
    pub(crate) fn socket_manager_extern_sender_waker_release(this: WakerObj);

    /// Increment ref count of the waker.
    pub(crate) fn socket_manager_extern_sender_waker_clone(this: WakerObj);
}
