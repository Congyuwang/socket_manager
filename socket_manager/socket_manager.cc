#include "socket_manager/socket_manager.h"

namespace socket_manager {

  SocketManager::SocketManager(const std::shared_ptr<ConnCallback> &conn_cb, size_t n_threads) : conn_cb(
          conn_cb) {
    char *err = nullptr;
    inner = socket_manager_init(OnConnObj{
            conn_cb.get()
    }, n_threads, &err);
    if (err) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  void SocketManager::listen_on_addr(const std::string &addr) {
    char *err = nullptr;
    if (socket_manager_listen_on_addr(inner, addr.c_str(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  void SocketManager::connect_to_addr(const std::string &addr) {
    char *err = nullptr;
    if (socket_manager_connect_to_addr(inner, addr.c_str(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  void SocketManager::cancel_listen_on_addr(const std::string &addr) {
    char *err = nullptr;
    if (socket_manager_cancel_listen_on_addr(inner, addr.c_str(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  void SocketManager::abort(bool wait) {
    char *err = nullptr;
    if (socket_manager_abort(inner, wait, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  void SocketManager::join() {
    char *err = nullptr;
    if (socket_manager_join(inner, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  SocketManager::~SocketManager() {
    socket_manager_free(inner);
  }

} // namespace socket_manager
