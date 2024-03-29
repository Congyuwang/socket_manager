#ifndef SOCKET_MANAGER_CONNECTION_H
#define SOCKET_MANAGER_CONNECTION_H

#include "msg_receiver.h"
#include "socket_manager_c_api.h"
#include <functional>
#include <memory>
#include <string>

namespace socket_manager {

const unsigned long long DEFAULT_WRITE_FLUSH_MILLI_SEC = 1;    // 1 millisecond
const unsigned long long DEFAULT_READ_MSG_FLUSH_MILLI_SEC = 1; // 1 millisecond
const size_t DEFAULT_MSG_BUF_SIZE =
    static_cast<const size_t>(64) * 1024; // 64KB

class MsgSender;

class Notifier;

/**
 * Use Connection to send and receive messages from
 * established connections.
 */
class Connection {

public:
  /**
   * Start a connection.
   *
   * <h3>Start / Close</h3>
   * If start is not called, close must be called to prevent memory leak.
   *
   * <h3>Close started connection</h3>
   * Drop the returned MsgSender object to close the `Write` side of the
   * connection after starting it.
   *
   * <h3>Thread Safety</h3>
   * Thread safe, but should be called exactly once,
   * otherwise throws error.
   *
   * @param msg_receiver the message receiver callback to receive
   *    messages from the peer. Non-null.
   * @param send_notifier the notifier for getting notified when the
   *    send buffer is ready. Pass nullptr to use a noop notifier.
   *    This parameter is needed only for async sending.
   * @param msg_buffer_size The size of the message buffer in bytes.
   *    The minimum is 8KB, and the maximum is 8MB. Default to 64KB.
   * @param write_flush_interval The interval in `milliseconds`
   *    of write buffer auto flushing. Set to 0 to disable auto flush.
   *    Default to 1 millisecond.
   * @param read_msg_flush_interval The interval in `milliseconds` of read
   * message buffer auto flushing. The value is ignored when `msg_buffer_size`
   * is 0. Set to 0 to disable auto flush (which is not recommended since there
   * is no manual flush, and small messages might get stuck in buffer). Default
   * to 1 millisecond.
   */
  void start(
      std::shared_ptr<MsgReceiverAsync> msg_receiver,
      std::shared_ptr<Notifier> send_notifier = nullptr,
      size_t msg_buffer_size = DEFAULT_MSG_BUF_SIZE,
      unsigned long long read_msg_flush_interval =
          DEFAULT_READ_MSG_FLUSH_MILLI_SEC,
      unsigned long long write_flush_interval = DEFAULT_WRITE_FLUSH_MILLI_SEC);

  /**
   * Get a string representation of the peer address.
   */
  std::string peer_address();

  /**
   * Get a string representation of the local address.
   */
  std::string local_address();

  /**
   * The close API works either before `start_connection` is called
   * or after the `on_connect` callback.
   * It won't have effect between the two points
   */
  void close();

private:
  friend class MsgSender;

  friend void ::socket_manager_extern_on_conn(
      struct SOCKET_MANAGER_C_API_OnConnObj this_,
      SOCKET_MANAGER_C_API_ConnStates states, char **err);

  // keep the msg_receiver alive
  std::shared_ptr<MsgReceiverAsync> receiver;

  // keep the notifier alive
  std::shared_ptr<Notifier> notifier;

  explicit Connection(SOCKET_MANAGER_C_API_Connection *inner);

  std::unique_ptr<SOCKET_MANAGER_C_API_Connection,
                  std::function<void(SOCKET_MANAGER_C_API_Connection *)>>
      inner;
};

} // namespace socket_manager

#endif // SOCKET_MANAGER_CONNECTION_H
