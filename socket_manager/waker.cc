#include "socket_manager/common/waker.h"

namespace socket_manager {

Waker::Waker() : waker(SOCKET_MANAGER_C_API_CWaker{nullptr, nullptr}) {}

Waker::Waker(SOCKET_MANAGER_C_API_CWaker waker) : waker(waker) {}

Waker::Waker(Waker &&other) noexcept : waker(other.waker) {
  other.waker.Data = nullptr;
  other.waker.Vtable = nullptr;
}

Waker &Waker::operator=(Waker &&other) noexcept {
  if (waker.Data != nullptr && waker.Vtable != nullptr) {
    socket_manager_waker_free(waker);
  }
  waker = other.waker;
  other.waker.Data = nullptr;
  other.waker.Vtable = nullptr;
  return *this;
}

void Waker::wake() {
  if (waker.Data != nullptr && waker.Vtable != nullptr) {
    socket_manager_waker_wake(&waker);
  }
}

Waker::~Waker() {
  if (waker.Data != nullptr && waker.Vtable != nullptr) {
    socket_manager_waker_free(waker);
  }
}

} // namespace socket_manager
