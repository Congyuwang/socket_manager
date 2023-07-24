//! This module contains the C functions that are called by the Rust code.
use crate::c_api::callbacks::{OnConnObj, OnMsgObj, WakerObj};
use crate::c_api::conn_events::{ConnMsg, ConnStates};
use crate::c_api::recv_waker::RecvWaker;
use std::ffi::{c_char, c_long};

#[link(name = "socket_manager")]
extern "C" {
    /// Callback function for receiving messages.
    /// Return positive number for `Ready`,
    /// and negative number for `Pending`.
    pub(crate) fn socket_manager_extern_on_msg(
        this: OnMsgObj,
        msg: ConnMsg,
        waker: RecvWaker,
        err: *mut *mut c_char,
    ) -> c_long;

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
