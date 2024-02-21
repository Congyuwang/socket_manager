#include "socket_manager/socket_manager.h"
#include "error.h"
#include "socket_manager_c_api.h"
#include <string_view>

namespace socket_manager {

LogData from_c_log_data(SOCKET_MANAGER_C_API_LogData log_data) {
  return {
      log_data.Level,
      std::string_view(log_data.Target, log_data.TargetN),
      std::string_view(log_data.File, log_data.FileN),
      log_data.Line,
      std::string_view(log_data.Message, log_data.MessageN),
  };
}

void init_logger(void (*tracer)(SOCKET_MANAGER_C_API_LogData),
                 SOCKET_MANAGER_C_API_TraceLevel tracer_max_level,
                 SOCKET_MANAGER_C_API_TraceLevel log_print_level) {
  char *err = nullptr;
  socket_manager_logger_init(tracer, tracer_max_level, log_print_level, &err);
  CHECK_RET(err, err);
}

SocketManager::SocketManager(const std::shared_ptr<ConnCallback> &conn_cb,
                             size_t n_threads)
    : conn_cb(conn_cb) {
  char *err = nullptr;
  auto *inner_ptr = socket_manager_init(
      SOCKET_MANAGER_C_API_OnConnObj{conn_cb.get()}, n_threads, &err);
  inner = std::unique_ptr<
      SOCKET_MANAGER_C_API_SocketManager,
      std::function<void(SOCKET_MANAGER_C_API_SocketManager *)>>(
      inner_ptr, socket_manager_free);
  CHECK_RET(err, err);
}

void SocketManager::listen_on_addr(const std::string &addr) {
  char *err = nullptr;
  int ret = socket_manager_listen_on_addr(inner.get(), addr.c_str(), &err);
  CHECK_RET(ret, err);
}

void SocketManager::connect_to_addr(const std::string &addr, uint64_t delay) {
  char *err = nullptr;
  int ret =
      socket_manager_connect_to_addr(inner.get(), addr.c_str(), delay, &err);
  CHECK_RET(ret, err);
}

void SocketManager::cancel_listen_on_addr(const std::string &addr) {
  char *err = nullptr;
  int ret =
      socket_manager_cancel_listen_on_addr(inner.get(), addr.c_str(), &err);
  CHECK_RET(ret, err);
}

void SocketManager::abort(bool wait) {
  char *err = nullptr;
  int ret = socket_manager_abort(inner.get(), wait, &err);
  CHECK_RET(ret, err);
}

void SocketManager::join() {
  char *err = nullptr;
  int ret = socket_manager_join(inner.get(), &err);
  CHECK_RET(ret, err);
}

} // namespace socket_manager
