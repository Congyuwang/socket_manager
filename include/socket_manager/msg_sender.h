#ifndef SOCKET_MANAGER_MSG_SENDER_H
#define SOCKET_MANAGER_MSG_SENDER_H

#include "socket_manager_c_api.h"
#include "connection.h"
#include "socket_manager/common/notifier.h"
#include <functional>
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
     * Non blocking message sending.
     *
     * # Async control flow (IMPORTANT)
     *
     * This function is non-blocking, it returns `PENDING = -1`
     * if the send buffer is full. So the caller should wait
     * by passing a `Notifier` which will be called when the
     * buffer is ready.
     *
     * When the buffer is ready, the function returns number of bytes sent.
     *
     * The caller is responsible for updating the buffer offset!!
     *
     * The function requires a `waker` reference, and the caller is recommended
     * to store the waker in the `connection` and reuse it to reduce the overhead
     * of allocation.
     *
     * @param data the message to send
     * @param notifier `notifier.wake()` is evoked when send_async
     *   could accept more data.
     * @return return the number of bytes successfully sent,
     *   or return `PENDING = -1` if the send buffer is full.
     * @throws std::runtime_error when the connection is closed.
     */
    long send_async(std::string_view data, const std::shared_ptr<Notifier> &notifier);

    /**
     * Send a message to the peer.
     *
     * # Blocking!!
     * This method might block, so it should never be used within the callbacks.
     *
     * # Thread Safety
     * This method is thread safe.
     *
     * # Errors
     * This method throws std::runtime_error when
     * the connection is closed.
     *
     * @param data the message to send
     * @throws std::runtime_error when the connection is closed.
     */
    void send_block(std::string_view data);

    /**
     * Manually flush the internal buffer.
     *
     * # Thread Safety
     * This method is thread safe.
     *
     */
    void flush();

  private:

    friend class Connection;

    friend void::socket_manager_extern_on_conn(
            struct SOCKET_MANAGER_C_API_OnConnObj this_,
            SOCKET_MANAGER_C_API_ConnStates conn,
            char **err);

    explicit MsgSender(SOCKET_MANAGER_C_API_MsgSender *inner, const std::shared_ptr<Connection> &);

    // keep a reference of connection for storing waker object
    // in connection, to prevent dangling pointer of waker.
    std::shared_ptr<Connection> conn;

    std::unique_ptr<SOCKET_MANAGER_C_API_MsgSender,
            std::function<void(SOCKET_MANAGER_C_API_MsgSender *)>> inner;

  };

} // namespace socket_manager

#endif // SOCKET_MANAGER_MSG_SENDER_H
