#ifndef SOCKET_MANAGER_RECV_WAKER_H
#define SOCKET_MANAGER_RECV_WAKER_H

#include "socket_manager_c_api.h"

/**
 * The RecvWaker is returned by `on_message_async`,
 * and is to resume the receiver process.
 *
 * When the workload is large, the implementation of
 * `on_message_async` can return `PENDING = -1`,
 * to interrupt the receiver callback, while resuming
 * the receiver using RecvWaker later.
 */
class RecvWaker {
public:
  /**
   * Call wake() to wake up the receiver process.
   */
  void wake();

  ~RecvWaker();

  /**
   * Create an empty waker.
   */
  explicit RecvWaker();

  RecvWaker(const RecvWaker &) = delete;

  RecvWaker &operator=(const RecvWaker &) = delete;

  RecvWaker(RecvWaker &&) noexcept ;

  RecvWaker &operator=(RecvWaker &&) noexcept ;

private:
  explicit RecvWaker(CWaker waker);

  friend long::socket_manager_extern_on_msg(struct OnMsgObj this_, ConnMsg msg, CWaker waker, char **err);

  CWaker waker;
};

#endif //SOCKET_MANAGER_RECV_WAKER_H
