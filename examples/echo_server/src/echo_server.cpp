#include <thread>
#include <iostream>
#include <socket_manager.h>
#include <variant>
#include <memory>

/**
 * Let the sender directly wake the receiver.
 */
class RcvSendWaker : public socket_manager::Notifier {
public:
  explicit RcvSendWaker(socket_manager::Waker &&wake) : waker(std::move(wake)) {}

  void set_waker(socket_manager::Waker &&wake) {
    waker = std::move(wake);
  }

private:
  void wake() override {
    waker.wake();
  }

  socket_manager::Waker waker;
};

/**
 * When the receiver receives,
 * it tries to send back the message,
 * unless the sender returns `PENDING`,
 * it sleeps until the sender wakes it up.
 */
class EchoReceiver :
        public socket_manager::MsgReceiverAsync,
        public std::enable_shared_from_this<EchoReceiver> {
public:
  explicit EchoReceiver(
          std::shared_ptr<socket_manager::MsgSender> &&sender,
          const std::shared_ptr<RcvSendWaker> &waker
  ) : waker(waker), sender(std::move(sender)) {};

private:
  long on_message_async(std::string_view data, socket_manager::Waker &&wake) override {
    waker->set_waker(std::move(wake));
    return sender->send_async(data);
  };
  std::shared_ptr<RcvSendWaker> waker;
  std::shared_ptr<socket_manager::MsgSender> sender;
};

/**
 * The callbacks for connection events.
 */
class EchoCallback : public socket_manager::ConnCallback {
private:
  void on_connect(const std::string &_local_addr, const std::string &_peer_addr,
                  std::shared_ptr<socket_manager::Connection> conn,
                  std::shared_ptr<socket_manager::MsgSender> sender) override {
    auto waker = std::make_shared<RcvSendWaker>(socket_manager::Waker());
    auto recv = std::make_shared<EchoReceiver>(std::move(sender), waker);
    conn->start(std::move(recv), std::move(waker));
  }

  void on_connection_close(const std::string &local_addr, const std::string &peer_addr) override {
    std::cout << "connection closed: " << local_addr << " -> " << peer_addr << std::endl;
  }

  void on_listen_error(const std::string &addr, const std::string &err) override {
    throw std::runtime_error("listen error: addr=" + addr + ", " + err);
  }

  void on_connect_error(const std::string &addr, const std::string &err) override {
    throw std::runtime_error("connect error: addr=" + addr + ", " + err);
  }
};

int main() {
  // start the server
  auto callback = std::make_shared<EchoCallback>();
  auto manager = socket_manager::SocketManager(callback);
  manager.listen_on_addr("127.0.0.1:10101");
  manager.join();
}
