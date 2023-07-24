#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include "conn_callback.h"
#include "socket_manager_c_api.h"
#include <functional>
#include <string>
#include <memory>

namespace socket_manager {

  /**
   * Manages a set of sockets.
   *
   * Inherit from SocketManager and implement the virtual methods:
   * `on_connect`, `on_connection_close`, `on_listen_error`, `on_connect_error`,
   * `on_message` as callbacks. Note that these callbacks shouldn't block.
   *
   * Dropping this object will close all the connections and wait for all the
   * threads to finish.
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
     * # Thread Safety
     * Thread safe.
     *
     * # Errors
     * Throws `std::runtime_error` if socket manager runtime has been aborted.
     * Throws `std::runtime_error` if the address is invalid.
     *
     * @param addr: the ip address to listen to (support both ipv4 and ipv6).
     */
    void listen_on_addr(const std::string &addr);

    /**
     * Connect to the given address.
     *
     * # Thread Safety
     * Thread safe.
     *
     * # Errors
     * Throws `std::runtime_error` if socket manager runtime has been aborted.
     * Throws `std::runtime_error` if the address is invalid.
     *
     * @param addr: the ip address to listen to (support both ipv4 and ipv6).
     */
    void connect_to_addr(const std::string &addr);

    /**
     * Cancel listening on the given address.
     *
     * # Thread Safety
     * Thread safe.
     *
     * # Errors
     * Throws `std::runtime_error` if socket manager runtime has been aborted.
     * Throw `std::runtime_error` if the address is invalid.
     *
     * @param addr cancel listening on this address.
     */
    void cancel_listen_on_addr(const std::string &addr);

    /**
     * Stop all background threads and drop all connections.
     *
     * Calling a second time will return immediately (if `wait = false`).
     *
     * # Argument
     * - `wait`: if true, wait for all the background threads to finish.
     *     Default to true.
     *
     * # Thread Safety
     * Thread safe.
     *
     * # Errors
     * Throws `std::runtime_error` if `wait = true` and the background
     * thread panicked.
     */
    void abort(bool wait = true);

    /**
     * Join and wait on the `SocketManager` background runtime.
     *
     * Returns immediately on the second call.
     *
     * # Thread Safety
     * Thread safe.
     *
     * # Errors
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
