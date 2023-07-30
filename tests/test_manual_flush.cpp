#undef NDEBUG

#include "test_utils.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

class FinalReceiver : public MsgReceiver {
public:
  FinalReceiver(const std::shared_ptr<bool> &hasReceived,
                const std::shared_ptr<std::mutex> &mutex,
                const std::shared_ptr<std::condition_variable> &cond)
      : has_received(hasReceived), mutex(mutex), cond(cond) {}

private:
  void on_message(std::string_view data) override {
    assert(data == "hello world");
    std::unique_lock<std::mutex> lock(*mutex);
    *has_received = true;
    cond->notify_one();
    std::cout << "final received" << std::endl;
  }

  std::shared_ptr<bool> has_received;
  std::shared_ptr<std::mutex> mutex;
  std::shared_ptr<std::condition_variable> cond;
};

class EchoReceiver : public MsgReceiver {
public:
  EchoReceiver(const std::shared_ptr<bool> &hasReceived,
               const std::shared_ptr<std::string> &data,
               const std::shared_ptr<std::mutex> &mutex,
               const std::shared_ptr<std::condition_variable> &cond)
      : has_received(hasReceived), _data(data), mutex(mutex), cond(cond) {}

private:
  void on_message(std::string_view data) override {
    std::unique_lock<std::mutex> lock(*mutex);
    *has_received = true;
    _data->append(data);
    cond->notify_one();
    std::cout << "echo received" << std::endl;
  }

  std::shared_ptr<bool> has_received;
  std::shared_ptr<std::string> _data;
  std::shared_ptr<std::mutex> mutex;
  std::shared_ptr<std::condition_variable> cond;
};

class HelloCallback : public ConnCallback {
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn,
                  std::shared_ptr<MsgSender> sender) override {
    auto rcv = std::make_unique<FinalReceiver>(has_received, mutex, cond);
    // disable write auto flush
    conn->start(std::move(rcv), nullptr, DEFAULT_MSG_BUF_SIZE,
                DEFAULT_READ_MSG_FLUSH_MILLI_SEC, 0);
    std::thread sender_t([sender] {
      sender->send_block("hello world");
      sender->flush();
    });
    sender_t.detach();
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
  HelloCallback()
      : has_received(std::make_shared<bool>(false)),
        mutex(std::make_shared<std::mutex>()),
        cond(std::make_shared<std::condition_variable>()) {}
  std::shared_ptr<MsgSender> _sender;
  std::shared_ptr<bool> has_received;
  std::shared_ptr<std::mutex> mutex;
  std::shared_ptr<std::condition_variable> cond;
};

class EchoCallback : public ConnCallback {
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn,
                  std::shared_ptr<MsgSender> sender) override {
    auto rcv = std::make_unique<EchoReceiver>(has_received, _data, mutex, cond);
    // disable write auto flush
    conn->start(std::move(rcv), nullptr, DEFAULT_MSG_BUF_SIZE, 1, 0);
    std::thread sender_t([sender, this]() {
      std::unique_lock<std::mutex> lock(*mutex);
      cond->wait(lock, [this]() { return has_received; });
      sender->send_block(*_data);
      std::cout << "echo received and sent back" << std::endl;
    });
    sender_t.detach();
  }

  void on_connection_close(const std::string &local_addr,
                           const std::string &peer_addr) override {}

  void on_listen_error(const std::string &addr,
                       const std::string &err) override {}

  void on_connect_error(const std::string &addr,
                        const std::string &err) override {}

public:
  EchoCallback()
      : has_received(std::make_shared<bool>(false)),
        _data(std::make_shared<std::string>()),
        mutex(std::make_shared<std::mutex>()),
        cond(std::make_shared<std::condition_variable>()) {}

  std::shared_ptr<bool> has_received;
  std::shared_ptr<std::string> _data;
  std::shared_ptr<std::mutex> mutex;
  std::shared_ptr<std::condition_variable> cond;
};

int test_manual_flush(int argc, char **argv) {

  const std::string addr = "127.0.0.1:40201";

  auto hello_cb = std::make_shared<HelloCallback>();
  auto echo_cb = std::make_shared<EchoCallback>();

  SocketManager hello(hello_cb);
  SocketManager echo(echo_cb);

  hello.listen_on_addr(addr);
  // Wait for 100ms
  std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_MILLIS));
  // create client
  echo.connect_to_addr(addr);

  std::cout << "Client connect" << std::endl;

  // wait for message
  {
    std::unique_lock<std::mutex> lock(*hello_cb->mutex);
    hello_cb->cond->wait(lock,
                         [&hello_cb]() { return hello_cb->has_received; });
  }

  return 0;
}
