#![feature(unboxed_closures)]
#![feature(fn_traits)]

mod c_api;

use dashmap::DashMap;
use futures::FutureExt;
use std::net::SocketAddr;
use std::num::NonZeroUsize;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;
use tokio::io::{AsyncReadExt, AsyncWriteExt, BufReader, BufWriter};
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
const BUFFER_SIZE: usize = 1024 * 8;

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
    pub(crate) send: UnboundedSender<SendCommand>,
}

pub enum SendCommand {
    Send(Vec<u8>),
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
    has_started: AtomicBool,
    inner: Option<ConnInner<OnMsg>>,
}

struct ConnInner<OnMsg> {
    conn_config_setter: oneshot::Sender<(OnMsg, ConnConfig)>,
    send: CMsgSender,
}

/// Connection configuration
pub struct ConnConfig {
    write_flush_interval: Option<Duration>,
}

impl<OnMsg: Fn(Msg<'_>) + Send + 'static + Clone> Conn<OnMsg> {
    /// This function should be called only once.
    pub fn start_connection(
        &mut self,
        on_msg: OnMsg,
        config: ConnConfig,
    ) -> std::io::Result<CMsgSender> {
        if self.has_started.swap(true, Ordering::SeqCst) {
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "connection already started",
            ));
        }
        let conn = self.inner.take().unwrap();
        if conn.conn_config_setter.send((on_msg, config)).is_err() {
            tracing::error!("callback config setter failed (likely remote connection closed)");
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "callback config setter failed (likely remote connection closed)",
            ));
        }
        Ok(conn.send)
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
    OnConnectionClose {
        local_addr: SocketAddr,
        peer_addr: SocketAddr,
    },
    /// sent on listen error
    OnListenError {
        addr: SocketAddr,
        error: std::io::Error,
    },
    /// sent on connection error.
    /// won't sent if connection successfully established.
    OnConnectError {
        addr: SocketAddr,
        error: std::io::Error,
    },
}

/// Internal connection state.
struct ConnectionState {
    listeners: DashMap<SocketAddr, oneshot::Receiver<()>>,
}

impl ConnectionState {
    fn new() -> Arc<Self> {
        Arc::new(ConnectionState {
            listeners: DashMap::new(),
        })
    }
}

