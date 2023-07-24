#include "socket_manager/msg_sender.h"
#include <stdexcept>

namespace socket_manager {

  void MsgSender::send_block(std::string_view data) {
    char *err = nullptr;
    if (socket_manager_msg_sender_send_block(
            inner.get(), data.data(), data.length(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  long MsgSender::send_async(std::string_view data, const std::shared_ptr<Notifier> &notifier) {
    char *err = nullptr;
    if (notifier == nullptr) {
      // does not allow nullptr
      throw std::runtime_error("waker is nullptr");
    }
    long n = socket_manager_msg_sender_send_async(
            inner.get(),
            data.data(),
            data.length(),
            SOCKET_MANAGER_C_API_Notifier{notifier.get()},
            &err);
    if (err) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
    // keep waker alive
    conn->notifier = notifier;
    return n;
  }

  void MsgSender::flush() {
    char *err = nullptr;
    if (socket_manager_msg_sender_flush(inner.get(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  MsgSender::MsgSender(SOCKET_MANAGER_C_API_MsgSender *inner, const std::shared_ptr<Connection> &conn)
          : conn(conn),
            inner(inner,
                  [](SOCKET_MANAGER_C_API_MsgSender *ptr) {
                    socket_manager_msg_sender_free(ptr);
                  }) {}

} // namespace socket_manager
