#include "test_utils.h"
#include <chrono>
#include <thread>

class TwiceStartCallback : public DoNothingConnCallback {

public:
  TwiceStartCallback() : error_thrown(0) {};

  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  const std::shared_ptr<Connection> &conn) override {
    auto receiver = std::make_unique<DoNothingMsgReceiver>();
    auto receiver2 = std::make_unique<DoNothingMsgReceiver>();
    conn->start(std::move(receiver));
    try {
      conn->start(std::move(receiver2));
    } catch (std::runtime_error &e) {
      std::cout << "Error thrown: " << e.what() << std::endl;
      error_thrown.fetch_add(1, std::memory_order_release);
    }
    try {
      conn->close();
    } catch (std::runtime_error &e) {
      std::cout << "Error thrown: " << e.what() << std::endl;
      error_thrown.fetch_add(1, std::memory_order_release);
    }
    // notify the main thread
    std::unique_lock<std::mutex> lock(mutex);
    cond.notify_all();
  }

  std::atomic_int error_thrown;
  std::mutex mutex;
  std::condition_variable cond;

private:

  class DoNothingMsgReceiver : public MsgReceiver {
  public:
    void on_message(const std::shared_ptr<std::string> &data) override {}
  };

};

int test_error_twice_start(int argc, char **argv) {
  const std::string addr = "127.0.0.1:12348";

  auto bad_cb = std::make_shared<TwiceStartCallback>();
  auto good_cb = std::make_shared<StoreAllEventsConnCallback>();

  SocketManager bad(bad_cb);
  SocketManager good(good_cb);

  bad.listen_on_addr(addr);
  // wait 100ms
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  good.connect_to_addr(addr);

  // wait for error
  while (true) {
    std::unique_lock<std::mutex> lock(bad_cb->mutex);
    if (bad_cb->error_thrown.load(std::memory_order_acquire) == 2) {
      break;
    }
    bad_cb->cond.wait(lock);
  }

  return 0;
}
