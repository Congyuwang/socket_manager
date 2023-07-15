#![feature(unboxed_closures)]
#![feature(fn_traits)]
#![allow(improper_ctypes)]

mod c_api;

use crate::c_api::callbacks::WakerObj;
use async_ringbuf::{AsyncHeapConsumer, AsyncHeapProducer, AsyncHeapRb};
use dashmap::DashMap;
use futures::FutureExt;
use std::future::Future;
use std::net::SocketAddr;
use std::num::NonZeroUsize;
use std::pin::{pin, Pin};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::task::Poll::Ready;
use std::task::{Context, Poll};
use std::time::Duration;
use tokio::io::{AsyncBufReadExt, AsyncWrite, AsyncWriteExt, BufReader, BufWriter};
use tokio::net::tcp::{OwnedReadHalf, OwnedWriteHalf};
use tokio::net::{TcpListener, TcpStream};
use tokio::runtime;
use tokio::runtime::{Handle, Runtime};
use tokio::sync::mpsc::{UnboundedReceiver, UnboundedSender};
use tokio::sync::{mpsc, oneshot};
use tokio::task::JoinHandle;
use tokio::time::MissedTickBehavior;
use tracing_subscriber::EnvFilter;

const MAX_WORKER_THREADS: usize = 256;
const SOCKET_LOG: &str = "SOCKET_LOG";
const RING_BUFFER_SIZE: usize = 256 * 1024; // 256KB
const MIN_MSG_BUFFER_SIZE: usize = 1024 * 8; // 8KB
const MAX_MSG_BUFFER_SIZE: usize = 1024 * 1024 * 8; // 8MB
const SHUTDOWN_TIMEOUT: Duration = Duration::from_secs(5);

/// The Main Struct of the Library.
///
/// This struct is thread safe.
pub struct CSocketManager {
    cmd_send: UnboundedSender<Command>,

    // use has_joined to fence the join_handle,
    // both should only be accessed by the `join` method.
    has_joined: AtomicBool,
    join_handle: Option<std::thread::JoinHandle<()>>,
}

/// Drop the sender to close the connection.
pub struct CMsgSender {
    pub(crate) cmd: UnboundedSender<SendCommand>,
    pub(crate) buf_prd: AsyncHeapProducer<u8>,
    handle: Handle,
}

