#![feature(unboxed_closures)]
#![feature(fn_traits)]
mod c_api;

use dashmap::DashMap;
use futures::FutureExt;
use std::net::SocketAddr;
use std::num::NonZeroUsize;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use tokio::io::{AsyncReadExt, AsyncWriteExt, BufReader};
use tokio::net::tcp::{OwnedReadHalf, OwnedWriteHalf};
use tokio::net::{TcpListener, TcpStream};
use tokio::runtime;
use tokio::runtime::{Handle, Runtime};
use tokio::sync::mpsc::{UnboundedReceiver, UnboundedSender};
use tokio::sync::{mpsc, oneshot};
use tokio::task::JoinHandle;
use tracing_subscriber::EnvFilter;

const MAX_WORKER_THREADS: usize = 256;
const SOCKET_LOG: &str = "SOCKET_LOG";
const READ_BUFFER_SIZE: usize = 1024 * 64;

/// The Main Struct of the Library
pub struct CSocketManager {
    cmd_send: UnboundedSender<Command>,
    join_handle: Option<std::thread::JoinHandle<()>>,
}

/// Msg struct for the on_msg callback.
pub struct Msg<'a> {
    conn_id: u64,
    bytes: &'a [u8],
}

/// internal commands
enum Command {
    ListenOnAddr { addr: SocketAddr },
    ConnectToAddr { addr: SocketAddr },
    CancelListenOnAddr { addr: SocketAddr },
    CancelConnection { conn_id: u64 },
}

/// The connection struct for the on_conn callback.
pub struct Conn<OnMsg> {
    callback_setter: Option<oneshot::Sender<OnMsg>>,
    send: Option<UnboundedSender<Vec<u8>>>,
}

impl<OnMsg: Fn(Msg<'_>) + Send + 'static + Clone> Conn<OnMsg> {
    /// This function should be called only once.
    pub fn start_connection(&mut self, on_msg: OnMsg) -> std::io::Result<UnboundedSender<Vec<u8>>> {
        match (self.callback_setter.take(), self.send.take()) {
            (Some(setter), Some(sender)) => {
                if setter.send(on_msg).is_err() {
                    tracing::error!("callback setter failed");
                    return Err(std::io::Error::new(
                        std::io::ErrorKind::Other,
                        "callback setter failed on start connection",
                    ));
                }
                Ok(sender)
            }
            _ => Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "connection already started",
            )),
        }
    }
}

