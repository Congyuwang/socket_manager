#include "test_utils.h"

using namespace socket_manager;

int test_error_call_after_abort(int argc, char **argv) {
  auto nothing_cb = std::make_shared<DoNothingConnCallback>();
  SocketManager<DoNothingConnCallback> nothing(nothing_cb);

  nothing.abort();

  // shouldn't time out
  nothing.join();

  try {
    nothing.connect_to_addr("127.0.0.1:12345");
    // should not reach here
    return -1;
  } catch (std::runtime_error &e) {
    std::cout << "connect_to_addr after abort-join throw error: " << e.what() << std::endl;
  }

  try {
    nothing.abort();
    // should not reach here
    return -1;
  } catch (std::runtime_error &e) {
    std::cout << "abort after abort-join throw error: " << e.what() << std::endl;
  }

  return 0;
}
