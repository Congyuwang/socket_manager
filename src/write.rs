use crate::conn::ConnConfig;
use crate::msg_sender::MsgRcv;
use crate::read::MIN_MSG_BUFFER_SIZE;
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
                // !has_data => wait for has_data
                // has_data => wait for write_threshold
                _ = ring.wait(if !has_data {1} else {MIN_MSG_BUFFER_SIZE}) => {
                    if ring.is_closed() {
                        break 'ring;
                    }
                    has_data = true;
                    if ring.len() >= MIN_MSG_BUFFER_SIZE {
                        flush(&mut ring, &mut write).await?;
                        has_data = false
                    }
                }
                // flush
                cmd = recv.cmd_recv.recv() => {
                    // always flush, including if sender is dropped
                    flush(&mut ring, &mut write).await?;
                    if cmd.is_none() {
                        break 'close;
                    }
                    has_data = false;
                }
                // tick flush
                _ = flush_tick.tick(), if has_data => {
                    flush(&mut ring, &mut write).await?;
                    has_data = false;
                }
                _ = stop.closed() => break 'close,
            }
        }
        // always clear the old ring_buf before reading the next
        flush(&mut ring, &mut write).await?;
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
                _ = ring.wait(MIN_MSG_BUFFER_SIZE) => {
                    if ring.is_closed() {
                        break 'ring;
                    }
                    flush(&mut ring, &mut write).await?;
                }
                // flush
                cmd = recv.cmd_recv.recv() => {
                    // always flush, including if sender is dropped
                    flush(&mut ring, &mut write).await?;
                    if cmd.is_none() {
                        break 'close;
                    }
                }
                _ = stop.closed() => break 'close,
            }
        }
        // always clear the old ring_buf before reading the next
        flush(&mut ring, &mut write).await?;
    }
    tracing::debug!("connection stopped");
    write.shutdown().await?;
    Ok(())
}

/// directly write from ring buffer to bufwriter.
/// until the ring buffer is empty.
async fn flush(
    ring_buf: &mut AsyncHeapConsumer<u8>,
    write: &mut OwnedWriteHalf,
) -> std::io::Result<()> {
    loop {
        let (left, _) = ring_buf.as_mut_base().as_slices();
        if !left.is_empty() {
            let count = write.write(left).await?;
            unsafe { ring_buf.as_mut_base().advance(count) };
        } else {
            // both empty, break
            return Ok(());
        };
    }
}
