#ifndef SOCKET_MANAGER_CONN_CALLBACK_H
#define SOCKET_MANAGER_CONN_CALLBACK_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include "connection.h"

namespace socket_manager {

  /**
   * The callback object for handling connection events.
   *
   * All methods must be thread safe.
   *
   * # Note on safety:
   *
   * - The `connection callback` object should have
   *   a longer lifetime than the socket manager.
   *
   * - The `msg receiver` callbacks should all have
   *   longer lifetimes than the `connection callback`.
   *
   * - The design stores a shared pointer to the
   *   `ConnCallback` in `SocketManager`, and shared
   *   pointers of `Connection`s in the `ConnCallback`
   *   objects, and store unique pointers of `MsgReceiver`
   *   in `Connection`.
   *
   *   Thus establish a dependency relationship as follows:
   *   `SocketManager` -> shared `ConnCallback` -> shared `Connection`s
   *   -> unique `MsgReceiver`, where the later object has a longer
   *   lifetime than the former.
   */
  class ConnCallback {
  public:

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
                            const std::shared_ptr<Connection> &conn) = 0;

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

    virtual ~ConnCallback() = default;

  private:

    friend class SocketManager;

    static void on_conn(void *conn_cb_ptr, ConnStates states) {
      auto conn_cb = static_cast<ConnCallback *>(conn_cb_ptr);
      switch (states.Code) {
        case ConnStateCode::Connect: {
          auto on_connect = states.Data.OnConnect;
          auto local_addr = std::string(on_connect.Local);
          auto peer_addr = std::string(on_connect.Peer);

          std::shared_ptr<Connection> conn(new Connection(on_connect.Conn));

          // keep the connection alive
          {
            std::unique_lock<std::mutex> lock(conn_cb->lock);
            conn_cb->conns[local_addr + peer_addr] = conn;
          }
          conn_cb->on_connect(local_addr, peer_addr, conn);
          break;
        }
        case ConnStateCode::ConnectionClose: {
          auto on_connection_close = states.Data.OnConnectionClose;
          auto local_addr = std::string(on_connection_close.Local);
          auto peer_addr = std::string(on_connection_close.Peer);

          // remove the connection from the map
          {
            std::unique_lock<std::mutex> lock(conn_cb->lock);
            conn_cb->conns.erase(local_addr + peer_addr);
          }
          conn_cb->on_connection_close(local_addr, peer_addr);
          break;
        }
        case ConnStateCode::ListenError: {
          auto listen_error = states.Data.OnListenError;
          auto addr = std::string(listen_error.Addr);
          auto err = std::string(listen_error.Err);
          conn_cb->on_listen_error(addr, err);
          break;
        }
        case ConnStateCode::ConnectError: {
          auto connect_error = states.Data.OnConnectError;
          auto addr = std::string(connect_error.Addr);
          auto err = std::string(connect_error.Err);
          conn_cb->on_connect_error(addr, err);
          break;
        }
      }
    }

    // keep the connection object alive before connection closed
    // to ensure that message listener is alive during connection.
    std::mutex lock;
    std::unordered_map<std::string, std::shared_ptr<Connection>> conns;

  };
}

#endif //SOCKET_MANAGER_CONN_CALLBACK_H