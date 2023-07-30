#include "socket_manager/socket_manager.h"

namespace socket_manager {

  SocketManager::SocketManager(const std::shared_ptr<ConnCallback> &conn_cb, size_t n_threads)
          : conn_cb(conn_cb) {
    char *err = nullptr;
    auto *inner_ptr = socket_manager_init(SOCKET_MANAGER_C_API_OnConnObj{
            conn_cb.get()
    }, n_threads, &err);
    inner = std::unique_ptr<SOCKET_MANAGER_C_API_SocketManager,
            std::function<void(SOCKET_MANAGER_C_API_SocketManager *)>>(
            inner_ptr,
            [](SOCKET_MANAGER_C_API_SocketManager *ptr) { socket_manager_free(ptr); }
    );
    if (err != nullptr) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  void SocketManager::listen_on_addr(const std::string &addr) {
    char *err = nullptr;
    if (0 != socket_manager_listen_on_addr(inner.get(), addr.c_str(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  void SocketManager::connect_to_addr(const std::string &addr, uint64_t delay) {
    char *err = nullptr;
    if (0 != socket_manager_connect_to_addr(inner.get(), addr.c_str(), delay, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  void SocketManager::cancel_listen_on_addr(const std::string &addr) {
    char *err = nullptr;
    if (0 != socket_manager_cancel_listen_on_addr(inner.get(), addr.c_str(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  void SocketManager::abort(bool wait) {
    char *err = nullptr;
    if (0 != socket_manager_abort(inner.get(), wait, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  void SocketManager::join() {
    char *err = nullptr;
    if (0 != socket_manager_join(inner.get(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

} // namespace socket_manager
