#ifndef SOCKET_MANAGER_MSG_RECEIVER_H
#define SOCKET_MANAGER_MSG_RECEIVER_H

#include <string>
#include <stdexcept>
#include <memory>
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

    /**
     * Called when a message is received.
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
    virtual void on_message(const std::shared_ptr<std::string> &data) = 0;

    virtual ~MsgReceiver() = default;

  private:

    friend class Connection;

    static char *on_msg(void *receiver_ptr,
                        ConnMsg msg) {
      auto receiver = reinterpret_cast<MsgReceiver *>(receiver_ptr);
      auto data_ptr = std::make_shared<std::string>(msg.Bytes, msg.Len);
      try {
        receiver->on_message(data_ptr);
      } catch (std::runtime_error &e) {
        return strdup(e.what());
      } catch (...) {
        return strdup("unknown error");
      }
      return nullptr;
    }

  };


} // namespace socket_manager

#endif //SOCKET_MANAGER_MSG_RECEIVER_H
