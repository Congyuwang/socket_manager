#include "socket_manager/msg_receiver.h"

namespace socket_manager {
  long MsgReceiver::on_message_async(std::string_view data, std::shared_ptr<RcvWaker> waker) {
    on_message(data);
    return (long) data.length();
  }
}
