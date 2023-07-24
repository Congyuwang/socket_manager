#ifndef SOCKET_MANAGER_RECV_WAKER_H
#define SOCKET_MANAGER_RECV_WAKER_H

#include "socket_manager_c_api.h"

class RcvWaker {
public:
  void wake();

  ~RcvWaker();

  RcvWaker(const RcvWaker &) = delete;

  void operator=(const RcvWaker &) = delete;

private:
  explicit RcvWaker(CWaker* waker);

  friend int ::socket_manager_extern_on_msg(struct OnMsgObj this_, ConnMsg msg, CWaker *waker, char **err);

  CWaker* waker;
};

#endif //SOCKET_MANAGER_RECV_WAKER_H
