#ifndef SOCKET_MANAGER_RECV_WAKER_H
#define SOCKET_MANAGER_RECV_WAKER_H

#include "socket_manager_c_api.h"

/**
 * The RcvWaker is returned by `on_message_async`,
 * and is to resume the receiver process.
 *
 * When the workload is large, the implementation of
 * `on_message_async` can return `PENDING = -1`,
 * to interrupt the receiver callback, while resuming
 * the receiver using RcvWaker later.
 */
class RcvWaker {
public:
  /**
   * Call wake() to wake up the receiver process.
   */
  void wake();

  ~RcvWaker();

  /**
   * Create an empty waker.
   */
  explicit RcvWaker();

  RcvWaker(const RcvWaker &) = delete;

  RcvWaker &operator=(const RcvWaker &) = delete;

  RcvWaker(RcvWaker &&) noexcept ;

  RcvWaker &operator=(RcvWaker &&) noexcept ;

private:
  explicit RcvWaker(CWaker waker);

  friend long::socket_manager_extern_on_msg(struct OnMsgObj this_, ConnMsg msg, CWaker waker, char **err);

  CWaker waker;
};

#endif //SOCKET_MANAGER_RECV_WAKER_H