/// Connection state changes for the on_conn callback.
pub enum ConnState<OnMsg> {
    /// sent on connection success
    OnConnect {
        conn_id: u64,
        local_addr: SocketAddr,
        peer_addr: SocketAddr,
        conn: Conn<OnMsg>,
    },
    /// sent on connection closed
    OnConnectionClose {
        conn_id: u64,
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
    connections: DashMap<u64, oneshot::Sender<()>>,
    listeners: DashMap<SocketAddr, oneshot::Sender<()>>,
    current_conn_id: AtomicU64,
}

impl ConnectionState {
    fn new() -> Arc<Self> {
        Arc::new(ConnectionState {
            connections: DashMap::new(),
            listeners: DashMap::new(),
            current_conn_id: AtomicU64::new(0),
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
        tracing_subscriber::fmt()
            .with_env_filter(
                EnvFilter::builder()
                    .with_env_var(SOCKET_LOG)
                    .from_env_lossy(),
            )
            .init();
        let runtime = start_runtime(n_threads)?;
        let (cmd_send, cmd_recv) = mpsc::unbounded_channel::<Command>();
        let connection_state = ConnectionState::new();
        let join_handle = Some(std::thread::spawn(move || {
            let handle = runtime.handle();
            runtime.block_on(main(cmd_recv, handle, on_conn, connection_state))
        }));
        Ok(CSocketManager {
            cmd_send,
            join_handle,
        })
    }

    pub fn listen_on_addr(&self, addr: SocketAddr) -> std::io::Result<()> {
        self.cmd_send
            .send(Command::ListenOnAddr { addr })
            .map_err(|_| std::io::Error::new(std::io::ErrorKind::Other, "cmd send failed"))
    }

    pub fn connect_to_addr(&self, addr: SocketAddr) -> std::io::Result<()> {
        self.cmd_send
            .send(Command::ConnectToAddr { addr })
            .map_err(|_| std::io::Error::new(std::io::ErrorKind::Other, "cmd send failed"))
    }

    pub fn cancel_listen_on_addr(&self, addr: SocketAddr) -> std::io::Result<()> {
        self.cmd_send
            .send(Command::CancelListenOnAddr { addr })
            .map_err(|_| std::io::Error::new(std::io::ErrorKind::Other, "cmd send failed"))
    }

    pub fn cancel_connection(&self, conn_id: u64) -> std::io::Result<()> {
        self.cmd_send
            .send(Command::CancelConnection { conn_id })
            .map_err(|_| std::io::Error::new(std::io::ErrorKind::Other, "cmd send failed"))
    }

    pub fn detach(&mut self) {
        drop(self.join_handle.take())
    }

    pub fn join(&mut self) -> std::io::Result<()> {
        self.join_handle
            .take()
            .map(|h| {
                h.join().map_err(|e| {
                    tracing::error!("error joining socket manager: {:?}", e);
                    std::io::Error::new(
                        std::io::ErrorKind::Other,
                        format!("join handle failed on err {:?}", e),
                    )
                })
            })
            .unwrap_or(Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "cannot join twice",
            )))
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
            Command::ListenOnAddr { addr } => {
                listen_on_addr(handle, addr, on_conn.clone(), connection_state.clone())
            }
            Command::ConnectToAddr { addr } => {
                connect_to_addr(handle, addr, on_conn.clone(), connection_state.clone())
            }
            Command::CancelConnection { conn_id } => {
                if let Some((_, cancel)) = connection_state.connections.remove(&conn_id) {
                    let _ = cancel.send(());
                }
            }
            Command::CancelListenOnAddr { addr } => {
                if let Some((_, cancel)) = connection_state.listeners.remove(&addr) {
                    let _ = cancel.send(());
                }
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
    OnConn: Fn(ConnState<OnMsg>) + Send + 'static + Clone,
    OnMsg: Fn(Msg<'_>) + Send + 'static,
>(
    handle: &Handle,
    addr: SocketAddr,
    on_conn: OnConn,
    connection_state: Arc<ConnectionState>,
) {
    let handle = handle.clone();
    let handle_clone = handle.clone();
    let on_conn_clone = on_conn.clone();
    // spawn listening task
    let connect_result = handle.clone().spawn(async move {
        let stream = TcpStream::connect(addr).await?;
        handle_connection(&handle, stream, on_conn_clone, connection_state)?;
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
                tracing::error!("error listening on addr: {e}");
                on_conn(ConnState::OnListenError { addr, error: e });
                return;
            }
        };
        tracing::info!("listening on addr={addr}");
        let (cancel, mut cancel_recv) = oneshot::channel::<()>();
        connection_state.listeners.insert(addr, cancel);
        loop {
            let accept = listener.accept().fuse();
            tokio::select! {
                _ = &mut cancel_recv => {
                    tracing::info!("cancel listening on addr success (addr={addr})");
                    break;
                }
                accept_result = accept => {
                    match accept_result {
                        Ok((stream, _)) => {
                            if let Err(e) = handle_connection(&handle, stream, on_conn.clone(), connection_state.clone()) {
                                tracing::error!("error handling connection local={addr} ({e})");
                            }
                        }
                        Err(e) => {
                            tracing::error!("error accepting connection listening to {addr} ({e})");
                        }
                    }
                }
            }
        }
    });
}

/// This function handles connection from a client.
fn handle_connection<
    OnConn: Fn(ConnState<OnMsg>) + Send + 'static,
    OnMsg: Fn(Msg<'_>) + Send + 'static,
>(
    handle: &Handle,
    stream: TcpStream,
    on_conn: OnConn,
    connection_state: Arc<ConnectionState>,
) -> std::io::Result<()> {
    // generate connection info
    let local_addr = stream.local_addr()?;
    let peer_addr = stream.peer_addr()?;
    let (send, recv) = mpsc::unbounded_channel::<Vec<u8>>();
    let (callback_sender, callback) = oneshot::channel::<OnMsg>();
    let (cancel, cancel_recv) = oneshot::channel::<()>();

    let conn_id = connection_state
        .current_conn_id
        .fetch_add(1, Ordering::SeqCst);

    // update connection state
    connection_state.connections.insert(conn_id, cancel);

    tracing::info!(
        "new connection: conn_id={conn_id}, local_addr={local_addr}, peer_addr={peer_addr}"
    );
    // call `on_conn` callback
    on_conn(ConnState::OnConnect {
        conn_id,
        local_addr,
        peer_addr,
        conn: Conn {
            callback_setter: Some(callback_sender),
            send: Some(send),
        },
    });

    // spawn reader and writer
    let (read, write) = stream.into_split();
    let writer = handle.spawn(handle_writer(write, recv));
    let reader = handle.spawn(handle_reader(read, conn_id, callback));

    // join reader and writer
    handle.spawn(join_reader_writer(
        conn_id,
        cancel_recv,
        (writer, reader),
        (local_addr, peer_addr),
        on_conn,
        connection_state,
    ));
    Ok(())
}

/// Receive bytes from recv and write to WriteHalf of TcpStream.
async fn handle_writer(
    mut write: OwnedWriteHalf,
    mut recv: UnboundedReceiver<Vec<u8>>,
) -> std::io::Result<()> {
    write.writable().await?;
    while let Some(msg) = recv.recv().await {
        write.write_all(&msg).await?;
    }
    Ok(())
}

/// Receive bytes ReadHalf of TcpStream and call `on_msg` callback.
async fn handle_reader<OnMsg: Fn(Msg<'_>) + Send + 'static>(
    read: OwnedReadHalf,
    conn_id: u64,
    callback: oneshot::Receiver<OnMsg>,
) -> std::io::Result<()> {
    read.readable().await?;
    let on_msg = callback.await.map_err(|e| {
        std::io::Error::new(
            std::io::ErrorKind::Other,
            format!("error receiving callback for connection ({e})"),
        )
    })?;
    let mut buf_reader = BufReader::new(read);
    let mut read_buf = [0u8; READ_BUFFER_SIZE];
    loop {
        let read_n = buf_reader.read(&mut read_buf).await;
        match read_n {
            Ok(n) => {
                if n == 0 {
                    return Ok(());
                }
                on_msg(Msg {
                    conn_id,
                    bytes: &read_buf[0..n],
                });
            }
            Err(e) => return Err(e),
        }
    }
}

/// On connection end, remove connection from connection state.
async fn join_reader_writer<OnConn: Fn(ConnState<OnMsg>), OnMsg>(
    conn_id: u64,
    mut cancel_recv: oneshot::Receiver<()>,
    (writer, reader): (
        JoinHandle<std::io::Result<()>>,
        JoinHandle<std::io::Result<()>>,
    ),
    (local_addr, peer_addr): (SocketAddr, SocketAddr),
    on_conn: OnConn,
    connection_state: Arc<ConnectionState>,
) {
    let mut writer_stopped = false;
    let mut reader_stopped = false;
    let writer_abort = writer.abort_handle();
    let reader_abort = reader.abort_handle();
    let mut writer = writer.fuse();
    let mut reader = reader.fuse();
    loop {
        tokio::select! {
            _ = &mut cancel_recv => {
                writer_abort.abort();
                reader_abort.abort();
                tracing::debug!("connection aborted local={local_addr}, peer={peer_addr}");
                break;
            }
            w = &mut writer => {
                if let Err(e) = w {
                    tracing::error!("writer stopped local={local_addr}, peer={peer_addr} on error ({e})");
                } else {
                    tracing::debug!("writer stopped local={local_addr}, peer={peer_addr}");
                }
                writer_stopped = true;
                if reader_stopped {
                    break;
                }
            }
            r = &mut reader => {
                if let Err(e) = r {
                    tracing::error!("reader stopped local={local_addr}, peer={peer_addr} on error ({e})");
                } else {
                    tracing::debug!("reader stopped local={local_addr}, peer={peer_addr}");
                }
                reader_stopped = true;
                if writer_stopped {
                    break;
                }
            }
        }
    }
    tracing::info!(
        "connection closed: conn_id={conn_id}, local_addr={local_addr}, peer_addr={peer_addr}"
    );
    on_conn(ConnState::OnConnectionClose { conn_id });
    connection_state.connections.remove(&conn_id);
}
