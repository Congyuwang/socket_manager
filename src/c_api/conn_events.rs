use crate::c_api::on_msg::OnMsgObj;
use crate::MsgSender;
use std::ffi::c_char;

/// The data pointer is only valid for the duration of the callback.
#[repr(C)]
pub struct ConnMsg {
    pub(crate) bytes: *const c_char,
    pub(crate) len: usize,
}

/// All data is only valid for the duration of the callback
/// except for the `sender` field of `OnConnect`.
///
/// Do not manually free any of the data except `sender`!!
#[repr(C)]
pub struct ConnStates {
    pub(crate) code: ConnStateCode,
    pub(crate) data: ConnStateData,
}

#[repr(C)]
pub enum ConnStateCode {
    Connect = 0,
    ConnectionClose = 1,
    ListenError = 2,
    ConnectError = 3,
    RemoteClose = 4,
}

#[repr(C)]
pub union ConnStateData {
    pub(crate) on_connect: OnConnect,
    pub(crate) on_connection_close: OnConnectionClose,
    pub(crate) on_listen_error: OnListenError,
    pub(crate) on_connect_error: OnConnectError,
    pub(crate) on_remote_close: OnRemoteClose,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OnConnect {
    pub(crate) send: *mut MsgSender,
    pub(crate) conn: *mut Connection,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OnConnectionClose {
    pub(crate) local: *const c_char,
    pub(crate) peer: *const c_char,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OnListenError {
    pub(crate) addr: *const c_char,
    pub(crate) err: *const c_char,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OnConnectError {
    pub(crate) addr: *const c_char,
    pub(crate) err: *const c_char,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OnRemoteClose {
    pub(crate) local: *const c_char,
    pub(crate) peer: *const c_char,
}

pub struct Connection {
    pub(crate) conn: crate::Conn<OnMsgObj>,
}
