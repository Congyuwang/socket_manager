use std::net::SocketAddr;
use std::num::NonZeroUsize;
use tokio::net::TcpStream;

pub const MAX_WORKER_THREADS: usize = 256;

/// Start runtime
pub(crate) fn start_runtime(n_threads: usize) -> std::io::Result<tokio::runtime::Runtime> {
    let n_thread = n_threads.min(MAX_WORKER_THREADS);
    match n_thread {
        0 => {
            let n_cpu =
                std::thread::available_parallelism().unwrap_or(NonZeroUsize::new(1).unwrap());
            tracing::info!("socket manager started runtime with {n_cpu} threads");
            tokio::runtime::Builder::new_multi_thread()
                .enable_all()
                .worker_threads(n_cpu.get())
                .build()
        }
        1 => {
            tracing::info!("socket manager started runtime with single thread");
            tokio::runtime::Builder::new_current_thread()
                .enable_all()
                .build()
        }
        n => {
            tracing::info!("socket manager started runtime with {n} threads");
            tokio::runtime::Builder::new_multi_thread()
                .enable_all()
                .worker_threads(n)
                .build()
        }
    }
}

pub(crate) fn get_address_from_stream(
    stream: &TcpStream,
) -> std::io::Result<(SocketAddr, SocketAddr)> {
    let local = stream.local_addr()?;
    let peer = stream.peer_addr()?;
    Ok((local, peer))
}
