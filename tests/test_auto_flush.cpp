#include "test_utils.h"
#include <chrono>
#include <thread>

class ReceiverHelloWorld : public DoNothingReceiver {
public:
  ReceiverHelloWorld(std::mutex &mutex,
                     std::condition_variable &cond,
                     std::atomic_bool &received)
          : mutex(mutex), cond(cond), received(received) {}

  void on_message(const std::shared_ptr<std::string> &data) override {
    if (*data == "hello world") {
      received.store(true);
      std::unique_lock<std::mutex> u_lock(mutex);
      cond.notify_all();
    }
  }

  std::mutex &mutex;
  std::condition_variable &cond;
  std::atomic_bool &received;
};

class HelloWorldManager : public DoNothingConnCallback {
public:
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  const std::shared_ptr<Connection> &conn) override {
    auto do_nothing = std::make_unique<ReceiverHelloWorld>(mutex, cond, received);
    sender = conn->start(std::move(do_nothing));
    sender->send("hello world");
  }

  std::mutex mutex;
  std::condition_variable cond;
  std::atomic_bool received{false};

private:
  std::shared_ptr<MsgSender> sender;
};

class SendHelloWorldDoNotClose : public DoNothingConnCallback {
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  const std::shared_ptr<Connection> &conn) override {
    auto do_nothing = std::make_unique<DoNothingReceiver>();
    sender = conn->start(std::move(do_nothing));
    sender->send("hello world");
  }

private:
  // store sender, do not close connection
  std::shared_ptr<MsgSender> sender;
};

int test_auto_flush(int argc, char **argv) {
  const std::string addr = "127.0.0.1:40101";

  auto send_cb = std::make_shared<SendHelloWorldDoNotClose>();
  SocketManager send(send_cb);
  send.listen_on_addr(addr);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  auto recv_cb = std::make_shared<HelloWorldManager>();
  SocketManager recv(recv_cb);
  recv.connect_to_addr(addr);

  while (true) {
    std::unique_lock<std::mutex> u_lock(recv_cb->mutex);
    if (recv_cb->received.load()) {
      break;
    }
    recv_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
  }

  return 0;
}
