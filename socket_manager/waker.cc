#include "socket_manager/common/waker.h"

namespace socket_manager {

  Waker::Waker()
          : waker(CWaker{nullptr, nullptr}) {}

  Waker::Waker(CWaker waker) : waker(waker) {}

  Waker::Waker(Waker &&other) noexcept: waker(other.waker) {
    other.waker.Data = nullptr;
    other.waker.Vtable = nullptr;
  }

  Waker &Waker::operator=(Waker &&other) noexcept {
    waker = other.waker;
    other.waker.Data = nullptr;
    other.waker.Vtable = nullptr;
    return *this;
  }

  void Waker::wake() {
    if (waker.Data != nullptr && waker.Vtable != nullptr) {
      msg_waker_wake(&waker);
    }
  }

  Waker::~Waker() {
    if (waker.Data != nullptr && waker.Vtable != nullptr) {
      msg_waker_free(waker);
    }
  }

} // namespace socket_manager
