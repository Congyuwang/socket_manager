#include "test_utils.h"
#include <chrono>
#include <thread>

// this test is to test that dropping sender object closes remote connections

int test_drop_sender(int argc, char **argv) {
  const std::string local_addr = "127.0.0.1:12353";

  std::mutex lock;
  std::condition_variable cond;
  std::atomic_int sig(0);
  std::vector<std::tuple<std::string, std::shared_ptr<std::string>>> buffer;

  StoreAllEventsSocketManager server;
  // bit flag socket drop sender directly, which should close the connection.
  BitFlagSocketManager test(lock, cond, sig, buffer);

  server.listen_on_addr(local_addr);
  // wait 100ms for server to start listening
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  test.connect_to_addr(local_addr);

  // Wait for the connection to close
  while (true) {
    std::unique_lock<std::mutex> u_lock(server.mutex);
    if (server.events.size() == 2) {
      assert(std::get<0>(server.events[0]) == CONNECTED);
      assert(std::get<0>(server.events[1]) == CONNECTION_CLOSED);
      break;
    }
    server.cond.wait(u_lock);
  }

  while (true) {
    int load_sig = sig.load(std::memory_order_seq_cst);
    if (load_sig & CONNECTION_CLOSED) {
      assert(load_sig & CONNECTED);
      assert(!(load_sig & CONNECT_ERROR));
      assert(!(load_sig & LISTEN_ERROR));
      assert(buffer.empty());
      return 0;
    }
    {
      std::unique_lock<std::mutex> u_lock(lock);
      cond.wait(u_lock);
    }
  }
}
