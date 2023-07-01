#ifndef SOCKET_MANAGER_CONNECTION_H
#define SOCKET_MANAGER_CONNECTION_H

#include <atomic>
#include <memory>
#include "msg_receiver.h"
#include "msg_sender.h"
#include "socket_manager_c_api.h"

namespace socket_manager {

  static unsigned long long DEFAULT_WRITE_FLUSH_MILLI_SEC = 10;

  /**
   * Use Connection to send and receive messages from
   * established connections.
   */
  class Connection {

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
     * @param write_flush_interval The interval in `milliseconds`
     * of write buffer auto flushing. Set to 0 to disable auto flush.
     * Default to 20 milliseconds.
     */
    std::shared_ptr<MsgSender> start(
            std::unique_ptr<MsgReceiver> msg_receiver,
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

    friend class ConnCallback;

    // keep the msg_receiver alive
    std::unique_ptr<MsgReceiver> receiver;
    std::atomic_bool started;

    explicit Connection(CConnection *inner);

    CConnection *inner;

  };

} // namespace socket_manager

#endif //SOCKET_MANAGER_CONNECTION_H
