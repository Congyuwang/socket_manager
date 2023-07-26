use crate::conn::{Conn, ConnConfig};
use crate::msg_sender::make_sender;
use crate::{read, write, ConnState, ConnectionState, Msg};
use futures::FutureExt;
use std::net::SocketAddr;
use std::sync::Arc;
use std::task::{Poll, Waker};
use tokio::net::TcpStream;
use tokio::runtime::Handle;
use tokio::sync::oneshot;
use tokio::task::JoinHandle;

/// This function handles connection from a client.
pub(crate) fn handle_connection<
    OnConn: Fn(ConnState<OnMsg>) -> Result<(), String> + Send + 'static + Clone,
    OnMsg: Fn(Msg<'_>, Waker) -> Poll<Result<usize, String>> + Send + Unpin + 'static,
>(
    local_addr: SocketAddr,
    peer_addr: SocketAddr,
    handle: Handle,
    stream: TcpStream,
    on_conn: OnConn,
    connection_state: Arc<ConnectionState>,
) {
    let (conn_config_setter, conn_config) = oneshot::channel::<(OnMsg, ConnConfig)>();
    let (send, recv) = make_sender(handle.clone());

    let on_connect_on_conn = on_conn.clone();
    // Call `on_conn` callback, and wait for user to call `start` on connection.
    // Return the OnMsg callback and conn_config.
    let wait_for_start = async move {
        on_connect_on_conn(ConnState::OnConnect {
            local_addr,
            peer_addr,
            send,
            conn: Conn::new(conn_config_setter),
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
            let writer = handle.spawn(write::handle_writer(write, recv, config, stop));
            let reader = handle.spawn(read::handle_reader(read, on_msg, config));

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
        let _ = on_conn(ConnState::OnConnectionClose {
            local_addr,
            peer_addr,
        });
    });
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
