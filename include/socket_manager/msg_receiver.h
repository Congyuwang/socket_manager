#ifndef SOCKET_MANAGER_MSG_RECEIVER_H
#define SOCKET_MANAGER_MSG_RECEIVER_H

#include <string_view>
#include <stdexcept>
#include <memory>
#include <cstdlib>
#include <cstring>
#include "socket_manager_c_api.h"
#include "socket_manager/common/waker.h"

namespace socket_manager {

  /**
   * Implement this class to receive messages from Connection.
   *
   * Must read the following details to implement correctly!
   *
   * # Asynchronous Message Receiving
   * The caller should return the exact number of bytes written
   * to the runtime if some bytes are written. The runtime
   * will increment the read offset accordingly.
   *
   * If the caller is unable to receive any bytes,
   * it should return `PENDING = -1` to the runtime
   * to interrupt message receiving task. The read offset
   * will not be incremented.
   *
   * When the caller is able to receive bytes again,
   * it should call `waker.wake()` to wake up the runtime.
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
     * # Asynchronous Message Receiving
     * The caller should return the exact number of bytes written
     * to the runtime if some bytes are written. The runtime
     * will increment the read offset accordingly.
     *
     * If the caller is unable to receive any bytes,
     * it should return `PENDING = -1` to the runtime
     * to interrupt message receiving task. The read offset
     * will not be incremented.
     *
     * When the caller is able to receive bytes again,
     * it should call `waker.wake()` to wake up the runtime.
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
     * Throwing runtime_error in `on_message` callback will cause
     * the connection to close.
     *
     * @param data the message received.
     */
    virtual long on_message_async(std::string_view data, Waker &&waker) = 0;

    friend long::socket_manager_extern_on_msg(
            struct SOCKET_MANAGER_C_API_OnMsgObj this_,
            SOCKET_MANAGER_C_API_ConnMsg msg,
            SOCKET_MANAGER_C_API_CWaker waker,
            char **err);

  };

  /**
   * If the caller has unlimited buffer implementation,
   * it can use this simplified class to receive messages.
   *
   * The caller should implement `on_message` method to
   * store the received message in buffer or queue and immediately
   * return `on_message` method, and should not block the runtime.
   *
   * # Thread Safety
   * This callback must be thread safe.
   */
  class MsgReceiver : public MsgReceiverAsync {
  public:

    ~MsgReceiver() override = default;

  private:

    virtual void on_message(std::string_view data) = 0;

    long on_message_async(std::string_view data, Waker &&waker) override;
  };

} // namespace socket_manager

#endif //SOCKET_MANAGER_MSG_RECEIVER_H
