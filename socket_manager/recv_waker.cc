#include "socket_manager/recv_waker.h"

RcvWaker::RcvWaker(CWaker waker) : waker(waker) {}

RcvWaker::RcvWaker(RcvWaker &&other) noexcept : waker(other.waker) {
  other.waker.Data = nullptr;
  other.waker.Vtable = nullptr;
}

RcvWaker &RcvWaker::operator=(RcvWaker &&other) noexcept {
  waker = other.waker;
  other.waker.Data = nullptr;
  other.waker.Vtable = nullptr;
  return *this;
}

void RcvWaker::wake() {
  if (waker.Data != nullptr && waker.Vtable != nullptr) {
    msg_waker_wake(&waker);
  }
}

RcvWaker::~RcvWaker() {
  if (waker.Data != nullptr && waker.Vtable != nullptr) {
    msg_waker_free(waker);
  }
}
