#ifndef SOCKET_MANAGER_CONN_CALLBACK_H
#define SOCKET_MANAGER_CONN_CALLBACK_H

#include "connection.h"
#include "socket_manager_c_api.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace socket_manager {

/**
 * The callback object for handling connection events.
 * <br /><br />
 * Throwing error in the callback will cause the runtime
 * to abort.
 *
 * <h3>Thread Safety</h3>
 * All callback methods must be thread safe and non-blocking.
 */
class ConnCallback {
public:
  virtual ~ConnCallback() = default;

private:
  friend void ::socket_manager_extern_on_conn(
      struct SOCKET_MANAGER_C_API_OnConnObj this_,
      SOCKET_MANAGER_C_API_ConnStates states, char **err);

  /**
   * Called when a new connection is established.
   *
   * <h3>Error handling</h3>
   * Throwing error in `on_connect` callback will close the connection
   * and a `on_connection_close` callback will be evoked.
   * <br /><br />
   * It should be non-blocking.
   * <br /><br />
   * Drop the returned `MsgSender` to close the `Write` side of the
   * socket connection.
   *
   * @param conn a `Connection` object for starting the connection.
   * @param sender a `Sender` object for sending data.
   */
  virtual void on_connect(std::shared_ptr<Connection> conn,
                          std::unique_ptr<MsgSender> sender) = 0;

  /**
   * Called when a connection is closed.
   *
   * <h3>Error handling</h3>
   * Throwing error in `on_connection_close` callback is logged as error,
   * but ignored.
   * <br /><br />
   * It should be non-blocking.
   *
   * @param local_addr the local address of the connection.
   * @param peer_addr the peer address of the connection.
   */
  virtual void on_connection_close(const std::string &local_addr,
                                   const std::string &peer_addr) = 0;

  /**
   * Called when socket remote is closed.
   *
   * No more message will be received from this peer.
   *
   * <h3>Error handling</h3>
   * Throwing error in `on_connection_close` callback is logged as error,
   * but ignored.
   * <br /><br />
   * It should be non-blocking.
   *
   * @param local_addr the local address of the connection.
   * @param peer_addr the peer address of the connection.
   */
  virtual void on_remote_close(const std::string &local_addr,
                               const std::string &peer_addr) = 0;

  /**
   * Called when an error occurs when listening on the given address.
   *
   * <h3>Error handling</h3>
   * Throwing error in `on_listen_error` callback is logged as error,
   * but ignored.
   * <br /><br />
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
   * <h3>Error handling</h3>
   * Throwing error in `on_connect_error` callback is logged as error,
   * but ignored.
   * <br /><br />
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
} // namespace socket_manager

#endif // SOCKET_MANAGER_CONN_CALLBACK_H
