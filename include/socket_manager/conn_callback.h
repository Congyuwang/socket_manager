#ifndef SOCKET_MANAGER_CONN_CALLBACK_H
#define SOCKET_MANAGER_CONN_CALLBACK_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include "connection.h"
#include "socket_manager_c_api.h"

namespace socket_manager {

  /**
   * The callback object for handling connection events.
   *
   * # Error Handling
   * Throwing error in the callback will cause the runtime
   * to abort.
   *
   * # Thread Safety
   * All callback methods must be thread safe and non-blocking.
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

    virtual ~ConnCallback() = default;

  private:

    template<class> friend
    class SocketManager;

    friend char* ::socket_manager_extern_on_conn(void *this_, ConnStates conn);

    /**
     * Called when a new connection is established.
     *
     * # Error handling
     * Throwing error in `on_connect` callback will close the connection
     * and a `on_connection_close` callback will be evoked.
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
     * # Error handling
     * Throwing error in `on_connection_close` callback is logged as error,
     * but ignored.
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
     * # Error handling
     * Throwing error in `on_listen_error` callback is logged as error,
     * but ignored.
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
     * # Error handling
     * Throwing error in `on_connect_error` callback is logged as error,
     * but ignored.
     *
     * Should be non-blocking.
     *
     * @param addr the address that failed to connect to.
     * @param err the error message.
     */
    virtual void on_connect_error(const std::string &addr,
                                  const std::string &err) = 0;

    // keep the connection object alive before connection closed
    // to ensure that message listener is alive during connection.
    std::mutex lock;
    std::unordered_map<std::string, std::shared_ptr<Connection>> conns;

  };
}

#endif //SOCKET_MANAGER_CONN_CALLBACK_H
