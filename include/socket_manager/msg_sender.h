#ifndef SOCKET_MANAGER_MSG_SENDER_H
#define SOCKET_MANAGER_MSG_SENDER_H

#include "socket_manager_c_api.h"
#include <string>

namespace socket_manager {

  /**
   * Use MsgSender to send messages to the peer.
   */
  class MsgSender {

  public:

    /**
     * Send a message to the peer.
     *
     * This method is thread safe.
     * This method does not implement backpressure
     * (i.e., it caches all the messages in memory).
     *
     * @param data the message to send
     */
    void send(const std::string &data);

    ~MsgSender();

  private:

    friend class Connection;

    explicit MsgSender(CMsgSender *inner);

    CMsgSender *inner;

  };

} // namespace socket_manager

#endif // SOCKET_MANAGER_MSG_SENDER_H
