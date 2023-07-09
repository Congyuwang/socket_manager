#undef NDEBUG
#include <socket_manager/socket_manager.h>
#include "test_utils.h"

int test_error_listen(int argc, char **argv) {
  std::mutex lock;
  std::condition_variable cond;
  std::atomic_int sig(0);
  std::vector<std::tuple<std::string, std::shared_ptr<std::string>>> buffer;

  auto test_cb = std::make_shared<BitFlagCallback>(lock, cond, sig, buffer);
  SocketManager test(test_cb);
  test.listen_on_addr("127.0.0.1:40105");
  test.listen_on_addr("127.0.0.1:40105");

  // Wait for the connection to fail
  while (true) {
    int load_sig = sig.load(std::memory_order_seq_cst);
    if (load_sig & LISTEN_ERROR) {
      assert(!(load_sig & CONNECTED));
      assert(!(load_sig & CONNECTION_CLOSED));
      assert(!(load_sig & CONNECT_ERROR));
      assert(buffer.empty());
      return 0;
    }
    {
      std::unique_lock<std::mutex> u_lock(lock);
      cond.wait_for(u_lock, std::chrono::milliseconds(10));
    }
  }
}
