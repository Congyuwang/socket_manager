#ifndef SOCKET_MANAGER_MSG_SENDER_H
#define SOCKET_MANAGER_MSG_SENDER_H

#include "socket_manager_c_api.h"
#include "connection.h"
#include "send_waker.h"
#include <string>
#include <memory>
#include <functional>

namespace socket_manager {

  class Connection;

  /**
   * Use MsgSender to send messages to the peer.
   *
   * Drop the MsgSender object to close the connection.
   */
  class MsgSender {

  public:

    /**
     * Send a message to the peer.
     *
     * # Blocking!!
     * This method might block, so it should
     * never be used within the callbacks.
     *
     * # Thread Safety
     * This method is thread safe.
     * This method does not implement backpressure
     * (i.e., it caches all the messages in memory).
     *
     * # Errors
     * This method throws std::runtime_error when
     * the connection is closed.
     *
     * @param data the message to send
     */
    void send(std::string_view data);

    /**
     * Non blocking message sending.
     *
     * DO NOT USE THIS METHOD in mixture with `send` method.
     * Since send method is blocking, it preserves the order,
     * while this method must be used with the waker class.
     *
     * @param data the message to send
     * @param offset the offset of the message to send.
     *   That is data[offset..] is the message to send.
     *   Increment the offset based on the return value.
     * @param waker `waker.wake()` is evoked when try_send
     *   could accept more data. Pass nullptr to disable wake
     *   notification.
     * @return If waker is provided, returns the number of bytes sent on success,
     *   and 0 on connection closed, -1 on pending.
     *   If waker is not provided, returns the number of bytes sent.
     *   0 might indicate the connection is closed, or the message buffer is full.
     */
    long try_send(std::string_view data, size_t offset, const std::shared_ptr<Waker> &waker = nullptr);

    /**
     * Manually flush the internal buffer.
     *
     * # Thread Safety
     * This method is thread safe.
     *
     */
    void flush();

    /**
     * Drop the sender to close the connection.
     */
    ~MsgSender();

    MsgSender(const MsgSender &) = delete;

    MsgSender &operator=(const MsgSender &) = delete;

  private:

    friend class Connection;

    friend void::socket_manager_extern_on_conn(struct OnConnObj this_, ConnStates conn, char **err);

    explicit MsgSender(CMsgSender *inner, const std::shared_ptr<Connection> &);

    // keep a reference of connection for storing waker object
    // in connection, to prevent dangling pointer of waker.
    std::shared_ptr<Connection> conn;

    CMsgSender *inner;

  };

} // namespace socket_manager

#endif // SOCKET_MANAGER_MSG_SENDER_H
