use libc::size_t;
use std::ffi::{c_char, c_void, CString};
use tokio::sync::mpsc::UnboundedSender;

/// The data pointer is only valid for the duration of the callback.
#[repr(C)]
pub struct ConnMsg {
    bytes: *const c_char,
    len: size_t,
}

/// All data is only valid for the duration of the callback
/// except for the `sender` field of `OnConnect`.
///
/// Do not manually free any of the data except `sender`!!
#[repr(C)]
pub struct ConnStates {
    code: ConnStateCode,
    data: ConnStateData,
}

/// Callback function for receiving messages.
///
/// `callback_self` is feed to the first argument of the callback.
///
/// # Thread Safety
/// Must be thread safe!
#[repr(C)]
#[derive(Clone)]
pub struct OnMsgCallback {
    callback_self: *mut c_void,
    callback: unsafe extern "C" fn(*mut c_void, ConnMsg),
}

/// Callback function for connection state changes.
///
/// `callback_self` is feed to the first argument of the callback.
///
/// # Thread Safety
/// Must be thread safe!
#[repr(C)]
#[derive(Clone)]
pub struct OnConnCallback {
    callback_self: *mut c_void,
    callback: unsafe extern "C" fn(*mut c_void, ConnStates),
}

#[repr(C)]
pub enum ConnStateCode {
    Connect = 0,
    ConnectionClose = 1,
    ListenError = 2,
    ConnectError = 3,
}

#[repr(C)]
pub union ConnStateData {
    on_connect: OnConnect,
    on_connection_close: OnConnectionClose,
    on_listen_error: OnListenError,
    on_connect_error: OnConnectError,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OnConnect {
    local: *const c_char,
    peer: *const c_char,
    conn: *mut CConnection,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OnConnectionClose {
    local: *const c_char,
    peer: *const c_char,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OnListenError {
    addr: *const c_char,
    err: *const c_char,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OnConnectError {
    addr: *const c_char,
    err: *const c_char,
}

/// Drop the sender to close the connection.
pub struct CMsgSender {
    pub(crate) send: UnboundedSender<Vec<u8>>,
}

pub struct CConnection {
    pub(crate) conn: crate::Conn<OnMsgCallback>,
}

impl OnMsgCallback {
    pub fn call_inner(&self, conn_msg: crate::Msg<'_>) {
        let conn_msg = ConnMsg {
            bytes: conn_msg.bytes.as_ptr() as *const c_char,
            len: conn_msg.bytes.len(),
        };
        unsafe { (self.callback)(self.callback_self, conn_msg) }
    }
}

impl FnMut<(crate::Msg<'_>,)> for OnMsgCallback {
    extern "rust-call" fn call_mut(&mut self, conn_msg: (crate::Msg<'_>,)) -> Self::Output {
        self.call_inner(conn_msg.0)
    }
}

impl FnOnce<(crate::Msg<'_>,)> for OnMsgCallback {
    type Output = ();

    extern "rust-call" fn call_once(self, conn_msg: (crate::Msg<'_>,)) -> Self::Output {
        self.call_inner(conn_msg.0)
    }
}

impl Fn<(crate::Msg<'_>,)> for OnMsgCallback {
    extern "rust-call" fn call(&self, conn_msg: (crate::Msg<'_>,)) {
        self.call_inner(conn_msg.0)
    }
}

impl OnConnCallback {
    /// connection callback
    pub(crate) fn call_inner(&self, conn_states: crate::ConnState<OnMsgCallback>) {
        let on_conn = |msg| unsafe { (self.callback)(self.callback_self, msg) };
        match conn_states {
            crate::ConnState::OnConnect {
                local_addr,
                peer_addr,
                conn,
            } => {
                let local = CString::new(local_addr.to_string()).unwrap();
                let peer = CString::new(peer_addr.to_string()).unwrap();
                let conn = Box::into_raw(Box::new(CConnection { conn }));
                let conn_msg = ConnStates {
                    code: ConnStateCode::Connect,
                    data: ConnStateData {
                        on_connect: OnConnect {
                            local: local.as_ptr(),
                            peer: peer.as_ptr(),
                            conn,
                        },
                    },
                };
                on_conn(conn_msg)
            }
            crate::ConnState::OnConnectionClose {
                local_addr,
                peer_addr,
            } => {
                let local = CString::new(local_addr.to_string()).unwrap();
                let peer = CString::new(peer_addr.to_string()).unwrap();
                let conn_msg = ConnStates {
                    code: ConnStateCode::ConnectionClose,
                    data: ConnStateData {
                        on_connection_close: OnConnectionClose {
                            local: local.as_ptr(),
                            peer: peer.as_ptr(),
                        },
                    },
                };
                on_conn(conn_msg)
            }
            crate::ConnState::OnListenError { addr, error } => {
                let addr = CString::new(addr.to_string()).unwrap();
                let error = CString::new(error.to_string()).unwrap();
                let conn_msg = ConnStates {
                    code: ConnStateCode::ListenError,
                    data: ConnStateData {
                        on_listen_error: OnListenError {
                            addr: addr.as_ptr(),
                            err: error.as_ptr(),
                        },
                    },
                };
                on_conn(conn_msg)
            }
            crate::ConnState::OnConnectError { addr, error } => {
                let addr = CString::new(addr.to_string()).unwrap();
                let error = CString::new(error.to_string()).unwrap();
                let conn_msg = ConnStates {
                    code: ConnStateCode::ConnectError,
                    data: ConnStateData {
                        on_connect_error: OnConnectError {
                            addr: addr.as_ptr(),
                            err: error.as_ptr(),
                        },
                    },
                };
                on_conn(conn_msg)
            }
        }
    }
}

impl FnMut<(crate::ConnState<OnMsgCallback>,)> for OnConnCallback {
    extern "rust-call" fn call_mut(
        &mut self,
        conn_msg: (crate::ConnState<OnMsgCallback>,),
    ) -> Self::Output {
        self.call_inner(conn_msg.0)
    }
}

impl FnOnce<(crate::ConnState<OnMsgCallback>,)> for OnConnCallback {
    type Output = ();

    extern "rust-call" fn call_once(
        self,
        conn_msg: (crate::ConnState<OnMsgCallback>,),
    ) -> Self::Output {
        self.call_inner(conn_msg.0)
    }
}

impl Fn<(crate::ConnState<OnMsgCallback>,)> for OnConnCallback {
    extern "rust-call" fn call(&self, conn_msg: (crate::ConnState<OnMsgCallback>,)) {
        self.call_inner(conn_msg.0)
    }
}

unsafe impl Send for OnMsgCallback {}
unsafe impl Sync for OnMsgCallback {}
unsafe impl Send for OnConnCallback {}
unsafe impl Sync for OnConnCallback {}
