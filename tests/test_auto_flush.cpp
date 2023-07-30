#undef NDEBUG

#include "test_utils.h"
#include <chrono>
#include <thread>

class ReceiverHelloWorld : public DoNothingReceiver {
public:
  ReceiverHelloWorld(const std::shared_ptr<std::mutex> &mutex,
                     const std::shared_ptr<std::condition_variable> &cond,
                     const std::shared_ptr<std::atomic_bool> &received)
      : mutex(mutex), cond(cond), received(received) {}

  void on_message(std::string_view data) override {
    if (data == "hello world") {
      received->store(true);
      std::unique_lock<std::mutex> u_lock(*mutex);
      cond->notify_all();
    }
  }

  std::shared_ptr<std::mutex> mutex;
  std::shared_ptr<std::condition_variable> cond;
  std::shared_ptr<std::atomic_bool> received;
};

class HelloWorldManager : public DoNothingConnCallback {
public:
  HelloWorldManager()
      : mutex(std::make_shared<std::mutex>()),
        cond(std::make_shared<std::condition_variable>()),
        received(std::make_shared<std::atomic_bool>(false)) {}

  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn,
                  std::shared_ptr<MsgSender> send) override {
    auto do_nothing =
        std::make_unique<ReceiverHelloWorld>(mutex, cond, received);
    conn->start(std::move(do_nothing));
    this->sender = send;
    sender->send_block("hello world");
  }

  std::shared_ptr<std::mutex> mutex;
  std::shared_ptr<std::condition_variable> cond;
  std::shared_ptr<std::atomic_bool> received;
  std::shared_ptr<MsgSender> sender;
};

class SendHelloWorldDoNotClose : public DoNothingConnCallback {
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn,
                  std::shared_ptr<MsgSender> sender) override {
    auto do_nothing = std::make_unique<DoNothingReceiver>();
    conn->start(std::move(do_nothing));
    this->sender = sender;
    std::thread sender_t([this] { this->sender->send_block("hello world"); });
    sender_t.detach();
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

  std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_MILLIS));

  auto recv_cb = std::make_shared<HelloWorldManager>();
  SocketManager recv(recv_cb);
  recv.connect_to_addr(addr);

  while (true) {
    std::unique_lock<std::mutex> u_lock(*recv_cb->mutex);
    if (recv_cb->received->load()) {
      break;
    }
    recv_cb->cond->wait_for(u_lock, std::chrono::milliseconds(WAIT_MILLIS));
  }

  return 0;
}
