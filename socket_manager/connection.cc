#include "socket_manager/connection.h"
#include "socket_manager_c_api.h"

namespace socket_manager {

  std::shared_ptr<MsgSender> Connection::start(std::unique_ptr<MsgReceiver> msg_receiver) {

    // ensure that this function is called only once
    if(started.exchange(true, std::memory_order_seq_cst)) {
      throw std::runtime_error("Connection already started");
    }

    // keep the msg_receiver alive.
    receiver = std::move(msg_receiver);

    // start the connection.
    char *err = nullptr;
    CMsgSender *sender = connection_start(inner, OnMsgCallback{
            &receiver,
            MsgReceiver::on_msg
    }, &err);
    if (sender == nullptr) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }

    // return the sender
    return std::shared_ptr<MsgSender>(new MsgSender(sender));
  }

  Connection::Connection(CConnection *inner)
          : inner(inner), started(false) {}

  Connection::~Connection() {
    connection_free(inner);
  }

} // namespace socket_manager
