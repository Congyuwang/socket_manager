#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include "msg_sender.h"
#include "conn_callback.h"
#include "socket_manager_c_api.h"
#include <functional>
#include <string>
#include <memory>

namespace socket_manager {

  /**
   * @brief Manages a set of sockets.
   *
   * <h3>Usage</h3>
   * The user needs to implement a `ConnCallback` object to handle
   * connection events, and pass it to the constructor of `SocketManager`.
   * <br /><br />
   * When the connection is established, the `on_connect` callback
   * returns a `MsgSender` object for sending messages to the peer,
   * and a `Connection` object for receiving messages from the peer.
   * <br /><br />
   * To receive messages from the peer, the user needs to implement
   * a `MsgReceiver` object and pass it to the `Connection::start`
   * method.
   *
   * <h3>Memory Management</h3>
   * The system internally use shared pointers to manage the lifetime
   * of the objects. Note the following dependency relationship to avoid
   * memory leak (i.e., circular reference):
   * <ul>
   * <li> `SocketManager` ---strong ref--> `ConnCallback` </li>
   * <li> `ConnCallback` ---strong ref--> (active) `Connection`s (drop on `connection_close`) </li>
   * <li> `Connection` ---strong ref--> `Notifier` </li>
   * <li> `Connection` ---strong ref--> `(Async)MsgReceiver` </li>
   * <li> `MsgSender` ---strong ref--> `Connection` </li>
   * </ul>
   * Notice that if `MsgSender` is strongly referenced by `Notifier`,
   * or strongly referenced by `(Async)MsgReceiver`, then the connection
   * will have a memory leak. The user could `reset()` the `shared_ptr\<MsgSender\>`
   * on connection close event, and thus break the cycle.
   * <br /><br />
   * In short, the user must guarantee that the `MsgSender` object
   * is released for connection resources to be properly released.
   *
   * <h3>Note on lifetime:</h3>
   *
   * <ul>
   * <li> The `connection callback` object should have
   *   a longer lifetime than the socket manager. </li>
   *
   * <li> The `msg receiver` should live as long as
   *   connection is not closed. </li>
   *
   * <li> The `Notifier` object should live as long as
   *   connection is not closed. </li>
   * </ul>
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
    explicit SocketManager(const std::shared_ptr<ConnCallback> &conn_cb, size_t n_threads = 1);

    /**
     * Listen on the given address.
     *
     * <h3>Thread Safety</h3>
     * Thread safe.
     *
     * <h3>Errors</h3>
     * Throws `std::runtime_error` if socket manager runtime has been aborted.
     * Throws `std::runtime_error` if the address is invalid.
     *
     * @param addr: the ip address to listen to (support both ipv4 and ipv6).
     */
    void listen_on_addr(const std::string &addr);

    /**
     * Connect to the given address.
     *
     * <h3>Thread Safety</h3>
     * Thread safe.
     *
     * <h3>Errors</h3>
     * Throws `std::runtime_error` if socket manager runtime has been aborted.
     * Throws `std::runtime_error` if the address is invalid.
     *
     * @param addr: the ip address to listen to (support both ipv4 and ipv6).
     * @param delay: the delay in milliseconds before connecting to the address.
     */
    void connect_to_addr(const std::string &addr, uint64_t delay = 0);

    /**
     * Cancel listening on the given address.
     *
     * <h3>Thread Safety</h3>
     * Thread safe.
     *
     * <h3>Errors</h3>
     * Throws `std::runtime_error` if socket manager runtime has been aborted.
     * Throw `std::runtime_error` if the address is invalid.
     *
     * @param addr cancel listening on this address.
     */
    void cancel_listen_on_addr(const std::string &addr);

    /**
     * Stop all background threads and drop all connections.
     * <br /><br />
     * Calling a second time will return immediately (if `wait = false`).
     *
     * <h3>Argument</h3>
     * - `wait`: if true, wait for all the background threads to finish.
     *     Default to true.
     *
     * <h3>Thread Safety</h3>
     * Thread safe.
     *
     * <h3>Errors</h3>
     * Throws `std::runtime_error` if `wait = true` and the background
     * thread panicked.
     */
    void abort(bool wait = true);

    /**
     * Join and wait on the `SocketManager` background runtime.
     * <br /><br />
     * Returns immediately on the second call.
     *
     * <h3>Thread Safety</h3>
     * Thread safe.
     *
     * <h3>Errors</h3>
     * Throws `std::runtime_error` if the background runtime panicked.
     */
    void join();

  private:

    std::unique_ptr<SOCKET_MANAGER_C_API_SocketManager, std::function<void(
            SOCKET_MANAGER_C_API_SocketManager *)>> inner;
    std::shared_ptr<ConnCallback> conn_cb;

  };

} // namespace socket_manager

#endif // SOCKET_MANAGER_H
