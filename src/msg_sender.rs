use crate::c_api::callbacks::WakerObj;
use async_ringbuf::AsyncHeapProducer;
use std::future::Future;
use std::pin::pin;
use std::task::Poll::Ready;
use std::task::{Context, Poll};
use tokio::runtime::Handle;
use tokio::sync::mpsc::UnboundedSender;

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

enum BurstWriteState {
    Pending,
    Finished,
}

#[inline(always)]
fn burst_write(
    offset: &mut usize,
    buf: &mut AsyncHeapProducer<u8>,
    bytes: &[u8],
) -> BurstWriteState {
    loop {
        let n = buf.as_mut_base().push_slice(&bytes[*offset..]);
        if n == 0 {
            // no bytes read, return
            break BurstWriteState::Pending;
        }
        *offset += n;
        if *offset == bytes.len() {
            // all bytes read, return
            break BurstWriteState::Finished;
        }
    }
}

impl CMsgSender {
    /// The blocking API of sending bytes.
    /// Do not use this method in the callback (i.e. async context),
    /// as it might block.
    pub fn send_block(&mut self, bytes: &[u8]) -> std::io::Result<()> {
        let mut offset = 0usize;
        // attempt to write the entire message without blocking
        if let BurstWriteState::Finished = burst_write(&mut offset, &mut self.buf_prd, bytes) {
            return Ok(());
        }
        // unfinished, enter into future
        self.handle.clone().block_on(async {
            loop {
                self.buf_prd.wait_free(1).await;
                // check if closed
                if self.buf_prd.is_closed() {
                    break Err(std::io::Error::new(
                        std::io::ErrorKind::Other,
                        "connection closed",
                    ));
                }
                if let BurstWriteState::Finished =
                    burst_write(&mut offset, &mut self.buf_prd, bytes)
                {
                    return Ok(());
                }
            }
        })?;
        Ok(())
    }

    /// Try sending bytes.
    ///
    /// Returning -1 to indicate pending.
    pub fn try_send(&mut self, bytes: &[u8], waker_obj: Option<WakerObj>) -> std::io::Result<i64> {
        if bytes.is_empty() {
            return Ok(0);
        }
        if self.buf_prd.is_closed() {
            return Err(std::io::Error::new(
                std::io::ErrorKind::WriteZero,
                "connection closed",
            ));
        }
        let n = self.buf_prd.as_mut_base().push_slice(bytes);
        if n > 0 {
            return Ok(n as i64);
        }
        // n = 0, not closed
        if let Some(waker_obj) = waker_obj {
            // some waker, wait on the waker
            let waker = unsafe { waker_obj.make_waker() };
            match pin!(self.buf_prd.wait_free(1))
                .poll(&mut Context::from_waker(&waker))
            {
                Ready(_) => {
                    // might be ready on closed
                    if self.buf_prd.is_closed() {
                        return Err(std::io::Error::new(
                            std::io::ErrorKind::WriteZero,
                            "connection closed",
                        ));
                    }
                    let n = self.buf_prd.as_mut_base().push_slice(bytes);
                    Ok(n as i64)
                }
                Poll::Pending => Ok(-1),
            }
        } else {
            // no waker, return 0
            Ok(0)
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
