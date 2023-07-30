#include "socket_manager/msg_receiver.h"

namespace socket_manager {
  long MsgReceiver::on_message_async(std::string_view data, Waker &&waker) {
    on_message(data);
    Waker const drop_waker(std::move(waker));
    return (long) data.length();
  }
}
