#ifndef SOCKET_MANAGER_CONNECTION_H
#define SOCKET_MANAGER_CONNECTION_H

#include <atomic>
#include <memory>
#include <stdexcept>
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
    template<class Rcv>
    std::shared_ptr<MsgSender> start(
            std::unique_ptr<Rcv> msg_receiver,
            unsigned long long write_flush_interval = DEFAULT_WRITE_FLUSH_MILLI_SEC) {

      static_assert(
              std::is_base_of<MsgReceiver, Rcv>::value,
              "msg_receiver should be derived from MsgReceiver");

      // start the connection.
      // calling twice `connection_start` will throw exception.
      char *err = nullptr;
      CMsgSender *sender = connection_start(inner, OnMsgObj{
              msg_receiver.get(),
      }, write_flush_interval, &err);
      if (sender == nullptr) {
        const std::string err_str(err);
        free(err);
        throw std::runtime_error(err_str);
      }

      // keep the msg_receiver alive.
      receiver = std::move(msg_receiver);

      // return the sender
      return std::shared_ptr<MsgSender>(new MsgSender(sender));
    }

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
    void close() {
      char *err = nullptr;
      if (connection_close(inner, &err)) {
        const std::string err_str(err);
        free(err);
        throw std::runtime_error(err_str);
      }
    }

    Connection(const Connection &) = delete;

    void operator=(const Connection &) = delete;

    ~Connection() {
      connection_free(inner);
    }

  private:

    friend class ConnCallback;

    friend char* ::socket_manager_extern_on_conn(void *this_, ConnStates conn);

    // keep the msg_receiver alive
    std::unique_ptr<MsgReceiver> receiver;

    explicit Connection(CConnection *inner) : inner(inner) {}

    CConnection *inner;

  };

} // namespace socket_manager

#endif //SOCKET_MANAGER_CONNECTION_H
