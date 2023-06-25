#ifndef MSG_SENDER_H
#define MSG_SENDER_H

#include "socket_manager_c_api.h"
#include <string>

namespace socket_manager {

  class MsgSender {

  public:

    void send(const std::string &data);

    ~MsgSender();

  private:

    friend class SocketManager;

    explicit MsgSender(CMsgSender *inner);

    CMsgSender *inner;

  };

} // namespace socket_manager

#endif // MSG_SENDER_H
