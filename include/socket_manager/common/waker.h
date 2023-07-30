#ifndef SOCKET_MANAGER_WAKER_H
#define SOCKET_MANAGER_WAKER_H

#include "socket_manager_c_api.h"

namespace socket_manager {

/**
 * Return `PENDING` to interrupt runtime task.
 */
const long PENDING = -1;

/**
 * @brief Waker is used to wake up a pending runtime task.
 *
 * The implementation of `MsgReceiverAsync::on_message_async`
 * can return `PENDING = -1` to interrupt a message receiving
 * task in the runtime (e.g. when the caller buffer is full).
 *
 * And use `waker.wake()` to resume the message receiving task
 * when the caller buffer is ready.
 *
 * <h3>Resource Leak Note</h3>
 * The `Waker` must be properly destroyed to avoid resource leak.
 */
class Waker {
public:
  /**
   * Call wake() to wake up the receiver process.
   */
  void wake();

  ~Waker();

  /**
   * Create an empty noop waker.
   */
  explicit Waker();

  Waker(const Waker &) = delete;

  Waker &operator=(const Waker &) = delete;

  Waker(Waker &&) noexcept;

  Waker &operator=(Waker &&) noexcept;

private:
  explicit Waker(SOCKET_MANAGER_C_API_CWaker waker);

  friend long ::socket_manager_extern_on_msg(
      struct SOCKET_MANAGER_C_API_OnMsgObj this_,
      SOCKET_MANAGER_C_API_ConnMsg msg, SOCKET_MANAGER_C_API_CWaker waker,
      char **err);

  SOCKET_MANAGER_C_API_CWaker waker;
};
} // namespace socket_manager

#endif // SOCKET_MANAGER_WAKER_H
