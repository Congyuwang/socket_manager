use crate::conn::ConnConfig;
use crate::msg_sender::MsgRcv;
use crate::RING_BUFFER_SIZE;
use async_ringbuf::AsyncHeapConsumer;
use std::time::Duration;
use tokio::io::AsyncWriteExt;
use tokio::net::tcp::OwnedWriteHalf;
use tokio::sync::oneshot;
use tokio::time::MissedTickBehavior;

/// Receive bytes from recv and write to WriteHalf of TcpStream.
pub(crate) async fn handle_writer(
    write: OwnedWriteHalf,
    recv: MsgRcv,
    config: ConnConfig,
    stop: oneshot::Sender<()>,
) -> std::io::Result<()> {
    let duration = config.write_flush_interval;
    if duration.is_zero() {
        handle_writer_no_auto_flush(write, recv, stop).await
    } else {
        handle_writer_auto_flush(write, recv, duration, stop).await
    }
}

async fn handle_writer_auto_flush(
    mut write: OwnedWriteHalf,
    mut recv: MsgRcv,
    duration: Duration,
    mut stop: oneshot::Sender<()>,
) -> std::io::Result<()> {
    debug_assert!(!duration.is_zero());
    let send_buf_size = socket2::SockRef::from(write.as_ref()).send_buffer_size()?;
    tracing::trace!("send buffer size: {}", send_buf_size);
    let chunk_size = send_buf_size.min(RING_BUFFER_SIZE);
    let mut flush_tick = tokio::time::interval(duration);
    flush_tick.set_missed_tick_behavior(MissedTickBehavior::Skip);

    'close: loop {
        // obtain a ring buffer
        let ring = tokio::select! {
            biased;
            ring = recv.rings.recv() => ring,
            _ = stop.closed() => break 'close,
        };
        let mut ring = match ring {
            Some(ring) => ring,
            None => break 'close,
        };
        let mut has_data = true;
        'ring: loop {
            tokio::select! {
                biased;
                // buf threshold
                _ = ring.wait(chunk_size) => {
                    if ring.is_closed() {
                        break 'ring;
                    }
                    copy_from_ring_buf(&mut ring, &mut write, chunk_size).await?;
                    has_data = false;
                }
                // flush
                cmd = recv.cmd_recv.recv() => {
                    if cmd.is_none() {
                        break 'close;
                    }
                    copy_from_ring_buf(&mut ring, &mut write, chunk_size).await?;
                    has_data = false;
                }
                _ = ring.wait(1), if !has_data => {
                    if ring.is_closed() {
                        break 'ring;
                    }
                    // got data, no writing, enable ticking
                    has_data = true;
                }
                // tick flush
                _ = flush_tick.tick(), if has_data => {
                    copy_from_ring_buf(&mut ring, &mut write, chunk_size).await?;
                    has_data = false;
                }
                _ = stop.closed() => break 'close,
            }
        }
        // always clear the old ring_buf before reading the next
        copy_from_ring_buf(&mut ring, &mut write, chunk_size).await?;
    }
    tracing::debug!("connection stopped");
    write.shutdown().await?;
    Ok(())
}

async fn handle_writer_no_auto_flush(
    mut write: OwnedWriteHalf,
    mut recv: MsgRcv,
    mut stop: oneshot::Sender<()>,
) -> std::io::Result<()> {
    let send_buf_size = socket2::SockRef::from(write.as_ref()).send_buffer_size()?;
    tracing::trace!("send buffer size: {}", send_buf_size);
    let chunk_size = send_buf_size.min(RING_BUFFER_SIZE);

    'close: loop {
        // obtain a ring buffer
        let ring = tokio::select! {
            biased;
            ring = recv.rings.recv() => ring,
            _ = stop.closed() => break 'close,
        };
        let mut ring = match ring {
            Some(ring) => ring,
            None => break 'close,
        };
        'ring: loop {
            tokio::select! {
                biased;
                // buf threshold
                _ = ring.wait(chunk_size) => {
                    if ring.is_closed() {
                        break 'ring;
                    }
                    copy_from_ring_buf(&mut ring, &mut write, chunk_size).await?;
                }
                // flush
                cmd = recv.cmd_recv.recv() => {
                    if cmd.is_none() {
                        break 'close;
                    }
                    copy_from_ring_buf(&mut ring, &mut write, chunk_size).await?;
                }
                _ = stop.closed() => break 'close,
            }
        }
        // always clear the old ring_buf before reading the next
        copy_from_ring_buf(&mut ring, &mut write, chunk_size).await?;
    }
    tracing::debug!("connection stopped");
    write.shutdown().await?;
    Ok(())
}

/// directly write from ring buffer to bufwriter.
/// until the ring buffer is empty.
async fn copy_from_ring_buf(
    ring_buf: &mut AsyncHeapConsumer<u8>,
    write: &mut OwnedWriteHalf,
    chunk_size: usize,
) -> std::io::Result<usize> {
    let mut n = 0;
    loop {
        let written = write_chunk(ring_buf, write, chunk_size).await?;
        if written == 0 {
            break;
        }
        n += written;
    }
    Ok(n)
}

#[inline]
async fn write_chunk(
    ring_buf: &mut AsyncHeapConsumer<u8>,
    write: &mut OwnedWriteHalf,
    chunk_size: usize,
) -> std::io::Result<usize> {
    debug_assert!(chunk_size > 0);
    let (left, right) = ring_buf.as_base().as_slices();

    // precompute all lengths to reduce cpu branching
    let left_written = left.len().min(chunk_size);
    let remaining = chunk_size - left_written;
    let right_written = right.len().min(remaining);
    let total = left_written + right_written;

    // execute write
    if left_written > 0 {
        write.write_all(&left[..left_written]).await?;
    }
    if right_written > 0 {
        write.write_all(&right[..right_written]).await?;
    }

    // update ring_buf
    unsafe { ring_buf.as_mut_base().advance(total) };
    Ok(total)
}
