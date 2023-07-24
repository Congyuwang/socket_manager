#undef NDEBUG

#include "test_utils.h"
#include <string_view>
#include <concurrentqueue.h>
#include <lightweightsemaphore.h>
#include <thread>

const size_t MSG_BUF_SIZE = 256 * 1024;

class CondWaker : public Notifier {
public:
  explicit CondWaker(const std::shared_ptr<moodycamel::LightweightSemaphore> &sem) : sem(sem) {}

  void wake() override {
    sem->signal();
  }

private:
  std::shared_ptr<moodycamel::LightweightSemaphore> sem;
};

class SendLargeDataConnCallbackAsync : public DoNothingConnCallback {
public:
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn, std::shared_ptr<MsgSender> sender) override {
    auto rcv = std::make_unique<DoNothingReceiver>();
    conn->start(std::move(rcv));

    std::thread t([sender]() {
      // send 1000MB data
      int progress = 0;
      size_t offset = 0;
      auto sem = std::make_shared<moodycamel::LightweightSemaphore>();
      auto waker = std::make_shared<CondWaker>(sem);

      std::string data;
      data.reserve(1024 * 1000);
      for (int i = 0; i < 100 * 1024; i++) {
        data.append("helloworld");
      }
      std::string_view data_view(data);

      while (progress < 1024) {
        auto sent = sender->send_async(data_view.substr(offset), waker);
        if (sent < 0) {
          sem->wait();
        } else {
          offset += sent;
        }
        if (offset == data.size()) {
          offset = 0;
          progress += 1;
        }
      }
    });

    t.detach();
  }
};

class StoreAllDataAsync : public MsgReceiver {
public:
  explicit StoreAllDataAsync(size_t &buffer, int &count) : buffer(buffer), count(count) {}

  void on_message(std::string_view data) override {
    if (count % 100 == 0) {
      std::cout << "received " << count << " messages "
                << ",size = " << buffer << std::endl;
    }
    buffer += data.length();
    count += 1;
  }

  size_t &buffer;
  int &count;
};

class StoreAllDataNotifyOnCloseCallbackAsync : public ConnCallback {
public:

  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn, std::shared_ptr<MsgSender> send) override {
    auto rcv = std::make_unique<StoreAllDataAsync>(add_data, count);
    // store sender so connection is not dropped.
    this->sender = send;
    conn->start(std::move(rcv), MSG_BUF_SIZE);
  }

  void on_connection_close(const std::string &local_addr, const std::string &peer_addr) override {
    std::unique_lock<std::mutex> lk(mutex);
    has_closed.store(true);
    std::cout << "on_connection_close" << std::endl;
    cond.notify_all();
  }

  void on_listen_error(const std::string &addr, const std::string &err) override {}

  void on_connect_error(const std::string &addr, const std::string &err) override {}

  std::mutex mutex;
  std::condition_variable cond;
  std::atomic_bool has_closed{false};
  size_t add_data{0};
  int count{0};
  std::shared_ptr<MsgSender> sender;
};

int test_transfer_data_large_async(int argc, char **argv) {
  const std::string addr = "127.0.0.1:40013";

  auto send_cb = std::make_shared<SendLargeDataConnCallbackAsync>();
  auto store_cb = std::make_shared<StoreAllDataNotifyOnCloseCallbackAsync>();
  SocketManager send(send_cb);
  SocketManager store(store_cb);

  send.listen_on_addr(addr);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  store.connect_to_addr(addr);

  // Wait for the connection to close
  while (true) {
    if (store_cb->has_closed.load()) {
      auto avg_size = store_cb->add_data / store_cb->count;
      std::cout << "received " << store_cb->count << " messages ,"
                << "total size = " << store_cb->add_data << " bytes, "
                << "average size = " << avg_size << " bytes"
                << std::endl;
      assert(store_cb->add_data == 1024 * 1024 * 1000);
      return 0;
    }
    {
      std::unique_lock<std::mutex> u_lock(store_cb->mutex);
      store_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
    }
  }
}
