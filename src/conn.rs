use crate::msg_sender::CMsgSender;
use crate::Msg;
use std::num::NonZeroUsize;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;
use tokio::sync::oneshot;

/// The connection struct for the on_conn callback.
pub struct Conn<OnMsg> {
    consumed: AtomicBool,
    inner: Option<ConnInner<OnMsg>>,
}

struct ConnInner<OnMsg> {
    conn_config_setter: oneshot::Sender<(OnMsg, ConnConfig)>,
    send: CMsgSender,
}

impl<OnMsg> Conn<OnMsg> {
    pub(crate) fn new(
        conn_config_setter: oneshot::Sender<(OnMsg, ConnConfig)>,
        send: CMsgSender,
    ) -> Self {
        Self {
            consumed: AtomicBool::new(false),
            inner: Some(ConnInner {
                conn_config_setter,
                send,
            }),
        }
    }
}

/// Connection configuration
#[derive(Copy, Clone)]
pub struct ConnConfig {
    pub write_flush_interval: Option<Duration>,
    pub read_msg_flush_interval: Option<Duration>,
    pub msg_buffer_size: Option<NonZeroUsize>,
}

impl<OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + 'static + Clone> Conn<OnMsg> {
    /// This function should be called only once.
    pub fn start_connection(
        &mut self,
        on_msg: OnMsg,
        config: ConnConfig,
    ) -> std::io::Result<CMsgSender> {
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
        Ok(conn.send)
    }

    /// close the connection without using it.
    pub fn close(&mut self) -> std::io::Result<()> {
        self.consumed
            .compare_exchange(false, true, Ordering::Acquire, Ordering::Relaxed)
            .map_err(|_| {
                std::io::Error::new(
                    std::io::ErrorKind::Other,
                    "calling `close` after connection consumed",
                )
            })?;
        drop(self.inner.take().unwrap());
        Ok(())
    }
}
