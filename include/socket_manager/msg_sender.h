#ifndef SOCKET_MANAGER_MSG_SENDER_H
#define SOCKET_MANAGER_MSG_SENDER_H

#include "socket_manager_c_api.h"
#include <string>

namespace socket_manager {

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
     * @param data the message to send
     */
    void send(const std::string &data);

    /**
     * Non blocking message sending.
     *
     * @param data the message to send
     * @param offset the offset of the message to send.
     *   That is data[offset..] is the message to send.
     *   Increment the offset based on the return value.
     */
    size_t try_send(const std::string &data, size_t offset);

    /**
     * This function is invoked when sender allows
     * receiving more data.
     *
     * Override this method to implement notification
     * on when the sender allows receiving more data.
     */
    virtual void waker();

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
    virtual ~MsgSender();

    MsgSender(const MsgSender &) = delete;

    void operator=(const MsgSender &) = delete;

  private:

    friend class Connection;

    friend void ::socket_manager_extern_sender_waker_wake(struct MsgSenderObj this_);

    friend void ::socket_manager_extern_sender_waker_release(struct MsgSenderObj this_);

    friend void ::socket_manager_extern_sender_waker_clone(struct MsgSenderObj this_);

    explicit MsgSender(CMsgSender *inner);

    std::atomic_size_t waker_ref_count;

    CMsgSender *inner;

  };

} // namespace socket_manager

#endif // SOCKET_MANAGER_MSG_SENDER_H
