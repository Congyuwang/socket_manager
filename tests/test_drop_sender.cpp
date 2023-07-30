#undef NDEBUG
#include "test_utils.h"
#include <chrono>
#include <thread>

// this test is to test that dropping sender object closes remote connections

int test_drop_sender(int argc, char **argv) {
  const std::string local_addr = "127.0.0.1:40100";

  auto lock = std::make_shared<std::mutex>();
  auto cond = std::make_shared<std::condition_variable>();
  auto sig = std::make_shared<std::atomic_int>(0);
  auto buffer = std::make_shared<
      std::vector<std::tuple<std::string, std::shared_ptr<std::string>>>>();

  auto server_cb = std::make_shared<StoreAllEventsConnCallback>();
  // bit flag socket drop sender directly, which should close the connection.
  auto test_cb = std::make_unique<BitFlagCallback>(lock, cond, sig, buffer);

  SocketManager server(server_cb);
  SocketManager test(std::move(test_cb));

  server.listen_on_addr(local_addr);
  // wait 10ms for server to start listening
  std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_MILLIS));
  test.connect_to_addr(local_addr);

  // Wait for the connection to close
  while (true) {
    std::unique_lock<std::mutex> u_lock(*server_cb->mutex);
    if (server_cb->events->size() == 2) {
      assert(std::get<0>(server_cb->events->at(0)) == CONNECTED);
      assert(std::get<0>(server_cb->events->at(1)) == CONNECTION_CLOSED);
      break;
    }
    server_cb->cond->wait_for(u_lock, std::chrono::milliseconds(WAIT_MILLIS));
  }

  while (true) {
    int load_sig = sig->load(std::memory_order_seq_cst);
    if (0 != (load_sig & CONNECTION_CLOSED)) {
      assert(load_sig & CONNECTED);
      assert(!(load_sig & CONNECT_ERROR));
      assert(!(load_sig & LISTEN_ERROR));
      assert(buffer->empty());
      return 0;
    }
    {
      std::unique_lock<std::mutex> u_lock(*lock);
      cond->wait_for(u_lock, std::chrono::milliseconds(WAIT_MILLIS));
    }
  }
}
