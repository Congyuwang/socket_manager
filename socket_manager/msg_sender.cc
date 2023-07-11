#include "socket_manager/msg_sender.h"
#include <stdexcept>

namespace socket_manager {

  void MsgSender::send(const std::string &data) {
    char *err = nullptr;
    if (msg_sender_send(inner, data.data(), data.length(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  long MsgSender::try_send(const std::string &data, size_t offset, std::unique_ptr<Waker> waker) {
    // check length
    if (offset >= data.length()) {
      throw std::runtime_error("offset >= data.length()");
    }
    char *err = nullptr;
    long n = msg_sender_try_send(
            inner,
            data.data() + offset,
            data.length() - offset,
            MsgSenderObj{this},
            &err);
    if (err) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
    return n;
  }

  void MsgSender::waker() {}

  void MsgSender::flush() {
    char *err = nullptr;
    if (msg_sender_flush(inner, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  MsgSender::MsgSender(CMsgSender *inner) : waker_ref_count(0), inner(inner) {}

  MsgSender::~MsgSender() {
    // ensure that it is not freed when there is still waker
    if (waker_ref_count.load() == 0) {
      msg_sender_free(inner);
    }
  }

} // namespace socket_manager