impl CMsgSender {
    /// The blocking API of sending bytes.
    /// Do not use this method in the callback (i.e. async context),
    /// as it might block.
    pub fn send_block(&mut self, bytes: &[u8]) -> std::io::Result<()> {
        const MAX_SPIN: i32 = 1000;
        let mut spin_count = 0;
        let mut written = 0usize;
        while written < bytes.len() && spin_count < MAX_SPIN {
            let n = self.buf_prd.as_mut_base().push_slice(&bytes[written..]);
            // n = 0, check if closed
            if n == 0 && self.buf_prd.is_closed() {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::Other,
                    "connection closed",
                ));
            }
            written += n;
            spin_count += 1;
        }
        // unfinished after spinning, enter into future
        if written < bytes.len() {
            self.handle.clone().block_on(self.buf_prd.write_all(&bytes[written..]))?;
        }
        Ok(())
    }

    /// Try sending bytes.
    ///
    /// Returning -1 to indicate pending.
    pub fn try_send(&mut self, bytes: &[u8], waker_obj: Option<WakerObj>) -> std::io::Result<i64> {
        let n = self.buf_prd.as_mut_base().push_slice(bytes);
        match (n, waker_obj) {
            // no bytes written, wait on the waker
            (0, Some(waker_obj)) => {
                let waker = unsafe { waker_obj.make_waker() };
                match pin!(self.buf_prd.wait_free(RING_BUFFER_SIZE / 2))
                    .poll(&mut Context::from_waker(&waker))
                {
                    Ready(_) => {
                        if self.buf_prd.is_closed() {
                            Ok(0)
                        } else {
                            let n = self.buf_prd.as_mut_base().push_slice(bytes);
                            Ok(n as i64)
                        }
                    }
                    Poll::Pending => Ok(-1),
                }
            }
            // no bytes written, no waker, return 0
            (0, None) => Ok(0),
            // some bytes written, return the number of bytes written
            (n, _) => Ok(n as i64),
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

pub enum SendCommand {
    Flush,
}

/// Msg struct for the on_msg callback.
pub struct Msg<'a> {
    bytes: &'a [u8],
}

/// internal commands
enum Command {
    Listen { addr: SocketAddr },
    Connect { addr: SocketAddr },
    CancelListen { addr: SocketAddr },
    Abort,
}

/// The connection struct for the on_conn callback.
pub struct Conn<OnMsg> {
    consumed: AtomicBool,
    inner: Option<ConnInner<OnMsg>>,
}

struct ConnInner<OnMsg> {
    conn_config_setter: oneshot::Sender<(OnMsg, ConnConfig)>,
    send: CMsgSender,
}

impl<OnMsg> Conn<OnMsg> {
    fn new(conn_config_setter: oneshot::Sender<(OnMsg, ConnConfig)>, send: CMsgSender) -> Self {
        Self {
            consumed: AtomicBool::new(false),
            inner: Some(ConnInner {
                conn_config_setter,
                send,
            }),
        }
    }
}

/// Connection configuration
#[derive(Copy, Clone)]
pub struct ConnConfig {
    write_flush_interval: Option<Duration>,
    read_msg_flush_interval: Option<Duration>,
    msg_buffer_size: Option<NonZeroUsize>,
}

impl<OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + 'static + Clone> Conn<OnMsg> {
    /// This function should be called only once.
    pub fn start_connection(
        &mut self,
        on_msg: OnMsg,
        config: ConnConfig,
    ) -> std::io::Result<CMsgSender> {
        self.consumed
            .compare_exchange(false, true, Ordering::Acquire, Ordering::Relaxed)
            .map_err(|_| {
                std::io::Error::new(
                    std::io::ErrorKind::Other,
                    "calling `start_connection` after connection consumed",
                )
            })?;
        let conn = self.inner.take().unwrap();
        if conn.conn_config_setter.send((on_msg, config)).is_err() {
            // if 'OnConnect' callback throws error before calling start_connection,
            // might result in conn_config receiver being dropped before this.
            tracing::error!("callback config setter send failed");
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "callback config setter send failed",
            ));
        }
        Ok(conn.send)
    }

    /// close the connection without using it.
    pub fn close(&mut self) -> std::io::Result<()> {
        self.consumed
            .compare_exchange(false, true, Ordering::Acquire, Ordering::Relaxed)
            .map_err(|_| {
                std::io::Error::new(
                    std::io::ErrorKind::Other,
                    "calling `close` after connection consumed",
                )
            })?;
        drop(self.inner.take().unwrap());
        Ok(())
    }
}

/// Connection state changes for the on_conn callback.
pub enum ConnState<OnMsg> {
    /// sent on connection success
    OnConnect {
        local_addr: SocketAddr,
        peer_addr: SocketAddr,
        conn: Conn<OnMsg>,
    },
    /// sent on connection closed
    /// `OnConnection` and `OnConnectionClose` are
    /// sent within `handle connection`.
    OnConnectionClose {
        local_addr: SocketAddr,
        peer_addr: SocketAddr,
    },
    /// sent on listen error
    OnListenError {
        addr: SocketAddr,
        error: std::io::Error,
    },
    /// sent on connection error
    /// `OnConnect` and `OnConnectError`
    /// are mutually exclusive.
    OnConnectError {
        addr: SocketAddr,
        error: std::io::Error,
    },
}

/// Internal connection state.
struct ConnectionState {
    listeners: DashMap<SocketAddr, oneshot::Receiver<()>>,
    connections: DashMap<(SocketAddr, SocketAddr), oneshot::Receiver<()>>,
}

impl ConnectionState {
    fn new() -> Arc<Self> {
        Arc::new(ConnectionState {
            listeners: DashMap::new(),
            connections: DashMap::new(),
        })
    }

    // called only in abort!
    fn stop_all(&self) {
        // first drop all listeners
        self.listeners.clear();
        // then drop all connections
        self.connections.clear();
    }
}

