#include "socket_manager.h"
#include <stdexcept>
#include <string>

namespace socket_manager {

  SocketManager::SocketManager() {
    char *err = nullptr;
    inner = socket_manager_init(this, on_conn, on_msg, &err);
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

  void SocketManager::cancel_connection(unsigned long long id) {
    char *err = nullptr;
    if (socket_manager_cancel_connection(inner, id, &err)) {
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

  void SocketManager::join() {
    char *err = nullptr;
    if (socket_manager_join(inner, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  void SocketManager::detach() {
    char *err = nullptr;
    if (socket_manager_detach(inner, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  SocketManager::~SocketManager() { socket_manager_free(inner); }

} // namespace socket_manager
