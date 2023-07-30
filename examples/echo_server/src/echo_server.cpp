#include <iostream>
#include <memory>
#include <socket_manager.h>
#include <thread>
#include <unordered_map>
#include <variant>

/**
 * UniqueWaker is a wrapper of `socket_manager::Waker`
 * that also implements `socket_manager::Notifier`.
 */
class WrapWaker : public socket_manager::Notifier {
public:
  explicit WrapWaker(socket_manager::Waker &&wake) : waker(std::move(wake)) {}

  void set_waker(socket_manager::Waker &&wake) { waker = std::move(wake); }

private:
  void wake() override { waker.wake(); }

  socket_manager::Waker waker;
};

/**
 * When the receiver receives,
 * it tries to send back the message,
 * unless the sender returns `PENDING`,
 * it sleeps until the sender wakes it up.
 */
class EchoReceiver : public socket_manager::MsgReceiverAsync {
public:
  explicit EchoReceiver(std::shared_ptr<socket_manager::MsgSender> &&sender,
                        const std::shared_ptr<WrapWaker> &waker)
      : waker(waker), sender(std::move(sender)){};

  /**
   * Release resources to break potential ref cycles.
   */
  void close() {
    waker.reset();
    sender.reset();
  }

private:
  auto on_message_async(std::string_view data, socket_manager::Waker &&wake)
      -> long override {
    waker->set_waker(std::move(wake));
    return sender->send_async(data);
  };
  std::shared_ptr<WrapWaker> waker;
  std::shared_ptr<socket_manager::MsgSender> sender;
};

/**
 * The callbacks for connection events.
 */
class EchoCallback : public socket_manager::ConnCallback {
private:
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<socket_manager::Connection> conn,
                  std::shared_ptr<socket_manager::MsgSender> sender) override {
    auto waker = std::make_shared<WrapWaker>(socket_manager::Waker());
    auto recv = std::make_shared<EchoReceiver>(std::move(sender), waker);
    {
      // add the receiver to the map for cleanup
      std::lock_guard<std::mutex> lock(mutex);
      receivers[local_addr + peer_addr] = recv;
    }
    conn->start(std::move(recv), std::move(waker));
  }

  void on_connection_close(const std::string &local_addr,
                           const std::string &peer_addr) override {
    {
      std::lock_guard<std::mutex> lock(mutex);
      auto find = receivers.find(local_addr + peer_addr);
      if (find != receivers.end()) {
        // release receiver resources
        find->second->close();
        receivers.erase(find);
      } else {
        throw std::runtime_error("connection not found: " + local_addr +
                                 " -> " + peer_addr);
      }
    }
    std::cout << "connection closed: " << local_addr << " -> " << peer_addr
              << std::endl;
  }

  void on_listen_error(const std::string &addr,
                       const std::string &err) override {
    throw std::runtime_error("listen error: addr=" + addr + ", " + err);
  }

  void on_connect_error(const std::string &addr,
                        const std::string &err) override {
    throw std::runtime_error("connect error: addr=" + addr + ", " + err);
  }

  std::mutex mutex;
  std::unordered_map<std::string, std::shared_ptr<EchoReceiver>> receivers;
};

auto main() -> int {
  // start the server
  auto callback = std::make_shared<EchoCallback>();
  auto manager = socket_manager::SocketManager(callback);
  manager.listen_on_addr("127.0.0.1:10101");
  manager.join();
}
