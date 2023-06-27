#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include "connection.h"
#include <mutex>
#include "socket_manager_c_api.h"
#include <stdexcept>
#include <string>
#include <memory>

namespace socket_manager {

  /**
   * Manages a set of sockets.
   *
   * Inherit from SocketManager and implement the virtual methods:
   * `on_connect`, `on_connection_close`, `on_listen_error`, `on_connect_error`,
   * `on_message` as callbacks. Note that these callbacks shouldn't block.
   */
  class SocketManager {

  public:

    /**
     * Create a socket manager with n threads.
     *
     * @param n_threads the number of threads to use. If n_threads is 0, then
     *                 the number of threads is equal to the number of cores.
     *                 Default to single-threaded runtime.
     */
    explicit SocketManager(size_t n_threads = 1);

    /**
     * Listen on the given address.
     *
     * # Errors
     * Throws `std::runtime_error` if the address is invalid.
     *
     * @param addr: the ip address to listen to (support both ipv4 and ipv6).
     */
    void listen_on_addr(const std::string &addr);

    /**
     * Connect to the given address.
     *
     * # Errors
     * Throws `std::runtime_error` if the address is invalid.
     *
     * @param addr: the ip address to listen to (support both ipv4 and ipv6).
     */
    void connect_to_addr(const std::string &addr);

    /**
     * Cancel listening on the given address.
     *
     * # Errors
     * Throw `std::runtime_error` if the address is invalid.
     *
     * @param addr cancel listening on this address.
     */
    void cancel_listen_on_addr(const std::string &addr);

    /**
     * Join and wait on the `SocketManager` background runtime.
     *
     * # Errors
     * Throw `std::runtime_error` if failed to join.
     */
    void join();

    /**
     * Detach the `SocketManager`'s background runtime.
     */
    void detach();

    /// virtual methods: must be thread safe

    /**
     * Called when a new connection is established.
     *
     * It should be non-blocking.
     *
     * @param local_addr the local address of the connection.
     * @param peer_addr the peer address of the connection.
     * @param conn a `Connection` object for sending and receiving data.
     */
    virtual void on_connect(const std::string &local_addr,
                            const std::string &peer_addr,
                            std::shared_ptr<Connection> conn) = 0;

    /**
     * Called when a connection is closed.
     *
     * It should be non-blocking.
     *
     * @param local_addr the local address of the connection.
     * @param peer_addr the peer address of the connection.
     */
    virtual void on_connection_close(const std::string &local_addr,
                                     const std::string &peer_addr) = 0;

    /**
     * Called when an error occurs when listening on the given address.
     *
     * Should be non-blocking.
     *
     * @param addr the address that failed to listen on.
     * @param err the error message.
     */
    virtual void on_listen_error(const std::string &addr,
                                 const std::string &err) = 0;

    /**
     * Called when an error occurs when connecting to the given address.
     *
     * Should be non-blocking.
     *
     * @param addr the address that failed to connect to.
     * @param err the error message.
     */
    virtual void on_connect_error(const std::string &addr,
                                  const std::string &err) = 0;

    ~SocketManager();

  private:

    static void on_conn(void *manager_ptr, ConnStates states) {
      auto manager = static_cast<SocketManager *>(manager_ptr);
      switch (states.Code) {
        case ConnStateCode::Connect: {
          auto on_connect = states.Data.OnConnect;
          auto local_addr = std::string(on_connect.Local);
          auto peer_addr = std::string(on_connect.Peer);

          std::shared_ptr<Connection> conn(new Connection(on_connect.Conn));

          // keep the connection alive
          {
            std::unique_lock<std::mutex> lock(manager->lock);
            manager->conns[local_addr + peer_addr] = conn;
          }
          manager->on_connect(local_addr, peer_addr, conn);
          break;
        }
        case ConnStateCode::ConnectionClose: {
          auto on_connection_close = states.Data.OnConnectionClose;
          auto local_addr = std::string(on_connection_close.Local);
          auto peer_addr = std::string(on_connection_close.Peer);

          // remove the connection from the map
          {
            std::unique_lock<std::mutex> lock(manager->lock);
            manager->conns.erase(local_addr + peer_addr);
          }
          manager->on_connection_close(local_addr, peer_addr);
          break;
        }
        case ConnStateCode::ListenError: {
          auto listen_error = states.Data.OnListenError;
          auto addr = std::string(listen_error.Addr);
          auto err = std::string(listen_error.Err);
          manager->on_listen_error(addr, err);
          break;
        }
        case ConnStateCode::ConnectError: {
          auto connect_error = states.Data.OnConnectError;
          auto addr = std::string(connect_error.Addr);
          auto err = std::string(connect_error.Err);
          manager->on_connect_error(addr, err);
          break;
        }
      }
    }

    // keep the connection object alive before connection closed
    // to ensure that message listener is alive during connection.
    std::mutex lock;
    std::unordered_map<std::string, std::shared_ptr<Connection>> conns;

    CSocketManager *inner;

  };

} // namespace socket_manager

#endif // SOCKET_MANAGER_H
