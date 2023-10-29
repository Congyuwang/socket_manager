#![feature(unboxed_closures)]
#![feature(fn_traits)]
#![feature(waker_getters)]
#![allow(improper_ctypes)]
#![feature(core_intrinsics)]

mod buf_read_write;
mod c_api;
mod conn;
mod conn_handle;
mod msg_sender;
mod read;
mod utils;
mod write;

use c_api::tracer::ForeignLogger;
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
use tracing_subscriber::filter::{Filtered, LevelFilter};
use tracing_subscriber::util::{SubscriberInitExt, TryInitError};
use tracing_subscriber::Layer;
use tracing_subscriber::{prelude::*, Registry};

pub use c_api::tracer::TraceLevel;
pub use conn::*;
pub use msg_sender::*;

const SHUTDOWN_TIMEOUT: Duration = Duration::from_secs(5);

/// The Main Struct of the Library.
///
/// This struct is thread safe.
pub struct SocketManager {
    cmd_send: UnboundedSender<Command>,

    // use has_joined to fence the join_handle,
    // both should only be accessed by the `join` method.
    has_joined: AtomicBool,
    join_handle: Option<std::thread::JoinHandle<()>>,
}

/// internal commands
enum Command {
    Listen { addr: SocketAddr },
    Connect { addr: SocketAddr, delay: Duration },
    CancelListen { addr: SocketAddr },
    Abort,
}

/// Connection state changes for the on_conn callback.
pub enum ConnState<OnMsg> {
    /// sent on connection success
    OnConnect { send: MsgSender, conn: Conn<OnMsg> },
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

/// Initialize socket manager logger.
/// Call this before anything else to prevent missing info.
/// This cannot be called twice, even accross multiple socket_manager instances.
pub fn init_logger(
    log_print_level: LevelFilter,
    foreign_logger: Filtered<ForeignLogger, LevelFilter, Registry>,
) -> Result<(), TryInitError> {
    let fmt_layer = {
        let fmt = tracing_subscriber::fmt::layer();
        fmt.with_filter(log_print_level)
    };
    tracing_subscriber::registry()
        .with(foreign_logger)
        .with(fmt_layer)
        .try_init()
}

/// Msg struct for the on_msg callback.
pub struct Msg<'a> {
    bytes: &'a [u8],
}

impl SocketManager {
    /// start background threads to run the runtime
    ///
    /// # Arguments
    /// - `on_conn`: the callback for connection state changes.
    /// - `n_threads`: number of threads to run the runtime:
    ///    - 0: use the default number of threads.
    ///    - 1: use the current thread.
    ///    - \>1: use the specified number of threads.
    pub fn init<
        OnConn: Fn(ConnState<OnMsg>) -> Result<(), String> + Send + 'static + Clone,
        OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + Unpin + 'static,
    >(
        on_conn: OnConn,
        n_threads: usize,
    ) -> std::io::Result<SocketManager> {
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
        Ok(SocketManager {
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

    pub fn connect_to_addr(&self, addr: SocketAddr, delay: Duration) -> std::io::Result<()> {
        self.cmd_send
            .send(Command::Connect { addr, delay })
            .map_err(|_| {
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

impl Drop for SocketManager {
    fn drop(&mut self) {
        let _ = self.abort(true);
    }
}

/// The main loop running in the background.
async fn main<
    OnConn: Fn(ConnState<OnMsg>) -> Result<(), String> + Send + 'static + Clone,
    OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + Unpin + 'static,
>(
    mut cmd_recv: UnboundedReceiver<Command>,
    handle: &Handle,
    on_conn: OnConn,
    connection_state: Arc<ConnectionState>,
) {
    while let Some(cmd) = cmd_recv.recv().await {
        match cmd {
            Command::Listen { addr } => listen_on_addr(handle, addr, &on_conn, &connection_state),
            Command::Connect { addr, delay } => {
                connect_to_addr(handle, addr, delay, &on_conn, &connection_state)
            }
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
///
/// The design of the function guarantees that either `OnConnect` or `OnConnectError`
/// will be called, but not both. And after `OnConnect` is called, `OnConnectionClose`
/// is guaranteed to be called.
///
/// The follow diagram shows the possible state transitions:
///
/// ```text
/// connect_command --> either --> OnConnect --> OnConnectionClose
///                          |
///                          --> OnConnectError
/// ```
fn connect_to_addr<
    OnConn: Fn(ConnState<OnMsg>) -> Result<(), String> + Send + 'static + Clone,
    OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + Unpin + 'static,
>(
    handle: &Handle,
    addr: SocketAddr,
    delay: Duration,
    on_conn: &OnConn,
    connection_state: &Arc<ConnectionState>,
) {
    let handle = handle.clone();
    let on_conn = on_conn.clone();
    let connection_state = connection_state.clone();

    // attempt to connect to address, and obtain connection info
    let connect_result = async move {
        if !delay.is_zero() {
            tokio::time::sleep(delay).await;
        }
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
            on_conn,
            &connection_state,
        )
        .await;
    });
}

async fn accept_connections<
    OnConn: Fn(ConnState<OnMsg>) -> Result<(), String> + Send + 'static + Clone,
    OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + Unpin + 'static,
>(
    addr: SocketAddr,
    listener: TcpListener,
    handle: &Handle,
    mut canceled: oneshot::Sender<()>,
    on_conn: OnConn,
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
