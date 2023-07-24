#include "socket_manager/connection.h"


namespace socket_manager {

  Connection::Connection(CConnection *inner)
          : inner(inner, [](CConnection *ptr) { connection_free(ptr); }) {}

  void Connection::start(
          std::shared_ptr<MsgReceiverAsync> msg_receiver,
          size_t msg_buffer_size,
          unsigned long long read_msg_flush_interval,
          unsigned long long write_flush_interval) {

    // start the connection.
    // calling twice `connection_start` will throw exception.
    char *err = nullptr;
    if (connection_start(inner.get(), OnMsgObj{
            msg_receiver.get(),
    }, msg_buffer_size, read_msg_flush_interval, write_flush_interval, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }

    // keep the msg_receiver alive.
    receiver = std::move(msg_receiver);
  }

  void Connection::close() {
    char *err = nullptr;
    if (connection_close(inner.get(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

} // namespace socket_manager
