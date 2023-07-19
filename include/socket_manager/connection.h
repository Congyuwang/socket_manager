#ifndef SOCKET_MANAGER_CONNECTION_H
#define SOCKET_MANAGER_CONNECTION_H

#include <atomic>
#include <memory>
#include <stdexcept>
#include "msg_receiver.h"
#include "msg_sender.h"
#include "socket_manager_c_api.h"

namespace socket_manager {

  static unsigned long long DEFAULT_WRITE_FLUSH_MILLI_SEC = 5; // 5 millisecond
  static unsigned long long DEFAULT_READ_MSG_FLUSH_MILLI_SEC = 5; // 5 millisecond
  static size_t DEFAULT_MSG_BUF_SIZE = 64 * 1024; // 64KB

  class MsgSender;

  class Waker;

  /**
   * Use Connection to send and receive messages from
   * established connections.
   */
  class Connection : public std::enable_shared_from_this<Connection> {

  public:

    /**
     * Start a connection.
     *
     * # Start / Close
     * Exactly one of `start` or `close` should be called!
     * Calling more than once will throw runtime exception.
     * Not calling any of them might result in resource leak.
     *
     * # Close started connection
     * Drop the returned MsgSender object to close the connection
     * after starting it.
     *
     * # Thread Safety
     * Thread safe, but should be called exactly once,
     * otherwise throws error.
     *
     * To close the connection, drop the returned
     * MsgSender object.
     *
     * @param msg_receiver the message receiver callback to
     *                    receive messages from the peer.
     * @param msg_buffer_size The size of the message buffer in bytes.
     *    Set to 0 to use no buffer (i.e., call `on_msg` immediately on receiving
     *    any data, expecting the user to implement buffer if needed).
     *    The minimum is 8KB, and the maximum is 8MB. Default to 64KB.
     * @param write_flush_interval The interval in `milliseconds`
     *    of write buffer auto flushing. Set to 0 to disable auto flush.
     *    Default to 1 millisecond.
     * @param read_msg_flush_interval The interval in `milliseconds` of read message buffer
     *    auto flushing. The value is ignored when `msg_buffer_size` is 0.
     *    Set to 0 to disable auto flush (which is not recommended since there is no
     *    manual flush, and small messages might get stuck in buffer).
     *    Default to 1 millisecond.
     */
    std::shared_ptr<MsgSender> start(
            std::unique_ptr<MsgReceiver> msg_receiver,
            size_t msg_buffer_size = DEFAULT_MSG_BUF_SIZE,
            unsigned long long read_msg_flush_interval = DEFAULT_READ_MSG_FLUSH_MILLI_SEC,
            unsigned long long write_flush_interval = DEFAULT_WRITE_FLUSH_MILLI_SEC);

    /**
     * Close the connection without using it.
     *
     * `on_connection_close` callback will be called.
     *
     * # Start / Close
     * Exactly one of `start` or `close` should be called!
     * Calling more than once will throw runtime exception.
     * Not calling any of them might result in resource leak.
     */
    void close();

    Connection(const Connection &) = delete;

    void operator=(const Connection &) = delete;

    ~Connection();

  private:

    friend class MsgSender;

    friend char* ::socket_manager_extern_on_conn(struct OnConnObj this_, ConnStates conn);

    // keep the msg_receiver alive
    std::unique_ptr<MsgReceiver> receiver;

    // keep the waker alive
    std::shared_ptr<Waker> waker;

    explicit Connection(CConnection *inner);

    CConnection *inner;

  };

} // namespace socket_manager

#endif //SOCKET_MANAGER_CONNECTION_H
