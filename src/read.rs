use crate::conn::ConnConfig;
use std::pin::Pin;
use std::task::Poll::Ready;
use std::task::{Context, Poll};
use std::time::Duration;
use tokio::io::{AsyncBufReadExt, AsyncWrite, AsyncWriteExt, BufReader, BufWriter};
use tokio::net::tcp::OwnedReadHalf;
use tokio::time::MissedTickBehavior;
use crate::Msg;

pub const MIN_MSG_BUFFER_SIZE: usize = 1024 * 8; // 8KB
pub const MAX_MSG_BUFFER_SIZE: usize = 1024 * 1024 * 8; // 8MB

/// Receive bytes ReadHalf of TcpStream and call `on_msg` callback.
pub(crate) async fn handle_reader<OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + Unpin + 'static>(
    read: OwnedReadHalf,
    on_msg: OnMsg,
    config: ConnConfig,
) -> std::io::Result<()> {
    if let Some(msg_buf_size) = config.msg_buffer_size {
        let msg_buf_size = msg_buf_size
            .get()
            .min(MAX_MSG_BUFFER_SIZE)
            .max(MIN_MSG_BUFFER_SIZE);
        match config.read_msg_flush_interval {
            None => handle_reader_no_auto_flush(read, on_msg, msg_buf_size).await,
            Some(duration) => {
                if duration.is_zero() {
                    handle_reader_no_auto_flush(read, on_msg, msg_buf_size).await
                } else {
                    handle_reader_auto_flush(read, on_msg, duration, msg_buf_size).await
                }
            }
        }
    } else {
        handle_reader_no_buf(read, on_msg).await
    }
}

/// Has write buffer with auto flush.
async fn handle_reader_auto_flush<
    OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + Unpin + 'static,
>(
    read: OwnedReadHalf,
    on_msg: OnMsg,
    duration: Duration,
    msg_buf_size: usize,
) -> std::io::Result<()> {
    debug_assert!(!duration.is_zero());
    let recv_buffer_size = socket2::SockRef::from(read.as_ref()).recv_buffer_size()?;
    tracing::trace!("recv buffer size: {}", recv_buffer_size);
    let mut buf_reader = BufReader::with_capacity(recv_buffer_size, read);
    let mut msg_writer = BufWriter::with_capacity(msg_buf_size, OnMsgWrite { on_msg });
    let mut flush_tick = tokio::time::interval(duration);
    let mut has_data = false;
    flush_tick.set_missed_tick_behavior(MissedTickBehavior::Skip);

    loop {
        tokio::select! {
            biased;
            fill_result = buf_reader.fill_buf() => {
                let bytes = fill_result?;
                let n = bytes.len();
                if n == 0 {
                    break;
                }
                msg_writer.write_all(bytes).await?;
                tracing::trace!("received {n} bytes");
                buf_reader.consume(n);
                // enable ticked flush when there is data.
                has_data = true;
            }
            _ = flush_tick.tick(), if has_data => {
                msg_writer.flush().await?;
                // disable ticked flush when there is no data.
                has_data = false;
            }
        }
    }

    msg_writer.flush().await?;
    msg_writer.shutdown().await?;
    Ok(())
}

/// Has write buffer but no auto flush (small messages might get stuck).
/// Call `OnMsg` on batch. This is not recommended.
async fn handle_reader_no_auto_flush<
    OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + Unpin + 'static,
>(
    read: OwnedReadHalf,
    on_msg: OnMsg,
    msg_buf_size: usize,
) -> std::io::Result<()> {
    let recv_buffer_size = socket2::SockRef::from(read.as_ref()).recv_buffer_size()?;
    tracing::trace!("recv buffer size: {}", recv_buffer_size);
    let mut buf_reader = BufReader::with_capacity(recv_buffer_size, read);
    let mut msg_writer = BufWriter::with_capacity(msg_buf_size, OnMsgWrite { on_msg });

    loop {
        let bytes = buf_reader.fill_buf().await?;
        let n = bytes.len();
        if n == 0 {
            break;
        }
        tracing::trace!("received {n} bytes");
        msg_writer.write_all(bytes).await?;
        buf_reader.consume(n);
    }

    msg_writer.flush().await?;
    msg_writer.shutdown().await?;
    Ok(())
}

/// Has no write buffer, received is sent immediately.
async fn handle_reader_no_buf<OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + Unpin + 'static>(
    read: OwnedReadHalf,
    on_msg: OnMsg,
) -> std::io::Result<()> {
    let recv_buffer_size = socket2::SockRef::from(read.as_ref()).recv_buffer_size()?;
    tracing::trace!("recv buffer size: {}", recv_buffer_size);
    let mut buf_reader = BufReader::with_capacity(recv_buffer_size, read);
    loop {
        let bytes = buf_reader.fill_buf().await?;
        let n = bytes.len();
        if n == 0 {
            break;
        }
        tracing::trace!("received {n} bytes");
        if let Err(e) = on_msg(Msg { bytes }) {
            return Err(std::io::Error::new(std::io::ErrorKind::Other, e));
        }
        buf_reader.consume(n);
    }
    Ok(())
}

/// async write wrapper for on_msg
struct OnMsgWrite<OnMsg> {
    on_msg: OnMsg,
}

impl<OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + 'static> AsyncWrite for OnMsgWrite<OnMsg> {
    fn poll_write(
        self: Pin<&mut Self>,
        _: &mut Context<'_>,
        bytes: &[u8],
    ) -> Poll<Result<usize, std::io::Error>> {
        let on_msg = &self.on_msg;
        let result = match on_msg(Msg { bytes }) {
            Ok(_) => Ok(bytes.len()),
            Err(e) => Err(std::io::Error::new(std::io::ErrorKind::Other, e)),
        };
        Ready(result)
    }

    fn poll_flush(self: Pin<&mut Self>, _: &mut Context<'_>) -> Poll<Result<(), std::io::Error>> {
        Ready(Ok(()))
    }

    fn poll_shutdown(
        self: Pin<&mut Self>,
        _: &mut Context<'_>,
    ) -> Poll<Result<(), std::io::Error>> {
        Ready(Ok(()))
    }
}
