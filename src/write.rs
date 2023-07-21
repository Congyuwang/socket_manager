use crate::conn::ConnConfig;
use crate::msg_sender::{SendCommand, RING_BUFFER_SIZE};
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
    let mut flush_tick = tokio::time::interval(duration);
    flush_tick.set_missed_tick_behavior(MissedTickBehavior::Skip);

    'closed: loop {
        let mut has_data = true;
        // start from burst mode
        'burst: loop {
            // burst mode loop
            if write_all_from_ring_buf(&mut ring_buf, &mut write).await? == 0 {
                // exist burst mode loop when there is no data
                break 'burst;
            }
        }
        // when burst mode got no data, switch to waker mode
        'waker: loop {
            tokio::select! {
                biased;
                Some(_) = recv.recv() => {
                    // on flush, read all data from ring buffer and write to socket.
                    write_all_from_ring_buf(&mut ring_buf, &mut write).await?;
                    // disable ticked flush when there is no data.
                    has_data = false;
                }
                _ = ring_buf.wait(RING_BUFFER_SIZE / 4) => {
                    if ring_buf.is_closed() {
                        break 'closed;
                    }
                    // got a bunch of data, switch to burst mode
                    break 'waker;
                }
                _ = ring_buf.wait(1), if !has_data => {
                    if ring_buf.is_closed() {
                        break 'closed;
                    }
                    // got small amount of data, enable ticking flush,
                    has_data = true;
                }
                _ = flush_tick.tick(), if has_data => {
                    // flush everything.
                    write_all_from_ring_buf(&mut ring_buf, &mut write).await?;
                    // disable ticked flush when there is no data.
                    has_data = false;
                }
                _ = stop.closed() => {
                    break 'closed;
                }
                else => {}
            }
        }
    }
    write_all_from_ring_buf(&mut ring_buf, &mut write).await?;
    write.shutdown().await?;
    tracing::debug!("connection stopped (socket manager dropped)");
    Ok(())
}

async fn handle_writer_no_auto_flush(
    mut write: OwnedWriteHalf,
    mut recv: UnboundedReceiver<SendCommand>,
    mut ring_buf: AsyncHeapConsumer<u8>,
    mut stop: oneshot::Sender<()>,
) -> std::io::Result<()> {
    let send_buf_size = socket2::SockRef::from(write.as_ref()).send_buffer_size()?;
    tracing::trace!("send buffer size: {}", send_buf_size);
    'closed: loop {
        // start from burst mode
        'burst: loop {
            // burst mode loop
            if write_all_from_ring_buf(&mut ring_buf, &mut write).await? == 0 {
                // exist burst mode loop when there is no data
                break 'burst;
            }
        }
        // when burst mode got no data, switch to waker mode
        'waker: loop {
            tokio::select! {
                biased;
                Some(_) = recv.recv() => {
                    // on flush, read all data from ring buffer and write to socket.
                    write_all_from_ring_buf(&mut ring_buf, &mut write).await?;
                }
                _ = ring_buf.wait(RING_BUFFER_SIZE / 4) => {
                    if ring_buf.is_closed() {
                        break 'closed;
                    }
                    break 'waker;
                }
                _ = stop.closed() => {
                    break 'closed;
                }
                else => {}
            }
        }
    }
    // flush and close
    write_all_from_ring_buf(&mut ring_buf, &mut write).await?;
    write.shutdown().await?;
    tracing::debug!("connection stopped (socket manager dropped)");
    Ok(())
}

/// directly write from ring buffer to bufwriter.
async fn write_all_from_ring_buf(
    ring_buf: &mut AsyncHeapConsumer<u8>,
    write: &mut OwnedWriteHalf,
) -> std::io::Result<usize> {
    let (left, right) = ring_buf.as_base().as_slices();
    let n = left.len() + right.len();
    if !left.is_empty() {
        write.write_all(left).await?;
    }
    if !right.is_empty() {
        write.write_all(right).await?;
    }
    unsafe { ring_buf.as_mut_base().advance(n) };
    tracing::trace!("write {} bytes", n);
    Ok(n)
}
