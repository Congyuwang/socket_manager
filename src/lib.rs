#![feature(unboxed_closures)]
#![feature(fn_traits)]
#![feature(waker_getters)]
#![allow(improper_ctypes)]

mod c_api;
mod conn;
mod conn_handle;
mod msg_sender;
mod read;
mod utils;
mod write;

use dashmap::DashMap;
use std::net::SocketAddr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::task::{Poll, Waker};
use std::time::Duration;
use tokio::net::{TcpListener, TcpStream};
use tokio::runtime::Handle;
use tokio::sync::mpsc::{UnboundedReceiver, UnboundedSender};
use tokio::sync::{mpsc, oneshot};
use tracing_subscriber::EnvFilter;

pub use conn::*;
pub use msg_sender::*;

const SOCKET_LOG: &str = "SOCKET_LOG";
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

/// internal commands
enum Command {
    Listen { addr: SocketAddr },
    Connect { addr: SocketAddr },
    CancelListen { addr: SocketAddr },
    Abort,
}

/// Connection state changes for the on_conn callback.
pub enum ConnState<OnMsg> {
    /// sent on connection success
    OnConnect {
        local_addr: SocketAddr,
        peer_addr: SocketAddr,
        send: CMsgSender,
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

/// Msg struct for the on_msg callback.
pub struct Msg<'a> {
    bytes: &'a [u8],
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
        OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + Sync + Unpin + 'static + Clone,
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
        let runtime = utils::start_runtime(n_threads)?;
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
    OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + Sync + Unpin + 'static + Clone,
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

/// This function connects to a port.
fn connect_to_addr<
    OnConn: Fn(ConnState<OnMsg>) -> Result<(), String> + Send + 'static + Clone,
    OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + Unpin + 'static,
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
        let (local, peer) = utils::get_address_from_stream(&stream)?;
        Ok::<(TcpStream, SocketAddr, SocketAddr), std::io::Error>((stream, local, peer))
    };

    // spawn connecting task
    handle.clone().spawn(async move {
        // trying to connect to address
        match connect_result.await {
            Ok((stream, local_addr, peer_addr)) => {
                conn_handle::handle_connection(
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
    OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + Sync + Unpin + 'static + Clone,
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
    OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + Unpin + 'static + Clone,
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
                match utils::get_address_from_stream(&stream) {
                    Err(e) => {
                        tracing::error!("failed to read address from stream: {e}");
                    }
                    Ok((local_addr, peer_addr)) => {
                        let connection_state = connection_state.clone();
                        conn_handle::handle_connection(local_addr, peer_addr, handle.clone(), stream, on_conn, connection_state);
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
