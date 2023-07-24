#ifndef SOCKET_MANAGER_MSG_RECEIVER_H
#define SOCKET_MANAGER_MSG_RECEIVER_H

#include <string_view>
#include <stdexcept>
#include <memory>
#include <cstdlib>
#include <cstring>
#include "socket_manager_c_api.h"
#include "recv_waker.h"

namespace socket_manager {

  /**
   * Implement this class to receive messages from Connection.
   *
   * # Thread Safety
   * The callback should be thread safe.
   */
  class MsgReceiverAsync {

  public:

    virtual ~MsgReceiverAsync() = default;

  private:

    /**
     * Called when a message is received.
     *
     * # Async Return Rule
     * - negative number: pending.
     * - positive number: n bytes received, ready.
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
    virtual long on_message_async(std::string_view data, std::shared_ptr<RcvWaker> waker) = 0;

    friend long::socket_manager_extern_on_msg(struct OnMsgObj this_, ConnMsg msg, CWaker *waker, char **err);

  };

  class MsgReceiver : public MsgReceiverAsync {
  public:

    virtual ~MsgReceiver() = default;

  private:

    virtual void on_message(std::string_view data) = 0;

    long on_message_async(std::string_view data, std::shared_ptr<RcvWaker> waker) override;
  };

} // namespace socket_manager

#endif //SOCKET_MANAGER_MSG_RECEIVER_H
