#ifndef SOCKET_MANAGER_MSG_SENDER_H
#define SOCKET_MANAGER_MSG_SENDER_H

#include "socket_manager_c_api.h"
#include "connection.h"
#include "notifier.h"
#include <functional>
#include <string>
#include <memory>
#include <functional>

namespace socket_manager {

  class Connection;

  /**
   * Use MsgSender to send messages to the peer.
   * <br /><br />
   * Drop the MsgSender object to close the connection.
   */
  class MsgSender {

  public:
    /**
     * Asynchronous message sending.
     * <br /><br />
     * To use the method, the user should pass a `notifier` object
     * when calling `Connection::start()` in order to receive notification
     * when the send buffer is ready.
     *
     * <h3>Async control flow (IMPORTANT)</h3>
     * This function is non-blocking, it returns `PENDING = -1`
     * if the send buffer is full. So the caller should wait
     * by passing a `Notifier` which will be called when the
     * buffer is ready.
     * <br /><br />
     * When the buffer is ready, the function returns number of bytes sent.
     * <br /><br />
     * The caller is responsible for updating the buffer offset!!
     *
     * @param data the message to send
     * @param notifier `notifier.wake()` is evoked when send_async
     *   could accept more data.
     * @return return the number of bytes successfully sent,
     *   or return `PENDING = -1` if the send buffer is full.
     * @throws std::runtime_error when the connection is closed.
     */
    long send_async(std::string_view data);

    /**
     * Non-blocking message sending (NO BACKPRESSURE).
     * <br /><br />
     * This method is non-blocking. It returns immediately and
     * caches all the data in the internal buffer. So it comes
     * without back pressure.
     *
     * <h3>Performance</h3>
     * This method is generally slower than `send_async()` or
     * `send_block()` since it involves memory allocation during
     * message sending.
     *
     * @param data the message to send
     * @param notifier `notifier.wake()` is evoked when send_async
     *   could accept more data.
     * @throws std::runtime_error when the connection is closed.
     */
    void send_nonblock(std::string_view data);

    /**
     * Blocking message sending (DO NOT USE IN ASYNC CALLBACK).
     *
     * <h3>Blocking!!</h3>
     * This method might block, so it should never be used within any of
     * the async callbacks. Otherwise the runtime can panic!
     *
     * <h3>Thread Safety</h3>
     * This method is thread safe.
     *
     * <h3>Errors</h3>
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
     * <h3>Thread Safety</h3>
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
