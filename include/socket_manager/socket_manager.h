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
  template<class CB, class Rcv> class SocketManager {

  public:

    /**
     * Create a socket manager with n threads.
     *
     * @param n_threads the number of threads to use. If n_threads is 0, then
     *                 the number of threads is equal to the number of cores.
     *                 Default to single-threaded runtime.
     */
    explicit SocketManager(const std::shared_ptr<CB> &conn_cb, size_t n_threads = 1) : conn_cb(conn_cb) {
      static_assert(
              std::is_base_of<ConnCallback<Rcv>, CB>::value,
              "conn_cb should be derived from `ConnCallback`");

      static_assert(
              std::is_base_of<MsgReceiver, Rcv>::value,
              "Rcv should be derived from MsgReceiver");

      char *err = nullptr;
      inner = socket_manager_init(OnConnCallback{
              conn_cb.get(),
              ConnCallback<Rcv>::on_conn
      }, n_threads, &err);
      if (err) {
        const std::string err_str(err);
        free(err);
        throw std::runtime_error(err_str);
      }
    }

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
    void listen_on_addr(const std::string &addr) {
      char *err = nullptr;
      if (socket_manager_listen_on_addr(inner, addr.c_str(), &err)) {
        const std::string err_str(err);
        free(err);
        throw std::runtime_error(err_str);
      }
    }

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
    void connect_to_addr(const std::string &addr) {
      char *err = nullptr;
      if (socket_manager_connect_to_addr(inner, addr.c_str(), &err)) {
        const std::string err_str(err);
        free(err);
        throw std::runtime_error(err_str);
      }
    }

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
    void cancel_listen_on_addr(const std::string &addr) {
      char *err = nullptr;
      if (socket_manager_cancel_listen_on_addr(inner, addr.c_str(), &err)) {
        const std::string err_str(err);
        free(err);
        throw std::runtime_error(err_str);
      }
    }

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
     * # Errors
     * Throws `std::runtime_error` if socket manager runtime has been aborted.
     */
    void abort(bool wait = true) {
      char *err = nullptr;
      if (socket_manager_abort(inner, wait, &err)) {
        const std::string err_str(err);
        free(err);
        throw std::runtime_error(err_str);
      }
    }

    /**
     * Join and wait on the `SocketManager` background runtime.
     * Call `abort` in another thread to stop the background runtime.
     *
     * # Thread Safety
     * Thread safe.
     *
     * # Errors
     * Throws `std::runtime_error` if the background runtime panicked.
     * Returns immediately on the second call.
     */
    void join() {
      char *err = nullptr;
      if (socket_manager_join(inner, &err)) {
        const std::string err_str(err);
        free(err);
        throw std::runtime_error(err_str);
      }
    }

    ~SocketManager() {
      socket_manager_free(inner);
    }

    SocketManager(const SocketManager &) = delete;

    void operator=(const SocketManager &) = delete;

  private:

    CSocketManager *inner;
    std::shared_ptr<CB> conn_cb;

  };

} // namespace socket_manager

#endif // SOCKET_MANAGER_H
