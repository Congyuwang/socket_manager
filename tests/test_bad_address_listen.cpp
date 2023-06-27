#include "test_utils.h"

using namespace socket_manager;

int test_bad_address_listen(int argc, char **argv) {
  DoNothingSocketManager nothing;
  nothing.detach();
  try {
    nothing.listen_on_addr("bad_address");
  } catch (const std::runtime_error &e) {
    return 0;
  }
  return 1;
}
