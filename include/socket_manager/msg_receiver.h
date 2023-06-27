#ifndef SOCKET_MANAGER_MSG_RECEIVER_H
#define SOCKET_MANAGER_MSG_RECEIVER_H

#include <string>
#include <memory>
#include "socket_manager_c_api.h"

namespace socket_manager {

  /**
   * Implement this class to receive messages from Connection.
   */
  class MsgReceiver {

  public:

    /**
     * Called when a message is received.
     *
     * Should be non-blocking.
     *
     * @param data the message received.
     */
    virtual void on_message(std::shared_ptr<std::string> data) = 0;

    virtual ~MsgReceiver() = default;

  private:

    friend class Connection;

    static void on_msg(void *receiver_ptr,
                       ConnMsg msg) {
      auto receiver = reinterpret_cast<MsgReceiver *>(receiver_ptr);
      auto data_ptr = std::make_shared<std::string>(msg.Bytes, msg.Len);
      receiver->on_message(data_ptr);
    }

  };


} // namespace socket_manager

#endif //SOCKET_MANAGER_MSG_RECEIVER_H
