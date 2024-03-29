#include "socket_manager/connection.h"
#include "error.h"
#include "socket_manager/common/notifier.h"
#include "socket_manager_c_api.h"

namespace socket_manager {

Connection::Connection(SOCKET_MANAGER_C_API_Connection *inner)
    : notifier(std::make_shared<NoopNotifier>()),
      inner(inner, socket_manager_connection_free) {}

void Connection::start(std::shared_ptr<MsgReceiverAsync> msg_receiver,
                       std::shared_ptr<Notifier> send_notifier,
                       size_t msg_buffer_size,
                       unsigned long long read_msg_flush_interval,
                       unsigned long long write_flush_interval) {

  if (msg_receiver == nullptr) [[unlikely]] {
    throw std::runtime_error("msg_receiver should not be nullptr");
  }
  // keep the msg_receiver alive.
  this->receiver = std::move(msg_receiver);
  // keep the notifier alive.
  if (send_notifier != nullptr) {
    this->notifier = std::move(send_notifier);
  }

  // start the connection.
  // calling twice `connection_start` will throw exception.
  char *err = nullptr;
  int ret = socket_manager_connection_start(
      inner.get(),
      SOCKET_MANAGER_C_API_OnMsgObj{
          this->receiver.get(),
      },
      msg_buffer_size, read_msg_flush_interval, write_flush_interval, &err);
  CHECK_RET(ret, err);
}

std::string Connection::peer_address() {
  char *addr = socket_manager_connection_peer_addr(inner.get());
  std::string peer(addr);
  free(addr);
  return peer;
}

std::string Connection::local_address() {
  char *addr = socket_manager_connection_local_addr(inner.get());
  std::string local(addr);
  free(addr);
  return local;
}

void Connection::close() {
  char *err = nullptr;
  int ret = socket_manager_connection_close(inner.get(), &err);
  CHECK_RET(ret, err);
}

} // namespace socket_manager
