#ifndef SOCKET_MANAGER_SEND_WAKER_H
#define SOCKET_MANAGER_SEND_WAKER_H

#include "socket_manager_c_api.h"

namespace socket_manager {
  /**
 * Used for receiving writable notification for
 * `try_send` method.
 *
 * Each `try_test()` call releases the waker,
 * when `wake()` is actually invoked
 * (i.e., the number of calls of `release` and `clone`
 * are equal).
 */
  class SendWaker {

  public:
    virtual ~SendWaker() = default;

  private:

    virtual void wake() = 0;

    virtual void release() = 0;

    virtual void clone() = 0;

    friend void::socket_manager_extern_sender_waker_wake(struct WakerObj this_);

    friend void::socket_manager_extern_sender_waker_release(struct WakerObj this_);

    friend void::socket_manager_extern_sender_waker_clone(struct WakerObj this_);
  };

  class NoopWaker : public SendWaker {
  public:
    void wake() override {}

    void release() override {}

    void clone() override {}
  };
}

#endif //SOCKET_MANAGER_SEND_WAKER_H