impl CSocketManager {
    /// start background threads to run the runtime
    pub fn init<
        OnConn: Fn(ConnState<OnMsg>) + Send + 'static + Clone,
        OnMsg: Fn(Msg<'_>) + Send + 'static + Clone,
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
            runtime.block_on(main(cmd_recv, handle, on_conn, connection_state))
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
    pub fn abort(&self) -> std::io::Result<()> {
        self.cmd_send.send(Command::Abort).map_err(|_| {
            std::io::Error::new(std::io::ErrorKind::Other, "socket manager has stopped")
        })
    }

    /// Join the socket manager to the current thread.
    ///
    /// This function will block the current thread until
    /// `abort` is called from another thread.
    ///
    /// It returns immediately if called a second time.
    pub fn join(&mut self) -> std::io::Result<()> {
        if self.has_joined.swap(true, Ordering::SeqCst) {
            return Ok(());
        }
        self.join_handle.take().unwrap().join().map_err(|e| {
            tracing::error!("socket manager join returned error: {:?}", e);
            std::io::Error::new(
                std::io::ErrorKind::Other,
                format!("socket manager join returned error: {:?}", e),
            )
        })
    }
}

impl Drop for CSocketManager {
    fn drop(&mut self) {
        let _ = self.abort();
        let handle = {
            match self.join_handle.lock() {
                Ok(mut handle) => handle.take(),
                Err(e) => {
                    tracing::error!("error locking join handle on drop: {:?}", e);
                    None
                }
            }
        };
        if let Some(handle) = handle {
            if let Err(e) = handle.join() {
                tracing::error!("error joining socket manager on drop: {:?}", e);
            }
        }
    }
}

/// The main loop running in the background.
async fn main<
    OnConn: Fn(ConnState<OnMsg>) + Send + 'static + Clone,
    OnMsg: Fn(Msg<'_>) + Send + 'static + Clone,
>(
    mut cmd_recv: UnboundedReceiver<Command>,
    handle: &Handle,
    on_conn: OnConn,
    connection_state: Arc<ConnectionState>,
) {
    while let Some(cmd) = cmd_recv.recv().await {
        match cmd {
            Command::Listen { addr } => {
                listen_on_addr(handle, addr, on_conn.clone(), connection_state.clone())
            }
            Command::Connect { addr } => connect_to_addr(handle, addr, on_conn.clone()),
            Command::CancelListen { addr } => {
                if connection_state.listeners.remove(&addr).is_none() {
                    tracing::error!("cancel listening failed: not listening to {addr}");
                }
            }
            Command::Abort => break,
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
    OnConn: Fn(ConnState<OnMsg>) + Send + 'static + Clone,
    OnMsg: Fn(Msg<'_>) + Send + 'static,
>(
    handle: &Handle,
    addr: SocketAddr,
    on_conn: OnConn,
) {
    let handle = handle.clone();
    let handle_clone = handle.clone();
    let on_conn_clone = on_conn.clone();
    // spawn listening task
    let connect_result = handle.clone().spawn(async move {
        let stream = TcpStream::connect(addr).await?;
        handle_connection(&handle, stream, on_conn_clone).await?;
        Ok::<(), std::io::Error>(())
    });
    // handle connection result
    handle_clone.spawn(async move {
        match connect_result.await {
            Ok(Err(e)) => {
                tracing::error!("error connecting to addr: {e}");
                on_conn(ConnState::OnConnectError { addr, error: e });
            }
            Err(e) => {
                tracing::error!("error joining connecting task to addr: {:?}", e);
            }
            _ => {}
        }
    });
}

/// This function listens on a port.
fn listen_on_addr<
    OnConn: Fn(ConnState<OnMsg>) + Send + 'static + Clone,
    OnMsg: Fn(Msg<'_>) + Send + 'static + Clone,
>(
    handle: &Handle,
    addr: SocketAddr,
    on_conn: OnConn,
    connection_state: Arc<ConnectionState>,
) {
    let handle = handle.clone();
    // spawn listening task
    handle.clone().spawn(async move {
        let listener = match TcpListener::bind(addr).await {
            Ok(l) => l,
            Err(e) => {
                tracing::error!("error listening on addr={addr}: {e}");
                on_conn(ConnState::OnListenError { addr, error: e });
                return;
            }
        };
        tracing::info!("listening on addr={addr}");
        let (mut canceled, cancel) = oneshot::channel::<()>();
        connection_state.listeners.insert(addr, cancel);
        loop {
            let accept = listener.accept().fuse();
            tokio::select! {
                biased;
                accept_result = accept => {
                    match accept_result {
                        Ok((stream, _)) => {
                            let on_conn = on_conn.clone();
                            if let Err(e) = handle_connection(&handle, stream, on_conn).await {
                                tracing::error!("error handling connection local={addr} ({e})");
                            }
                        }
                        Err(e) => {
                            tracing::error!("error accepting connection listening to {addr} ({e})");
                        }
                    }
                }
                _ = canceled.closed() => {
                    tracing::info!("cancel listening on addr success (addr={addr})");
                    break;
                }
            }
        }
    });
}

/// This function handles connection from a client.
async fn handle_connection<
    OnConn: Fn(ConnState<OnMsg>) + Send + 'static,
    OnMsg: Fn(Msg<'_>) + Send + 'static,
>(
    handle: &Handle,
    stream: TcpStream,
    on_conn: OnConn,
) -> std::io::Result<()> {
    // generate connection info
    let local_addr = stream.local_addr()?;
    let peer_addr = stream.peer_addr()?;
    let (send, recv) = mpsc::unbounded_channel::<SendCommand>();
    let (conn_config_setter, conn_config) = oneshot::channel::<(OnMsg, ConnConfig)>();
    let send = CMsgSender { send };

    tracing::info!("new connection: local_addr={local_addr}, peer_addr={peer_addr}");
    // call `on_conn` callback
    on_conn(ConnState::OnConnect {
        local_addr,
        peer_addr,
        conn: Conn {
            has_started: AtomicBool::new(false),
            inner: Some(ConnInner {
                conn_config_setter,
                send,
            }),
        },
    });

    // note: this returns error only if conn_config_setter is dropped
    // without sending a value, which should never happen.
    let (on_msg, conn_config) = conn_config.await.map_err(|e| {
        std::io::Error::new(
            std::io::ErrorKind::Other,
            format!("conn_config_setter is dropped ({:?})", e),
        )
    })?;

    let write_flush_interval = conn_config.write_flush_interval;

    // spawn reader and writer
    let (read, write) = stream.into_split();
    let writer = handle.spawn(handle_writer(write, recv, write_flush_interval));
    let reader = handle.spawn(handle_reader(read, on_msg));

    // join reader and writer
    handle.spawn(join_reader_writer(
        (writer, reader),
        (local_addr, peer_addr),
        on_conn,
    ));
    Ok(())
}

/// Receive bytes from recv and write to WriteHalf of TcpStream.
async fn handle_writer(
    write: OwnedWriteHalf,
    recv: UnboundedReceiver<SendCommand>,
    write_flush_interval: Option<Duration>,
) -> std::io::Result<()> {
    match write_flush_interval {
        None => handle_writer_no_auto_flush(write, recv).await,
        Some(duration) => {
            if duration.is_zero() {
                handle_writer_no_auto_flush(write, recv).await
            } else {
                handle_writer_auto_flush(write, recv, duration).await
            }
        }
    }
}

async fn handle_writer_auto_flush(
    mut write: OwnedWriteHalf,
    mut recv: UnboundedReceiver<SendCommand>,
    duration: Duration,
) -> std::io::Result<()> {
    debug_assert!(!duration.is_zero());
    write.writable().await?;
    let mut buf_writer = BufWriter::new(&mut write);
    let mut flush_tick = tokio::time::interval(duration);
    flush_tick.set_missed_tick_behavior(MissedTickBehavior::Skip);
    loop {
        tokio::select! {
            // biased towards recv, skip flush tick if missed.
            // remove the usage of random generator to improve efficiency.
            biased;
            cmd = recv.recv() => {
                match cmd {
                    Some(SendCommand::Send(msg)) => buf_writer.write_all(&msg).await?,
                    Some(SendCommand::Flush) => buf_writer.flush().await?,
                    None => break,
                }
            }
            _ = flush_tick.tick() => {
                buf_writer.flush().await?;
            }
        }
    }
    // flush and close
    buf_writer.flush().await?;
    buf_writer.shutdown().await?;
    Ok(())
}

async fn handle_writer_no_auto_flush(
    mut write: OwnedWriteHalf,
    mut recv: UnboundedReceiver<SendCommand>,
) -> std::io::Result<()> {
    write.writable().await?;
    let mut buf_writer = BufWriter::new(&mut write);
    while let Some(cmd) = recv.recv().await {
        match cmd {
            SendCommand::Send(msg) => buf_writer.write_all(&msg).await?,
            SendCommand::Flush => buf_writer.flush().await?,
        }
    }
    // flush and close
    buf_writer.flush().await?;
    buf_writer.shutdown().await?;
    Ok(())
}

/// Receive bytes ReadHalf of TcpStream and call `on_msg` callback.
async fn handle_reader<OnMsg: Fn(Msg<'_>) + Send + 'static>(
    read: OwnedReadHalf,
    on_msg: OnMsg,
) -> std::io::Result<()> {
    read.readable().await?;
    let mut buf_reader = BufReader::new(read);
    let mut read_buf = [0u8; BUFFER_SIZE];
    loop {
        let read_n = buf_reader.read(&mut read_buf).await;
        match read_n {
            Ok(n) => {
                if n == 0 {
                    return Ok(());
                }
                tracing::trace!("received {n} bytes", n = n);
                on_msg(Msg {
                    bytes: &read_buf[0..n],
                });
            }
            Err(e) => return Err(e),
        }
    }
}

/// On connection end, remove connection from connection state.
async fn join_reader_writer<OnConn: Fn(ConnState<OnMsg>), OnMsg>(
    (writer, reader): (
        JoinHandle<std::io::Result<()>>,
        JoinHandle<std::io::Result<()>>,
    ),
    (local_addr, peer_addr): (SocketAddr, SocketAddr),
    on_conn: OnConn,
) {
    let writer_abort = writer.abort_handle();
    let reader_abort = reader.abort_handle();
    let mut writer = writer.fuse();
    let mut reader = reader.fuse();
    loop {
        tokio::select! {
            w = &mut writer => {
                if let Err(e) = w {
                    tracing::error!("writer stopped local={local_addr}, peer={peer_addr} on error ({e})");
                } else {
                    tracing::debug!("writer stopped local={local_addr}, peer={peer_addr}");
                }
                reader_abort.abort();
                break;
            }
            r = &mut reader => {
                if let Err(e) = r {
                    tracing::error!("reader stopped local={local_addr}, peer={peer_addr} on error ({e})");
                } else {
                    tracing::debug!("reader stopped local={local_addr}, peer={peer_addr}");
                }
                writer_abort.abort();
                break;
            }
        }
    }
    tracing::info!("connection closed: local_addr={local_addr}, peer_addr={peer_addr}");
    on_conn(ConnState::OnConnectionClose {
        local_addr,
        peer_addr,
    });
}
