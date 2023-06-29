#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include "conn_callback.h"
#include "socket_manager_c_api.h"
#include <string>
#include <memory>
#include <mutex>

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
     * Throw `std::runtime_error` if the address is invalid.
     *
     * @param addr cancel listening on this address.
     */
    void cancel_listen_on_addr(const std::string &addr);

    /**
     * Join and wait on the `SocketManager` background runtime.
     * Call `abort` in another thread to stop the background runtime.
     *
     * # Thread Safety
     * Thread safe.
     *
     * Returns immediately on the second call.
     *
     * Returns error if the background runtime has already been joined
     * or if the runtime panicked.
     */
    void join();

    /**
     * Stop all background threads and drop all connections.
     *
     * # Argument
     * - `wait`: if true, wait for all the background threads to finish.
     *     Default to true.
     *
     * # Thread Safety
     * Thread safe.
     *
     * This method does not wait for the background threads to finish.
     *
     * Call methods after successful `aborted` will result in runtime errors.
     *
     */
    void abort(bool wait = true);

    ~SocketManager();

  private:

    CSocketManager *inner;
    std::shared_ptr<ConnCallback> conn_cb;

  };

} // namespace socket_manager

#endif // SOCKET_MANAGER_H
