#include "socket_manager/recv_waker.h"

RcvWaker::RcvWaker(CWaker *waker) : waker(waker) {}

void RcvWaker::wake() {
  msg_waker_wake(waker);
}

RcvWaker::~RcvWaker() {
  msg_waker_destroy(waker);
}
