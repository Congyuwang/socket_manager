use async_ringbuf::halves::{AsyncCons, AsyncProd};
use async_ringbuf::producer::AsyncProducer;
use async_ringbuf::traits::{AsyncObserver, Observer, Producer, Split};
use async_ringbuf::AsyncHeapRb;
use ringbuf::SharedRb;
use std::sync::Arc;
use std::task::Poll::{Pending, Ready};
use std::task::{Poll, Waker};
use tokio::runtime::Handle;
use tokio::sync::mpsc::{unbounded_channel, UnboundedReceiver, UnboundedSender};

/// 256KB ring buffer.
pub const RING_BUFFER_SIZE: usize = 256 * 1024;

/// Sender Commands other than bytes.
pub(crate) enum SendCommand {
    Flush,
}

pub type AsyncHeapProducer<T> = AsyncProd<Arc<AsyncHeapRb<T>>>;
pub type AsyncHeapConsumer<T> = AsyncCons<Arc<AsyncHeapRb<T>>>;

pub(crate) fn make_sender(handle: Handle) -> (MsgSender, MsgRcv) {
    let (cmd, cmd_recv) = unbounded_channel::<SendCommand>();
    let (rings_prd, rings) = unbounded_channel::<AsyncHeapConsumer<u8>>();
    let (ring_buf, ring) = AsyncHeapRb::<u8>::new(RING_BUFFER_SIZE).split();
    rings_prd.send(ring).unwrap();
    (
        MsgSender {
            cmd,
            ring_buf,
            rings_prd,
            handle,
        },
        MsgRcv { cmd_recv, rings },
    )
}

pub(crate) struct MsgRcv {
    pub(crate) cmd_recv: UnboundedReceiver<SendCommand>,
    pub(crate) rings: UnboundedReceiver<AsyncHeapConsumer<u8>>,
}

/// Drop the sender to close the connection.
pub struct MsgSender {
    pub(crate) cmd: UnboundedSender<SendCommand>,
    pub(crate) ring_buf: AsyncHeapProducer<u8>,
    pub(crate) rings_prd: UnboundedSender<AsyncHeapConsumer<u8>>,
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
        let n = buf.push_slice(&bytes[*offset..]);
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

impl MsgSender {
    /// The blocking API for sending bytes.
    /// Do not use this method in the callback (i.e. async context),
    /// as it might block.
    pub fn send_block(&mut self, bytes: &[u8]) -> std::io::Result<()> {
        if bytes.is_empty() {
            return Ok(());
        }
        let mut offset = 0usize;
        // attempt to write the entire message without blocking
        if let BurstWriteState::Finished = burst_write(&mut offset, &mut self.ring_buf, bytes) {
            return Ok(());
        }
        // unfinished, enter into future
        self.handle.clone().block_on(async {
            loop {
                self.ring_buf.wait_vacant(1).await;
                // check if closed
                if self.ring_buf.is_closed() {
                    break Err(std::io::Error::new(
                        std::io::ErrorKind::Other,
                        "connection closed",
                    ));
                }
                if let BurstWriteState::Finished =
                    burst_write(&mut offset, &mut self.ring_buf, bytes)
                {
                    return Ok(());
                }
            }
        })?;
        Ok(())
    }

    /// The non-blocking API for sending bytes.
    ///
    /// This API does not implement back pressure.
    /// It caches all received bytes in memory
    /// (efficiently using a chain of ring buffers).
    pub fn send_nonblock(&mut self, bytes: &[u8]) -> std::io::Result<()> {
        let msg_size = bytes.len();
        if msg_size == 0 {
            return Ok(());
        }
        let mut offset = 0usize;
        if msg_size <= RING_BUFFER_SIZE {
            // attempt direct write for small message
            if let BurstWriteState::Finished = burst_write(&mut offset, &mut self.ring_buf, bytes) {
                return Ok(());
            }
        }
        let (ring_buf, ring) = if msg_size <= RING_BUFFER_SIZE {
            let mut rb = AsyncHeapRb::<u8>::new(RING_BUFFER_SIZE);
            rb.push_slice(&bytes[offset..]);
            rb
        } else {
            // direct allocation for large message
            AsyncHeapRb::<u8>::from(unsafe {
                SharedRb::from_raw_parts(std::mem::transmute(bytes.to_vec()), 0, msg_size)
            })
        }.split();
        // send new allocation
        self.rings_prd.send(ring).map_err(|e| {
            std::io::Error::new(
                std::io::ErrorKind::WriteZero,
                format!("connection closed: {e}"),
            )
        })?;
        // set head to new ring_buf
        self.ring_buf = ring_buf;
        Ok(())
    }

    /// Try sending bytes (the async api).
    ///
    /// Unless the buffer is empty, it shouldn't return 0.
    pub fn send_async(&mut self, bytes: &[u8], waker: Waker) -> Poll<std::io::Result<usize>> {
        if bytes.is_empty() {
            return Ready(Ok(0));
        }
        let mut offset = 0usize;
        loop {
            // attempt to write as much as possible
            burst_write(&mut offset, &mut self.ring_buf, bytes);
            if offset > 0 {
                break Ready(Ok(offset));
            }
            // offset = 0, prepare to wait
            self.ring_buf.register_read_waker(&waker);
            // check again that the ring_buf is not empty
            // to prevent deadlock
            if !self.ring_buf.is_empty() {
                break Pending;
            }
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
