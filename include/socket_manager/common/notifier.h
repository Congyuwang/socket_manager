#ifndef SOCKET_MANAGER_NOTIFIER_H
#define SOCKET_MANAGER_NOTIFIER_H

#include "socket_manager_c_api.h"

namespace socket_manager {
  /**
   * @brief The Notifier class is used to receive notification
   * from rust runtime to request c/c++ task for further
   * execution.
   *
   * @note Our current implementation does not require ref count
   * since we bound the lifetime of the notifier to the
   * lifetime of the connection, and guarantee that the passed
   * notifier is valid by keeping a reference of the notifier
   * in the related connection.
   */
  class Notifier {

  public:
    virtual ~Notifier() = default;

  private:

    virtual void wake() = 0;

    void release() {};

    void clone() {};

    friend void::socket_manager_extern_notifier_wake(struct SOCKET_MANAGER_C_API_Notifier this_);

    friend void::socket_manager_extern_notifier_release(struct SOCKET_MANAGER_C_API_Notifier this_);

    friend void::socket_manager_extern_notifier_clone(struct SOCKET_MANAGER_C_API_Notifier this_);
  };

  class NoopNotifier : public Notifier {
  public:
    void wake() override {}
  };
}

#endif //SOCKET_MANAGER_NOTIFIER_H
