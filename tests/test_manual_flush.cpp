#undef NDEBUG

#include "test_utils.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

class FinalReceiver : public MsgReceiver {
public:
  FinalReceiver(bool &hasReceived, std::mutex &mutex,
                std::condition_variable &cond)
      : has_received(hasReceived), mutex(mutex), cond(cond) {}

private:
  void on_message(std::string_view data) override {
    assert(data == "hello world");
    std::unique_lock<std::mutex> lk(mutex);
    has_received = true;
    cond.notify_one();
    std::cout << "final received" << std::endl;
  }

  bool &has_received;
  std::mutex &mutex;
  std::condition_variable &cond;
};

class EchoReceiver : public MsgReceiver {
public:
  EchoReceiver(bool &hasReceived, std::string &data, std::mutex &mutex,
               std::condition_variable &cond)
      : has_received(hasReceived), _data(data), mutex(mutex), cond(cond) {}

private:
  void on_message(std::string_view data) override {
    std::unique_lock<std::mutex> lk(mutex);
    has_received = true;
    _data.append(data);
    cond.notify_one();
    std::cout << "echo received" << std::endl;
  }

  bool &has_received;
  std::string &_data;
  std::mutex &mutex;
  std::condition_variable &cond;
};

class HelloCallback : public ConnCallback {
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn,
                  std::shared_ptr<MsgSender> sender) override {
    auto rcv = std::make_unique<FinalReceiver>(has_received, mutex, cond);
    // disable write auto flush
    conn->start(std::move(rcv), nullptr, DEFAULT_MSG_BUF_SIZE, 1, 0);
    std::thread t([sender] {
      sender->send_block("hello world");
      sender->flush();
    });
    t.detach();
    _sender = sender;
    std::cout << "hello world sent" << std::endl;
  }

  void on_connection_close(const std::string &local_addr,
                           const std::string &peer_addr) override {}

  void on_listen_error(const std::string &addr,
                       const std::string &err) override {}

  void on_connect_error(const std::string &addr,
                        const std::string &err) override {}

public:
  std::shared_ptr<MsgSender> _sender;
  bool has_received{false};
  std::mutex mutex;
  std::condition_variable cond;
};

class EchoCallback : public ConnCallback {
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn,
                  std::shared_ptr<MsgSender> sender) override {
    auto rcv = std::make_unique<EchoReceiver>(has_received, _data, mutex, cond);
    // disable write auto flush
    conn->start(std::move(rcv), nullptr, DEFAULT_MSG_BUF_SIZE, 1, 0);
    std::thread t([sender, this]() {
      std::unique_lock<std::mutex> lk(mutex);
      cond.wait(lk, [this]() { return has_received; });
      sender->send_block(_data);
      std::cout << "echo received and sent back" << std::endl;
    });
    t.detach();
  }

  void on_connection_close(const std::string &local_addr,
                           const std::string &peer_addr) override {}

  void on_listen_error(const std::string &addr,
                       const std::string &err) override {}

  void on_connect_error(const std::string &addr,
                        const std::string &err) override {}

  bool has_received{false};
  std::string _data;
  std::mutex mutex;
  std::condition_variable cond;
};

int test_manual_flush(int argc, char **argv) {

  const std::string addr = "127.0.0.1:40201";

  auto hello_cb = std::make_shared<HelloCallback>();
  auto echo_cb = std::make_shared<EchoCallback>();

  SocketManager hello(hello_cb);
  SocketManager echo(echo_cb);

  hello.listen_on_addr(addr);
  // Wait for 100ms
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  // create client
  echo.connect_to_addr(addr);

  std::cout << "Client connect" << std::endl;

  // wait for message
  {
    std::unique_lock<std::mutex> lk(hello_cb->mutex);
    hello_cb->cond.wait(lk, [&hello_cb]() { return hello_cb->has_received; });
  }

  return 0;
}
