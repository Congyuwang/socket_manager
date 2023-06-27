#include <socket_manager/socket_manager.h>
#include "test_utils.h"

int test_error_connect(int argc, char **argv) {
  std::mutex lock;
  std::condition_variable cond;
  std::atomic_int sig(0);
  std::vector<std::tuple<std::string, std::shared_ptr<std::string>>> buffer;

  BitFlagSocketManager test(lock, cond, sig, buffer);
  test.connect_to_addr("127.0.0.1:12345");
  test.detach();

  // Wait for the connection to fail
  while (true) {
    {
      std::unique_lock<std::mutex> u_lock(lock);
      cond.wait(u_lock);
    }
    int load_sig = sig.load(std::memory_order_seq_cst);
    if (load_sig & CONNECT_ERROR) {
      assert(!(load_sig & CONNECTED));
      assert(!(load_sig & CONNECTION_CLOSED));
      assert(!(load_sig & LISTEN_ERROR));
      assert(buffer.empty());
      return 0;
    }
  }
}