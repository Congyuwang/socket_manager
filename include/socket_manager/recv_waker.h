#ifndef SOCKET_MANAGER_RECV_WAKER_H
#define SOCKET_MANAGER_RECV_WAKER_H

#include "socket_manager_c_api.h"

class RcvWaker {
public:
  /**
   * Call wake() to wake up the receiver,
   * and return `PENDING` in `on_message_async`
   * to interrupt the receiver.
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