impl CSocketManager {
    /// start background threads to run the runtime
    ///
    /// # Arguments
    /// - `on_conn`: the callback for connection state changes.
    /// - `n_threads`: number of threads to run the runtime:
    ///    - 0: use the default number of threads.
    ///    - 1: use the current thread.
    ///    - \>1: use the specified number of threads.
    pub fn init<
        OnConn: Fn(ConnState<OnMsg>) -> Result<(), String> + Send + Sync + 'static + Clone,
        OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + Sync + Unpin + 'static + Clone,
    >(
        on_conn: OnConn,
        n_threads: usize,
    ) -> std::io::Result<CSocketManager> {
        let _ = tracing_subscriber::fmt()
            .with_env_filter(
                EnvFilter::builder()
                    .with_env_var(SOCKET_LOG)
                    .from_env_lossy(),
            )
            .try_init();
        let runtime = start_runtime(n_threads)?;
        let (cmd_send, cmd_recv) = mpsc::unbounded_channel::<Command>();
        let connection_state = ConnectionState::new();
        let join_handle = Some(std::thread::spawn(move || {
            let handle = runtime.handle();
            runtime.block_on(main(cmd_recv, handle, on_conn, connection_state));
            tracing::info!("shutting down socket_manager");
            runtime.shutdown_timeout(SHUTDOWN_TIMEOUT);
            tracing::info!("socket_manager stopped");
        }));
        Ok(CSocketManager {
            cmd_send,
            has_joined: AtomicBool::new(false),
            join_handle,
        })
    }

    pub fn listen_on_addr(&self, addr: SocketAddr) -> std::io::Result<()> {
        self.cmd_send.send(Command::Listen { addr }).map_err(|_| {
            std::io::Error::new(std::io::ErrorKind::Other, "socket manager has stopped")
        })
    }

    pub fn connect_to_addr(&self, addr: SocketAddr) -> std::io::Result<()> {
        self.cmd_send.send(Command::Connect { addr }).map_err(|_| {
            std::io::Error::new(std::io::ErrorKind::Other, "socket manager has stopped")
        })
    }

    pub fn cancel_listen_on_addr(&self, addr: SocketAddr) -> std::io::Result<()> {
        self.cmd_send
            .send(Command::CancelListen { addr })
            .map_err(|_| {
                std::io::Error::new(std::io::ErrorKind::Other, "socket manager has stopped")
            })
    }

    /// Calling this function will stop the runtime and all the background threads.
    ///
    /// This function will not block the current thread.
    pub fn abort(&mut self, wait: bool) -> std::io::Result<()> {
        let _ = self.cmd_send.send(Command::Abort);
        if wait {
            self.join()?;
        }
        Ok(())
    }

    /// Join the socket manager to the current thread.
    ///
    /// This function will block the current thread until
    /// `abort` is called from another thread.
    ///
    /// It returns immediately if called a second time.
    pub fn join(&mut self) -> std::io::Result<()> {
        match self
            .has_joined
            .compare_exchange(false, true, Ordering::Acquire, Ordering::Relaxed)
        {
            Ok(_) => self.join_handle.take().unwrap().join().map_err(|e| {
                tracing::error!("socket manager join returned error: {:?}", e);
                std::io::Error::new(
                    std::io::ErrorKind::Other,
                    format!("socket manager join returned error: {:?}", e),
                )
            }),
            Err(_) => Ok(()),
        }
    }
}

impl Drop for CSocketManager {
    fn drop(&mut self) {
        let _ = self.abort(true);
    }
}

/// The main loop running in the background.
async fn main<
    OnConn: Fn(ConnState<OnMsg>) -> Result<(), String> + Send + Sync + 'static + Clone,
    OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + Sync + Unpin + 'static + Clone,
>(
    mut cmd_recv: UnboundedReceiver<Command>,
    handle: &Handle,
    on_conn: OnConn,
    connection_state: Arc<ConnectionState>,
) {
    while let Some(cmd) = cmd_recv.recv().await {
        match cmd {
            Command::Listen { addr } => listen_on_addr(handle, addr, &on_conn, &connection_state),
            Command::Connect { addr } => connect_to_addr(handle, addr, &on_conn, &connection_state),
            Command::CancelListen { addr } => {
                if connection_state.listeners.remove(&addr).is_none() {
                    tracing::warn!("cancel listening failed: not listening to {addr}");
                }
            }
            Command::Abort => {
                tracing::debug!("aborting socket manager");
                // send stop signals to all listeners and connections
                connection_state.stop_all();
                // single threaded runtime will pause all spawned tasks after
                // `block_on` returns. So we must use multi-threaded runtime.
                break;
            }
        }
    }
}

/// Start runtime
fn start_runtime(n_threads: usize) -> std::io::Result<Runtime> {
    let n_thread = n_threads.min(MAX_WORKER_THREADS);
    match n_thread {
        0 => {
            let n_cpu =
                std::thread::available_parallelism().unwrap_or(NonZeroUsize::new(1).unwrap());
            tracing::info!("socket manager started runtime with {n_cpu} threads");
            runtime::Builder::new_multi_thread()
                .enable_all()
                .worker_threads(n_cpu.get())
                .build()
        }
        1 => {
            tracing::info!("socket manager started runtime with single thread");
            runtime::Builder::new_current_thread().enable_all().build()
        }
        n => {
            tracing::info!("socket manager started runtime with {n} threads");
            runtime::Builder::new_multi_thread()
                .enable_all()
                .worker_threads(n)
                .build()
        }
    }
}

