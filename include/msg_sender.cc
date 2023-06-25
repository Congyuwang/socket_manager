#include "msg_sender.h"

namespace socket_manager {

  void MsgSender::send(const std::string &data) {
    char *err = nullptr;
    if (msg_sender_send(inner, data.data(), data.length(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  MsgSender::MsgSender(CMsgSender *inner) : inner(inner) {}

  MsgSender::~MsgSender() {
    msg_sender_free(inner);
  }

} // namespace socket_manager
