#include "socket_manager/msg_sender.h"
#include <stdexcept>

namespace socket_manager {

  void MsgSender::send(std::string_view data) {
    char *err = nullptr;
    if (msg_sender_send(inner, data.data(), data.length(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  long MsgSender::try_send(std::string_view data, size_t offset, const std::shared_ptr<Waker> &waker) {
    // check length
    if (offset >= data.length()) {
      throw std::runtime_error("offset >= data.length()");
    }
    char *err = nullptr;
    long n = msg_sender_try_send(
            inner,
            data.data() + offset,
            data.length() - offset,
            WakerObj{waker.get()},
            &err);
    if (err) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
    // keep waker alive
    conn->waker = waker;
    return n;
  }

  void MsgSender::flush() {
    char *err = nullptr;
    if (msg_sender_flush(inner, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  MsgSender::MsgSender(CMsgSender *inner, const std::shared_ptr<Connection> &conn)
          : conn(conn), inner(inner) {}

  MsgSender::~MsgSender() {
    msg_sender_free(inner);
  }

} // namespace socket_manager
