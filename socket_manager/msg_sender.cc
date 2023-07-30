#include "socket_manager/msg_sender.h"
#include <stdexcept>

namespace socket_manager {

void MsgSender::send_block(std::string_view data) {
  char *err = nullptr;
  if (0 != socket_manager_msg_sender_send_block(inner.get(), data.data(),
                                                data.length(), &err)) {
    const std::string err_str(err);
    free(err);
    throw std::runtime_error(err_str);
  }
}

void MsgSender::send_nonblock(std::string_view data) {
  char *err = nullptr;
  if (0 != socket_manager_msg_sender_send_nonblock(inner.get(), data.data(),
                                                   data.length(), &err)) {
    const std::string err_str(err);
    free(err);
    throw std::runtime_error(err_str);
  }
}

long MsgSender::send_async(std::string_view data) {
  char *err = nullptr;
  long const bytes_sent = socket_manager_msg_sender_send_async(
      inner.get(), data.data(), data.length(),
      SOCKET_MANAGER_C_API_Notifier{conn->notifier.get()}, &err);
  if (err != nullptr) {
    const std::string err_str(err);
    free(err);
    throw std::runtime_error(err_str);
  }
  return bytes_sent;
}

void MsgSender::flush() {
  char *err = nullptr;
  if (0 != socket_manager_msg_sender_flush(inner.get(), &err)) {
    const std::string err_str(err);
    free(err);
    throw std::runtime_error(err_str);
  }
}

MsgSender::MsgSender(SOCKET_MANAGER_C_API_MsgSender *inner,
                     const std::shared_ptr<Connection> &conn)
    : conn(conn), inner(inner, [](SOCKET_MANAGER_C_API_MsgSender *ptr) {
        socket_manager_msg_sender_free(ptr);
      }) {}

} // namespace socket_manager
