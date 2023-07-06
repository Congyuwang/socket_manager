#include "test_utils.h"

using namespace socket_manager;

int test_bad_address_listen(int argc, char **argv) {
  auto nothing_cb = std::make_shared<DoNothingConnCallback>();
  SocketManager<DoNothingConnCallback, DoNothingReceiver> nothing(nothing_cb);
  try {
    nothing.listen_on_addr("bad_address");
  } catch (const std::runtime_error &e) {
    return 0;
  }
  return 1;
}
