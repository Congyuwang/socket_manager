#ifndef SOCKET_MANAGER_MSG_RECEIVER_H
#define SOCKET_MANAGER_MSG_RECEIVER_H

#include "socket_manager_c_api.h"
#include "waker.h"
#include <string_view>

namespace socket_manager {

/**
 * Implement this class to receive messages from Connection.
 * <br /><br />
 * Must read the following details to implement correctly!
 *
 * <h3>Asynchronous Message Receiving</h3>
 * The caller should return the exact number of bytes written
 * to the runtime if some bytes are written. The runtime
 * will increment the read offset accordingly.
 * <br /><br />
 * If the caller is unable to receive any bytes,
 * it should return `PENDING = -1` to the runtime
 * to interrupt message receiving task. The read offset
 * will not be incremented.
 * <br /><br />
 * When the caller is able to receive bytes again,
 * it should call `waker.wake()` to wake up the runtime.
 *
 * <h3>Non-blocking</h3>
 * This callback must be non-blocking.
 */
class MsgReceiverAsync {

public:
  virtual ~MsgReceiverAsync() = default;

private:
  /**
   * Called when a message is received.
   *
   * <h3>Asynchronous Message Receiving</h3>
   * The caller should return the exact number of bytes written
   * to the runtime if some bytes are written. The runtime
   * will increment the read offset accordingly.
   * <br /><br />
   * If the caller is unable to receive any bytes,
   * it should return `PENDING = -1` to the runtime
   * to interrupt message receiving task. The read offset
   * will not be incremented.
   * <br /><br />
   * When the caller is able to receive bytes again,
   * it should call `waker.wake()` to wake up the runtime.
   *
   * <h3>MEMORY SAFETY</h3>
   * The `data` is only valid during the call of this function.
   * If you want to keep the data, you should copy it.
   *
   * <h3>Non-blocking</h3>
   * This callback must be non-blocking.
   *
   * <h3>Error Handling</h3>
   * Throwing runtime_error in `on_message` callback will cause
   * the connection to close.
   *
   * @param data the message received.
   */
  virtual long on_message_async(std::string_view data, Waker &&waker) = 0;

  friend long ::socket_manager_extern_on_msg(
      struct SOCKET_MANAGER_C_API_OnMsgObj this_,
      SOCKET_MANAGER_C_API_ConnMsg msg, SOCKET_MANAGER_C_API_CWaker waker,
      char **err);
};

/**
 * If the caller has unlimited buffer implementation,
 * it can use this simplified class to receive messages.
 * <br /><br />
 * The caller should implement `on_message` method to
 * store the received message in buffer or queue and immediately
 * return `on_message` method, and should not block the runtime.
 *
 * <h3>Non-blocking</h3>
 * This callback must be non-blocking.
 */
class MsgReceiver : public MsgReceiverAsync {
public:
  ~MsgReceiver() override = default;

private:
  /**
   * Compared to `on_message_async`, this method assumes that
   * all data is received by the caller, and the caller does
   * not need to report number of bytes written to the runtime.
   * Nor can the caller interrupt the runtime.
   * <br /><br />
   * Notice that this callback still needs to be non-blocking.
   * @param data the message received.
   */
  virtual void on_message(std::string_view data) = 0;

  long on_message_async(std::string_view data, Waker &&waker) final;
};

} // namespace socket_manager

#endif // SOCKET_MANAGER_MSG_RECEIVER_H
