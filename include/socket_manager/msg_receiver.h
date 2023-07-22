#ifndef SOCKET_MANAGER_MSG_RECEIVER_H
#define SOCKET_MANAGER_MSG_RECEIVER_H

#include <string_view>
#include <stdexcept>
#include <memory>
#include <cstdlib>
#include <cstring>
#include "socket_manager_c_api.h"

namespace socket_manager {

  /**
   * Implement this class to receive messages from Connection.
   *
   * # Thread Safety
   * The callback should be thread safe.
   */
  class MsgReceiver {

  public:

    virtual ~MsgReceiver() = default;

  private:

    /**
     * Called when a message is received.
     *
     * # MEMORY SAFETY
     * The `data` is only valid during the call of this function.
     * If you want to keep the data, you should copy it.
     *
     * # Thread Safety
     * This callback must be thread safe.
     * It should also be non-blocking.
     *
     * # Error Handling
     * Throwing error in `on_message` callback will cause
     * the connection to close.
     *
     * @param data the message received.
     */
    virtual void on_message(std::string_view data) = 0;

    friend char* ::socket_manager_extern_on_msg(struct OnMsgObj this_, ConnMsg msg);

  };

} // namespace socket_manager

#endif //SOCKET_MANAGER_MSG_RECEIVER_H
