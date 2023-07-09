#undef NDEBUG
#include "test_utils.h"

using namespace socket_manager;

int test_error_call_after_abort(int argc, char **argv) {
  auto nothing_cb = std::make_shared<DoNothingConnCallback>();
  SocketManager nothing(nothing_cb);

  nothing.abort();

  // shouldn't time out
  nothing.join();

  try {
    nothing.connect_to_addr("127.0.0.1:40103");
    // should not reach here
    return -1;
  } catch (std::runtime_error &e) {
    std::cout << "connect_to_addr after abort-join throw error: " << e.what() << std::endl;
  }

  // should not throw error on abort after abort
  nothing.abort();

  // should not throw error on join after join
  nothing.join();

  return 0;
}
