use crate::buf_read_write::BufReadWrite;
use crate::conn::ConnConfig;
use crate::Msg;
use std::pin::Pin;
use std::task::Poll::Ready;
use std::task::{ready, Context, Poll, Waker};
use std::time::Duration;
use tokio::io::AsyncWrite;
use tokio::net::tcp::OwnedReadHalf;
use tokio::time::MissedTickBehavior;

pub const MIN_MSG_BUFFER_SIZE: usize = 1024 * 8;
pub const MAX_MSG_BUFFER_SIZE: usize = 1024 * 1024 * 8;

/// Receive bytes ReadHalf of TcpStream and call `on_msg` callback.
pub(crate) async fn handle_reader<
    OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + Unpin + 'static,
>(
    read: OwnedReadHalf,
    on_msg: OnMsg,
    config: ConnConfig,
) -> std::io::Result<()> {
    let msg_buf_size = config
        .msg_buffer_size
        .min(MAX_MSG_BUFFER_SIZE)
        .max(MIN_MSG_BUFFER_SIZE);
    let duration = config.read_msg_flush_interval;
    if duration.is_zero() {
        handle_reader_no_auto_flush(read, on_msg, msg_buf_size).await
    } else {
        handle_reader_auto_flush(read, on_msg, duration, msg_buf_size).await
    }
}

/// Has write buffer with auto flush.
async fn handle_reader_auto_flush<
    OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + Unpin + 'static,
>(
    read: OwnedReadHalf,
    on_msg: OnMsg,
    duration: Duration,
    msg_buf_size: usize,
) -> std::io::Result<()> {
    debug_assert!(!duration.is_zero());
    let writer = OnMsgWrite { on_msg };
    let mut read_write = BufReadWrite::new(read, writer, msg_buf_size);
    let mut flush_tick = tokio::time::interval(duration);
    let mut has_data = false;
    flush_tick.set_missed_tick_behavior(MissedTickBehavior::Skip);

    loop {
        tokio::select! {
            biased;
            n = read_write.fill_flush() => {
                if n? == 0 {
                    break;
                }
                has_data = true;
            }
            _ = flush_tick.tick(), if has_data => {
                read_write.flush().await?;
                // disable ticked flush when there is no data.
                has_data = false;
            }
        }
    }

    read_write.flush().await?;
    Ok(())
}

/// Has write buffer but no auto flush (small messages might get stuck).
/// Call `OnMsg` on batch. This is not recommended.
async fn handle_reader_no_auto_flush<
    OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + Unpin + 'static,
>(
    read: OwnedReadHalf,
    on_msg: OnMsg,
    msg_buf_size: usize,
) -> std::io::Result<()> {
    let writer = OnMsgWrite { on_msg };
    let mut read_write = BufReadWrite::new(read, writer, msg_buf_size);
    while read_write.fill_flush().await? != 0 {}
    read_write.flush().await?;
    Ok(())
}

/// async write wrapper for on_msg
struct OnMsgWrite<OnMsg> {
    on_msg: OnMsg,
}

impl<OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + 'static> AsyncWrite
    for OnMsgWrite<OnMsg>
{
    fn poll_write(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        bytes: &[u8],
    ) -> Poll<Result<usize, std::io::Error>> {
        let on_msg = &self.on_msg;
        let result = ready!(on_msg(Msg { bytes }, cx.waker().clone()));
        Ready(result.map_err(|e| {
            std::io::Error::new(
                std::io::ErrorKind::Other,
                format!("error writing message: {e}"),
            )
        }))
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
