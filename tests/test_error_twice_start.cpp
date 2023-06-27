#include "test_utils.h"
#include <chrono>
#include <thread>

class TwiceStartSocketManager : public DoNothingSocketManager {

public:
  TwiceStartSocketManager() : error_thrown(false) {};

  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn) override {
    auto receiver = std::make_unique<DoNothingMsgReceiver>();
    conn->start(std::move(receiver));
    try {
      conn->start(std::move(receiver));
    } catch (std::runtime_error &e) {
      std::cout << "Error thrown: " << e.what() << std::endl;
      error_thrown.store(true, std::memory_order_release);
    }
    // notify the main thread
    std::unique_lock<std::mutex> lock(mutex);
    cond.notify_all();
  }

  std::atomic_bool error_thrown;
  std::mutex mutex;
  std::condition_variable cond;

private:

  class DoNothingMsgReceiver : public MsgReceiver {
  public:
    void on_message(std::shared_ptr<std::string> data) override {}
  };

};

int test_error_twice_start(int argc, char **argv) {
  const std::string addr = "127.0.0.1:8080";

  TwiceStartSocketManager bad;
  StoreAllEventsSocketManager good;

  bad.listen_on_addr(addr);
  // wait 100ms
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  good.connect_to_addr(addr);

  // wait for error
  while (true) {
    std::unique_lock<std::mutex> lock(bad.mutex);
    if (bad.error_thrown.load(std::memory_order_acquire)) {
      break;
    }
    bad.cond.wait(lock);
  }
  return 0;
}