/// This function connects to a port.
fn connect_to_addr<
    OnConn: Fn(ConnState<OnMsg>) -> Result<(), String> + Send + 'static + Clone,
    OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + Unpin + 'static,
>(
    handle: &Handle,
    addr: SocketAddr,
    on_conn: &OnConn,
    connection_state: &Arc<ConnectionState>,
) {
    let handle = handle.clone();
    let on_conn = on_conn.clone();
    let connection_state = connection_state.clone();

    // attempt to connect to address, and obtain connection info
    let connect_result = async move {
        let stream = TcpStream::connect(addr).await?;
        let (local, peer) = get_address_from_stream(&stream)?;
        Ok::<(TcpStream, SocketAddr, SocketAddr), std::io::Error>((stream, local, peer))
    };

    // spawn connecting task
    handle.clone().spawn(async move {
        // trying to connect to address
        match connect_result.await {
            Ok((stream, local_addr, peer_addr)) => {
                handle_connection(
                    local_addr,
                    peer_addr,
                    handle,
                    stream,
                    on_conn,
                    connection_state,
                );
            }
            Err(error) => {
                tracing::warn!("error connecting tp addr={addr}: {error}");
                let _ = on_conn(ConnState::OnConnectError { addr, error });
            }
        }
    });
}

/// This function listens on a port.
fn listen_on_addr<
    OnConn: Fn(ConnState<OnMsg>) -> Result<(), String> + Send + Sync + 'static + Clone,
    OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + Sync + Unpin + 'static + Clone,
>(
    handle: &Handle,
    addr: SocketAddr,
    on_conn: &OnConn,
    connection_state: &Arc<ConnectionState>,
) {
    let handle = handle.clone();
    let on_conn = on_conn.clone();
    let connection_state = connection_state.clone();

    // spawn listening task
    handle.clone().spawn(async move {
        // bind to address
        let listener = match TcpListener::bind(addr).await {
            Ok(l) => l,
            Err(e) => {
                tracing::warn!("error listening on addr={addr}: {e}");
                let _ = on_conn(ConnState::OnListenError { addr, error: e });
                return;
            }
        };

        // insert a cancel handle
        tracing::info!("listening on addr={addr}");
        let (canceled, cancel) = oneshot::channel::<()>();
        connection_state.listeners.insert(addr, cancel);

        // async loop to accept connections
        accept_connections(
            addr,
            listener,
            &handle,
            canceled,
            &on_conn,
            &connection_state,
        )
        .await;
    });
}

async fn accept_connections<
    OnConn: Fn(ConnState<OnMsg>) -> Result<(), String> + Send + 'static + Clone,
    OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + Unpin + 'static + Clone,
>(
    addr: SocketAddr,
    listener: TcpListener,
    handle: &Handle,
    mut canceled: oneshot::Sender<()>,
    on_conn: &OnConn,
    connection_state: &Arc<ConnectionState>,
) {
    loop {
        tokio::select! {
            biased;
            Ok((stream, _)) = listener.accept() => {
                let on_conn = on_conn.clone();
                match get_address_from_stream(&stream) {
                    Err(e) => {
                        tracing::error!("failed to read address from stream: {e}");
                    }
                    Ok((local_addr, peer_addr)) => {
                        let connection_state = connection_state.clone();
                        handle_connection(local_addr, peer_addr, handle.clone(), stream, on_conn, connection_state);
                    }
                }
            }
            _ = canceled.closed() => {
                tracing::info!("cancel listening on addr success (addr={addr})");
                break;
            }
            else => {
                tracing::error!("error accepting connection listening to {addr}");
            }
        }
    }
}

/// This function handles connection from a client.
fn handle_connection<
    OnConn: Fn(ConnState<OnMsg>) -> Result<(), String> + Send + 'static + Clone,
    OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + Unpin + 'static,
