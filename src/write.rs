use crate::conn::ConnConfig;
use crate::msg_sender::MsgRcv;
use crate::read::MIN_MSG_BUFFER_SIZE;
use crate::AsyncHeapConsumer;
use async_ringbuf::traits::{AsyncConsumer, Consumer, Observer};
use std::time::Duration;
use tokio::io::AsyncWriteExt;
use tokio::net::tcp::OwnedWriteHalf;
use tokio::time::MissedTickBehavior;

/// Receive bytes from recv and write to WriteHalf of TcpStream.
pub(crate) async fn handle_writer(
    write: OwnedWriteHalf,
    recv: MsgRcv,
    config: ConnConfig,
) -> std::io::Result<()> {
    let duration = config.write_flush_interval;
    if duration.is_zero() {
        handle_writer_no_auto_flush(write, recv).await
    } else {
        handle_writer_auto_flush(write, recv, duration).await
    }
}

async fn handle_writer_auto_flush(
    mut write: OwnedWriteHalf,
    mut recv: MsgRcv,
    duration: Duration,
) -> std::io::Result<()> {
    debug_assert!(!duration.is_zero());
    let mut flush_tick = tokio::time::interval(duration);
    flush_tick.set_missed_tick_behavior(MissedTickBehavior::Skip);

    while let Some(mut ring) = recv.rings.recv().await {
        let mut tick_flush = true;
        loop {
            tokio::select! {
                biased;
                // !has_data => wait for has_data
                // has_data => wait for write_threshold
                _ = ring.wait_occupied((tick_flush as usize) * MIN_MSG_BUFFER_SIZE + (!tick_flush as usize)) => {
                    if ring.is_closed() {
                        break;
                    }
                    tick_flush = ring.occupied_len() < MIN_MSG_BUFFER_SIZE;
                    if !tick_flush {
                        flush(&mut ring, &mut write).await?;
                    }
                }
                // tick flush
                _ = flush_tick.tick(), if tick_flush => {
                    flush(&mut ring, &mut write).await?;
                    tick_flush = false;
                }
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
) -> std::io::Result<()> {
    while let Some(mut ring) = recv.rings.recv().await {
        loop {
            ring.wait_occupied(MIN_MSG_BUFFER_SIZE).await;
            if ring.is_closed() {
                break;
            }
            flush(&mut ring, &mut write).await?;
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
        let (left, _) = ring_buf.as_slices();
        if !left.is_empty() {
            let count = write.write(left).await?;
            unsafe { ring_buf.advance_read_index(count) };
        } else {
            // both empty, break
            return Ok(());
        };
    }
}
