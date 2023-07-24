#include "socket_manager/connection.h"


namespace socket_manager {

  Connection::Connection(SOCKET_MANAGER_C_API_Connection *inner)
          : notifier(std::make_shared<NoopNotifier>()),
            inner(inner,
                  [](SOCKET_MANAGER_C_API_Connection *ptr) {
                    socket_manager_connection_free(ptr);
                  }) {}

  void Connection::start(
          std::shared_ptr<MsgReceiverAsync> msg_receiver,
          std::shared_ptr<Notifier> send_notifier,
          size_t msg_buffer_size,
          unsigned long long read_msg_flush_interval,
          unsigned long long write_flush_interval) {

    if (msg_receiver == nullptr) {
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
    if (socket_manager_connection_start(inner.get(), SOCKET_MANAGER_C_API_OnMsgObj{
            this->receiver.get(),
    }, msg_buffer_size, read_msg_flush_interval, write_flush_interval, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  void Connection::close() {
    char *err = nullptr;
    if (socket_manager_connection_close(inner.get(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

} // namespace socket_manager
