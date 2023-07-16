use crate::c_api::callbacks::WakerObj;
use async_ringbuf::AsyncHeapProducer;
use std::future::Future;
use std::pin::pin;
use std::task::Poll::Ready;
use std::task::{Context, Poll};
use tokio::io::AsyncWriteExt;
use tokio::runtime::Handle;
use tokio::sync::mpsc::UnboundedSender;

pub const RING_BUFFER_SIZE: usize = 256 * 1024; // 256KB

/// Sender Commands other than bytes.
pub(crate) enum SendCommand {
    Flush,
}

/// Drop the sender to close the connection.
pub struct CMsgSender {
    pub(crate) cmd: UnboundedSender<SendCommand>,
    pub(crate) buf_prd: AsyncHeapProducer<u8>,
    pub(crate) handle: Handle,
}

impl CMsgSender {
    /// The blocking API of sending bytes.
    /// Do not use this method in the callback (i.e. async context),
    /// as it might block.
    pub fn send_block(&mut self, bytes: &[u8]) -> std::io::Result<()> {
        let n = self.buf_prd.as_mut_base().push_slice(bytes);
        // n = 0, check if closed
        if n == 0 && self.buf_prd.is_closed() {
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "connection closed",
            ));
        }
        // unfinished, enter into future
        if n < bytes.len() {
            self.handle
                .clone()
                .block_on(self.buf_prd.write_all(&bytes[n..]))?;
        }
        Ok(())
    }

    /// Try sending bytes.
    ///
    /// Returning -1 to indicate pending.
    pub fn try_send(&mut self, bytes: &[u8], waker_obj: Option<WakerObj>) -> std::io::Result<i64> {
        let n = self.buf_prd.as_mut_base().push_slice(bytes);
        match (n, waker_obj) {
            // no bytes written, wait on the waker
            (0, Some(waker_obj)) => {
                let waker = unsafe { waker_obj.make_waker() };
                match pin!(self.buf_prd.wait_free(RING_BUFFER_SIZE / 2))
                    .poll(&mut Context::from_waker(&waker))
                {
                    Ready(_) => {
                        if self.buf_prd.is_closed() {
                            Ok(0)
                        } else {
                            let n = self.buf_prd.as_mut_base().push_slice(bytes);
                            Ok(n as i64)
                        }
                    }
                    Poll::Pending => Ok(-1),
                }
            }
            // no bytes written, no waker, return 0
            (0, None) => Ok(0),
            // some bytes written, return the number of bytes written
            (n, _) => Ok(n as i64),
        }
    }

    pub fn flush(&mut self) -> std::io::Result<()> {
        self.cmd.send(SendCommand::Flush).map_err(|_| {
            std::io::Error::new(
                std::io::ErrorKind::Other,
                "failed to send flush command, connection closed",
            )
        })
    }
}
