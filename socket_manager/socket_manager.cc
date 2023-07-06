#include "socket_manager/socket_manager.h"
#include <stdexcept>
#include <string>

namespace socket_manager {

  template<class CB, class Rcv>
  SocketManager<CB, Rcv>::SocketManager(
          const std::shared_ptr<CB> &conn_cb,
          size_t n_threads
  ) : conn_cb(conn_cb) {

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

  template<class CB, class Rcv>
  void SocketManager<CB, Rcv>::listen_on_addr(const std::string &addr) {
    char *err = nullptr;
    if (socket_manager_listen_on_addr(inner, addr.c_str(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  template<class CB, class Rcv>
  void SocketManager<CB, Rcv>::connect_to_addr(const std::string &addr) {
    char *err = nullptr;
    if (socket_manager_connect_to_addr(inner, addr.c_str(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  template<class CB, class Rcv>
  void SocketManager<CB, Rcv>::cancel_listen_on_addr(const std::string &addr) {
    char *err = nullptr;
    if (socket_manager_cancel_listen_on_addr(inner, addr.c_str(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  template<class CB, class Rcv>
  void SocketManager<CB, Rcv>::join() {
    char *err = nullptr;
    if (socket_manager_join(inner, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  template<class CB, class Rcv>
  void SocketManager<CB, Rcv>::abort(bool wait) {
    char *err = nullptr;
    if (socket_manager_abort(inner, wait, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  template<class CB, class Rcv>
  SocketManager<CB, Rcv>::~SocketManager() {
    socket_manager_free(inner);
  }

} // namespace socket_manager
