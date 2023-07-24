use crate::conn::ConnConfig;
use crate::msg_sender::SendCommand;
use async_ringbuf::AsyncHeapConsumer;
use std::time::Duration;
use tokio::io::AsyncWriteExt;
use tokio::net::tcp::OwnedWriteHalf;
use tokio::sync::mpsc::UnboundedReceiver;
use tokio::sync::oneshot;
use tokio::time::MissedTickBehavior;

/// Receive bytes from recv and write to WriteHalf of TcpStream.
pub(crate) async fn handle_writer(
    write: OwnedWriteHalf,
    recv: UnboundedReceiver<SendCommand>,
    ring_buf: AsyncHeapConsumer<u8>,
    config: ConnConfig,
    stop: oneshot::Sender<()>,
) -> std::io::Result<()> {
    match config.write_flush_interval {
        None => handle_writer_no_auto_flush(write, recv, ring_buf, stop).await,
        Some(duration) => {
            if duration.is_zero() {
                handle_writer_no_auto_flush(write, recv, ring_buf, stop).await
            } else {
                handle_writer_auto_flush(write, recv, ring_buf, duration, stop).await
            }
        }
    }
}

async fn handle_writer_auto_flush(
    mut write: OwnedWriteHalf,
    mut recv: UnboundedReceiver<SendCommand>,
    mut ring_buf: AsyncHeapConsumer<u8>,
    duration: Duration,
    mut stop: oneshot::Sender<()>,
) -> std::io::Result<()> {
    debug_assert!(!duration.is_zero());
    let send_buf_size = socket2::SockRef::from(write.as_ref()).send_buffer_size()?;
    tracing::trace!("send buffer size: {}", send_buf_size);
    let chunk_size = send_buf_size.min(ring_buf.capacity());
    let mut flush_tick = tokio::time::interval(duration);
    flush_tick.set_missed_tick_behavior(MissedTickBehavior::Skip);

    let mut has_data = true;
    loop {
        tokio::select! {
            biased;
            _ = ring_buf.wait(chunk_size) => {
                if ring_buf.is_closed() {
                    tracing::debug!("connection stopped (sender dropped)");
                    break;
                }
                copy_from_ring_buf(&mut ring_buf, &mut write, chunk_size).await?;
                has_data = false;
            }
            Some(_) = recv.recv() => {
                // on flush, read all data from ring buffer and write to socket.
                copy_from_ring_buf(&mut ring_buf, &mut write, chunk_size).await?;
                // disable ticked flush when there is no data.
                has_data = false;
            }
            _ = ring_buf.wait(1), if !has_data => {
                if ring_buf.is_closed() {
                    tracing::debug!("connection stopped (sender dropped)");
                    break;
                }
                // got small amount of data, enable ticking flush,
                has_data = true;
            }
            _ = flush_tick.tick(), if has_data => {
                // flush everything.
                copy_from_ring_buf(&mut ring_buf, &mut write, chunk_size).await?;
                // disable ticked flush when there is no data.
                has_data = false;
            }
            _ = stop.closed() => {
                tracing::debug!("connection stopped (socket manager dropped)");
                break;
            }
            else => {}
        }
    }
    copy_from_ring_buf(&mut ring_buf, &mut write, chunk_size).await?;
    write.shutdown().await?;
    Ok(())
}

async fn handle_writer_no_auto_flush(
    mut write: OwnedWriteHalf,
    mut recv: UnboundedReceiver<SendCommand>,
    mut ring_buf: AsyncHeapConsumer<u8>,
    mut stop: oneshot::Sender<()>,
) -> std::io::Result<()> {
    let send_buf_size = socket2::SockRef::from(write.as_ref()).send_buffer_size()?;
    let chunk_size = send_buf_size.min(ring_buf.capacity());
    tracing::trace!("send buffer size: {}", send_buf_size);
    loop {
        tokio::select! {
            biased;
            _ = ring_buf.wait(chunk_size) => {
                if ring_buf.is_closed() {
                    tracing::debug!("connection stopped (sender dropped)");
                    break;
                }
                copy_from_ring_buf(&mut ring_buf, &mut write, chunk_size).await?;
            }
            Some(_) = recv.recv() => {
                // on flush, read all data from ring buffer and write to socket.
                copy_from_ring_buf(&mut ring_buf, &mut write, chunk_size).await?;
            }
            _ = stop.closed() => {
                tracing::debug!("connection stopped (socket manager dropped)");
                break;
            }
            else => {}
        }
    }
    // flush and close
    copy_from_ring_buf(&mut ring_buf, &mut write, chunk_size).await?;
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

async fn write_chunk(
    ring_buf: &mut AsyncHeapConsumer<u8>,
    write: &mut OwnedWriteHalf,
    chunk_size: usize,
) -> std::io::Result<usize> {
    debug_assert!(chunk_size > 0);
    let (left, right) = ring_buf.as_base().as_slices();
    let left_len = left.len();
    let right_len = right.len();
    let written = if left_len >= chunk_size {
        write.write_all(&left[..chunk_size]).await?;
        chunk_size
    } else {
        if left_len > 0 {
            write.write_all(left).await?;
        }
        let remaining = chunk_size - left_len;
        if right_len >= remaining {
            write.write_all(&right[..remaining]).await?;
            chunk_size
        } else {
            if right_len > 0 {
                write.write_all(right).await?;
            }
            left_len + right_len
        }
    };
    unsafe { ring_buf.as_mut_base().advance(written) };
    Ok(written)
}
