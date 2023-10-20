#undef NDEBUG
#include "test_utils.h"
#include <thread>

using namespace socket_manager;

void abort_manager(SocketManager &manager) { manager.abort(); }

int test_abort_join(int argc, char **argv) {
  SpdLogger::init();
  auto nothing_cb = std::make_shared<DoNothingConnCallback>();
  SocketManager nothing(nothing_cb);

  std::thread abort_t(abort_manager, std::ref(nothing));

  // shouldn't time out
  nothing.join();

  abort_t.join();

  return 0;
}
