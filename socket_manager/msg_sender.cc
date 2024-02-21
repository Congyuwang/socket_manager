#include "socket_manager/msg_sender.h"
#include "error.h"

namespace socket_manager {

void MsgSender::send_block(std::string_view data) {
  char *err = nullptr;
  int ret = socket_manager_msg_sender_send_block(inner.get(), data.data(),
                                                 data.length(), &err);
  CHECK_RET(ret, err);
}

void MsgSender::send_nonblock(std::string_view data) {
  char *err = nullptr;
  int ret = socket_manager_msg_sender_send_nonblock(inner.get(), data.data(),
                                                    data.length(), &err);
  CHECK_RET(ret, err);
}

long MsgSender::send_async(std::string_view data) {
  char *err = nullptr;
  const long bytes_sent = socket_manager_msg_sender_send_async(
      inner.get(), data.data(), data.length(),
      SOCKET_MANAGER_C_API_Notifier{conn->notifier.get()}, &err);
  CHECK_RET(err, err);
  return bytes_sent;
}

void MsgSender::flush() {
  char *err = nullptr;
  int ret = socket_manager_msg_sender_flush(inner.get(), &err);
  CHECK_RET(ret, err);
}

MsgSender::MsgSender(SOCKET_MANAGER_C_API_MsgSender *inner,
                     const std::shared_ptr<Connection> &conn)
    : conn(conn), inner(inner, socket_manager_msg_sender_free) {}

} // namespace socket_manager
