#include "test_utils.h"

class SendLargeDataConnCallback : public DoNothingConnCallback {
public:
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  const std::shared_ptr<Connection> &conn) override {
    auto rcv = std::make_unique<DoNothingReceiver>();
    auto sender = conn->start(std::move(rcv));
    for (int i = 0; i < 1024 * 1024; ++i) {
      sender->send("helloworld"
                   "helloworld"
                   "helloworld"
                   "helloworld"
                   "helloworld"
                   "helloworld"
                   "helloworld"
                   "helloworld"
                   "helloworld"
                   "helloworld");
    }
    // close connection after sender finished.
  }
};

class StoreAllDataNotifyOnCloseCallback : public DoNothingConnCallback {
public:

  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  const std::shared_ptr<Connection> &conn) override {
    auto rcv = std::make_unique<StoreAllData>(add_data);
    // store sender so connection is not dropped.
    sender = conn->start(std::move(rcv));
  }

  void on_connection_close(const std::string &local_addr, const std::string &peer_addr) override {
    std::unique_lock<std::mutex> lk(mutex);
    has_closed.store(true);
    std::cout << "on_connection_close" << std::endl;
    cond.notify_all();
  }

  std::mutex mutex;
  std::condition_variable cond;
  std::atomic_bool has_closed{false};
  std::string add_data;
  std::shared_ptr<MsgSender> sender;

private:
  class StoreAllData : public MsgReceiver {
  public:
    explicit StoreAllData(std::string &buffer) : buffer(buffer) {}

    void on_message(const std::shared_ptr<std::string> &data) override {
      if (count % 100 == 0) {
        std::cout << "received " << count << " messages "
                  << ",size = " << buffer.size() << std::endl;
      }
      buffer.append(*data);
      count += 1;
    }

    std::string &buffer;
    int count = 0;
  };
};

int test_transfer_data_large(int argc, char **argv) {
  const std::string addr = "127.0.0.1:12353";

  auto send_cb = std::make_shared<SendLargeDataConnCallback>();
  auto store_cb = std::make_shared<StoreAllDataNotifyOnCloseCallback>();
  SocketManager send(send_cb);
  SocketManager store(store_cb);

  send.listen_on_addr(addr);
  store.connect_to_addr(addr);

  // Wait for the connection to close
  while (true) {
    if (store_cb->has_closed.load()) {
      assert(store_cb->add_data.size() == 1024 * 1024 * 100);
      return 0;
    }
    {
      std::unique_lock<std::mutex> u_lock(store_cb->mutex);
      store_cb->cond.wait(u_lock);
    }
  }
}