>(
    local_addr: SocketAddr,
    peer_addr: SocketAddr,
    handle: Handle,
    stream: TcpStream,
    on_conn: OnConn,
    connection_state: Arc<ConnectionState>,
) {
    let (send, recv) = mpsc::unbounded_channel::<SendCommand>();
    let (conn_config_setter, conn_config) = oneshot::channel::<(OnMsg, ConnConfig)>();
    let (buf_prd, ring_buf) = AsyncHeapRb::new(RING_BUFFER_SIZE).split();
    let send = CMsgSender {
        cmd: send,
        buf_prd,
        handle: handle.clone(),
    };

    let on_conn_clone = on_conn.clone();
    // Call `on_conn` callback, and wait for user to call `start` on connection.
    // Return the OnMsg callback and conn_config.
    let wait_for_start = async move {
        on_conn(ConnState::OnConnect {
            local_addr,
            peer_addr,
            conn: Conn::new(conn_config_setter, send),
        })
        .map_err(|_| ())?;

        // Try to wait for connection start signal.
        // note: this returns error only if conn_config_setter is dropped
        // without sending a value (connection dropped without calling start).
        let (on_msg, conn_config) = conn_config.await.map_err(|_| {
            tracing::warn!("connection dropped (local={local_addr}, peer={peer_addr})");
        })?;

        Ok::<(OnMsg, ConnConfig), ()>((on_msg, conn_config))
    };

    handle.clone().spawn(async move {
        tracing::info!("new connection: local_addr={local_addr}, peer_addr={peer_addr}");

        if let Ok((on_msg, config)) = wait_for_start.await {
            // spawn reader and writer
            let (stop, stopper) = oneshot::channel::<()>();
            let (read, write) = stream.into_split();
            let writer = handle.spawn(handle_writer(write, recv, ring_buf, config, stop));
            let reader = handle.spawn(handle_reader(read, on_msg, config));

            // insert the stopper into connection_state
            connection_state
                .connections
                .insert((local_addr, peer_addr), stopper);

            // join reader and writer
            join_reader_writer((writer, reader), (local_addr, peer_addr)).await;

            // remove connection from connection_state after reader and writer are done
            connection_state
                .connections
                .remove(&(local_addr, peer_addr));
        }

        tracing::info!("connection closed: local_addr={local_addr}, peer_addr={peer_addr}");
        let _ = on_conn_clone(ConnState::OnConnectionClose {
            local_addr,
            peer_addr,
        });
    });
}

/// Receive bytes from recv and write to WriteHalf of TcpStream.
async fn handle_writer(
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
                if ring_buf.is_closed() {
                    // n = 0, now check if the ring buffer is closed
                    break 'closed;
                }
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
                    write.flush().await?;
                    // disable ticked flush when there is no data.
                    has_data = false;
                }
                _ = ring_buf.wait(RING_BUFFER_SIZE / 2) => {
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
                    write.flush().await?;
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
    write.flush().await?;
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
                if ring_buf.is_closed() {
                    // n = 0, now check if the ring buffer is closed
                    break 'closed;
                }
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
                    write.flush().await?;
                }
                _ = ring_buf.wait(RING_BUFFER_SIZE / 2) => {
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
    write.flush().await?;
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

/// Receive bytes ReadHalf of TcpStream and call `on_msg` callback.
async fn handle_reader<OnMsg: Fn(Msg<'_>) -> Result<(), String> + Send + Unpin + 'static>(
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

/// On connection end, remove connection from connection state.
async fn join_reader_writer(
    (writer, reader): (
        JoinHandle<std::io::Result<()>>,
        JoinHandle<std::io::Result<()>>,
    ),
    (local_addr, peer_addr): (SocketAddr, SocketAddr),
) {
    let writer_abort = writer.abort_handle();
    let reader_abort = reader.abort_handle();
    let mut writer = writer.fuse();
    let mut reader = reader.fuse();
    loop {
        tokio::select! {
            w = &mut writer => {
                if let Err(e) = w {
                    tracing::error!("writer stopped on error ({e}), local={local_addr}, peer={peer_addr}");
                } else {
                    tracing::debug!("writer stopped local={local_addr}, peer={peer_addr}");
                }
                reader_abort.abort();
                break;
            }
            r = &mut reader => {
                if let Err(e) = r {
                    tracing::error!("reader stopped on error ({e}), local={local_addr}, peer={peer_addr}");
                } else {
                    tracing::debug!("reader stopped local={local_addr}, peer={peer_addr}");
                }
                writer_abort.abort();
                break;
            }
        }
    }
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

fn get_address_from_stream(stream: &TcpStream) -> std::io::Result<(SocketAddr, SocketAddr)> {
    let local = stream.local_addr()?;
    let peer = stream.peer_addr()?;
    Ok((local, peer))
}
