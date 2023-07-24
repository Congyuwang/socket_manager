#include "socket_manager/recv_waker.h"

RecvWaker::RecvWaker()
        : waker(CWaker{nullptr, nullptr}) {}

RecvWaker::RecvWaker(CWaker waker) : waker(waker) {}

RecvWaker::RecvWaker(RecvWaker &&other) noexcept: waker(other.waker) {
  other.waker.Data = nullptr;
  other.waker.Vtable = nullptr;
}

RecvWaker &RecvWaker::operator=(RecvWaker &&other) noexcept {
  waker = other.waker;
  other.waker.Data = nullptr;
  other.waker.Vtable = nullptr;
  return *this;
}

void RecvWaker::wake() {
  if (waker.Data != nullptr && waker.Vtable != nullptr) {
    msg_waker_wake(&waker);
  }
}

RecvWaker::~RecvWaker() {
  if (waker.Data != nullptr && waker.Vtable != nullptr) {
    msg_waker_free(waker);
  }
}
