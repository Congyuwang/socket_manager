#ifndef SOCKET_MANAGER_MSG_SENDER_H
#define SOCKET_MANAGER_MSG_SENDER_H

#include "socket_manager_c_api.h"
#include "connection.h"
#include <string>
#include <memory>
#include <functional>

namespace socket_manager {

  class Connection;

  /**
   * Used for receiving writable notification for
   * `try_send` method.
   */
  class Waker {

  public:
    virtual ~Waker() = default;

  private:

    virtual void wake() = 0;

    virtual void release() = 0;

    virtual void clone() = 0;

    friend void ::socket_manager_extern_sender_waker_wake(struct WakerObj this_);

    friend void ::socket_manager_extern_sender_waker_release(struct WakerObj this_);

    friend void ::socket_manager_extern_sender_waker_clone(struct WakerObj this_);
  };

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
    void send(const std::string &data);

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
    long try_send(const std::string &data, size_t offset, const std::shared_ptr<Waker> &waker = nullptr);

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

    void operator=(const MsgSender &) = delete;

  private:

    friend class Connection;

    explicit MsgSender(CMsgSender *inner, const std::shared_ptr<Connection>&);

    std::shared_ptr<Connection> conn;

    CMsgSender *inner;

  };

} // namespace socket_manager

#endif // SOCKET_MANAGER_MSG_SENDER_H
