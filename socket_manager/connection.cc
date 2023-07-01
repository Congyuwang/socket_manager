#include "socket_manager/connection.h"
#include "socket_manager_c_api.h"
#include <stdexcept>

namespace socket_manager {

  std::shared_ptr<MsgSender> Connection::start(
          std::unique_ptr<MsgReceiver> msg_receiver,
          unsigned long long write_flush_interval) {

    // start the connection.
    // calling twice `connection_start` will throw exception.
    char *err = nullptr;
    CMsgSender *sender = connection_start(inner, OnMsgCallback{
            msg_receiver.get(),
            MsgReceiver::on_msg
    }, write_flush_interval, &err);
    if (sender == nullptr) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }

    // keep the msg_receiver alive.
    receiver = std::move(msg_receiver);

    // return the sender
    return std::shared_ptr<MsgSender>(new MsgSender(sender));
  }

  void Connection::close() {
    char *err = nullptr;
    if (connection_close(inner, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  Connection::Connection(CConnection *inner) : inner(inner) {}

  Connection::~Connection() {
    connection_free(inner);
  }

} // namespace socket_manager
