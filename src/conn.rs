use crate::{ConnectionState, Msg};
use std::net::SocketAddr;
use std::num::NonZeroUsize;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::task::{Poll, Waker};
use std::time::Duration;
use tokio::sync::oneshot;

/// The connection struct for the on_conn callback.
pub struct Conn<OnMsg> {
    consumed: AtomicBool,
    inner: Option<ConnInner<OnMsg>>,
    pub(crate) local_addr: SocketAddr,
    pub(crate) peer_addr: SocketAddr,
    conn_state: Arc<ConnectionState>,
}

struct ConnInner<OnMsg> {
    conn_config_setter: oneshot::Sender<(OnMsg, ConnConfig)>,
}

impl<OnMsg> Conn<OnMsg> {
    pub(crate) fn new(
        conn_config_setter: oneshot::Sender<(OnMsg, ConnConfig)>,
        local_addr: SocketAddr,
        peer_addr: SocketAddr,
        conn_state: Arc<ConnectionState>,
    ) -> Self {
        Self {
            consumed: AtomicBool::new(false),
            inner: Some(ConnInner { conn_config_setter }),
            local_addr,
            peer_addr,
            conn_state,
        }
    }
}

/// Connection configuration
#[derive(Copy, Clone)]
pub struct ConnConfig {
    /// zero represent no auto flush
    pub write_flush_interval: Duration,
    /// zero represent no auto flush
    pub read_msg_flush_interval: Duration,
    pub msg_buffer_size: Option<NonZeroUsize>,
}

impl<OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + 'static> Conn<OnMsg> {
    /// This function should be called only once.
    pub fn start_connection(&mut self, on_msg: OnMsg, config: ConnConfig) -> std::io::Result<()> {
        self.consumed
            .compare_exchange(false, true, Ordering::Acquire, Ordering::Relaxed)
            .map_err(|_| {
                std::io::Error::new(
                    std::io::ErrorKind::Other,
                    "calling `start_connection` after connection consumed",
                )
            })?;
        let conn = self.inner.take().unwrap();
        if conn.conn_config_setter.send((on_msg, config)).is_err() {
            // if 'OnConnect' callback throws error before calling start_connection,
            // might result in conn_config receiver being dropped before this.
            tracing::error!("callback config setter send failed");
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "callback config setter send failed",
            ));
        }
        Ok(())
    }

    /// The close API works either before `start_connection` is called
    /// or after the `on_connect` callback.
    /// It won't have effect between the two points.
    pub fn close(&mut self) -> std::io::Result<()> {
        if self
            .consumed
            .compare_exchange(false, true, Ordering::Acquire, Ordering::Relaxed)
            .is_ok()
        {
            drop(self.inner.take().unwrap());
        }
        drop(
            self.conn_state
                .connections
                .remove(&(self.local_addr, self.peer_addr)),
        );
        Ok(())
    }
}
