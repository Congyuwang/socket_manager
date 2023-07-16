#undef NDEBUG
#include "test_utils.h"
#include <chrono>
#include <thread>

const size_t MSG_BUF_SIZE = 4 * 1024 * 1024;

class SendLargeManualDataConnCallback : public DoNothingConnCallback {
public:
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  const std::shared_ptr<Connection> &conn) override {
    auto rcv = std::make_unique<DoNothingReceiver>();
    auto sender = conn->start(std::move(rcv));
    std::thread t([sender]() {
      // send 1000MB data
      std::string data;
      for (int i = 0; i < 1024 * 1024; i++) {
        data.append("helloworld");
      }
      for (int i = 0; i < 100; ++i) {
        sender->send(data);
      }
      // close connection after sender finished.
    });
    t.detach();
  }
};

class StoreAllData : public MsgReceiver {
public:
  explicit StoreAllData(int &buffer, int &count) : buffer(buffer), count(count) {}

  void on_message(const std::shared_ptr<std::string> &data) override {
    if (count % 100 == 0) {
      std::cout << "received " << count << " messages "
                << ",size = " << buffer << std::endl;
    }
    buffer += (int)data->size();
    count += 1;
  }

  int &buffer;
  int &count;
};

class StoreAllDataNotifyOnCloseCallback : public ConnCallback {
public:

  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  const std::shared_ptr<Connection> &conn) override {
    auto rcv = std::make_unique<StoreAllData>(add_data, count);
    // store sender so connection is not dropped.
    sender = conn->start(std::move(rcv), MSG_BUF_SIZE);
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
  int add_data{0};
  int count{0};
  std::shared_ptr<MsgSender> sender;
};

int test_transfer_data_large_manual_flush(int argc, char **argv) {
  const std::string addr = "127.0.0.1:40013";

  auto send_cb = std::make_shared<SendLargeManualDataConnCallback>();
  auto store_cb = std::make_shared<StoreAllDataNotifyOnCloseCallback>();
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