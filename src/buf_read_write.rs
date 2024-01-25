use std::future::Future;
use std::pin::Pin;
use std::task::Poll::Ready;
use std::task::{Context, Poll};
use tokio::io::{AsyncRead, AsyncReadExt, AsyncWrite};

pub struct BufReadWrite<R, W> {
    reader: R,
    writer: W,
    buf: Vec<u8>,
    r_offset: usize,
    w_offset: usize,
}

impl<R: AsyncRead + Unpin, W: AsyncWrite + Unpin> BufReadWrite<R, W> {
    pub fn new(reader: R, writer: W, write_buffer_size: usize) -> BufReadWrite<R, W> {
        assert!(write_buffer_size > 0);
        Self {
            buf: vec![0; write_buffer_size],
            r_offset: 0,
            w_offset: 0,
            reader,
            writer,
        }
    }

    /// Fill the buffer with data from the reader,
    /// and flush the data to the writer if the buffer
    /// is full.
    ///
    /// Cancellation safe since partial flush progress is recorded.
    pub async fn fill_flush(&mut self) -> std::io::Result<usize> {
        // buf is full
        if self.w_offset == self.buf.len() {
            self.flush().await?;
            // write_all resets: w_offset = 0 and r_offset = 0
        }
        let n = self.reader.read(&mut self.buf[self.w_offset..]).await?;
        self.w_offset += n;
        Ok(n)
    }

    /// flush the buffer
    pub async fn flush(&mut self) -> std::io::Result<()> {
        write_all(
            &mut self.writer,
            &self.buf,
            &mut self.r_offset,
            &mut self.w_offset,
        )
        .await
        // write_all resets: w_offset = 0 and r_offset = 0
    }
}

/// Attempt to write all bytes in the buffer.
/// Always update the read offset before returning,
/// making sure this future is cancellation safe.
///
/// On complete (i.e., all bytes are written), reset the
/// read offset to 0.
fn write_all<'a, W: ?Sized>(
    writer: &'a mut W,
    buf: &'a [u8],
    r_offset: &'a mut usize,
    w_offset: &'a mut usize,
) -> WriteAll<'a, W> {
    WriteAll {
        writer,
        buf,
        r_offset,
        w_offset,
    }
}

#[must_use = "futures do nothing unless you `.await` or poll them"]
struct WriteAll<'a, W: ?Sized> {
    writer: &'a mut W,
    buf: &'a [u8],
    r_offset: &'a mut usize,
    w_offset: &'a mut usize,
}

impl<W: AsyncWrite + ?Sized + Unpin> Future for WriteAll<'_, W> {
    type Output = std::io::Result<()>;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        let mut pos = *self.r_offset;
        let end = *self.w_offset;
        while pos < end {
            let this = &mut *self;
            let buf = &this.buf[pos..end];
            match Pin::new(&mut this.writer).poll_write(cx, buf) {
                Ready(Ok(n)) => {
                    // update local read progress
                    pos += n;
                    if n == 0 {
                        // update read progress
                        *this.r_offset = pos;
                        return Ready(Err(std::io::ErrorKind::WriteZero.into()));
                    }
                }
                Ready(Err(e)) => {
                    // update read progress
                    *this.r_offset = pos;
                    return Ready(Err(e));
                }
                Poll::Pending => {
                    // update read progress
                    *this.r_offset = pos;
                    return Poll::Pending;
                }
            };
        }
        // all flushed, reset the read offset
        assert_eq!(pos, end);
        *self.r_offset = 0;
        *self.w_offset = 0;
        Ready(Ok(()))
    }
}
