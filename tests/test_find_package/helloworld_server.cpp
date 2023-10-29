#include <iostream>
#include <mutex>
#include <socket_manager.h>
#include <unordered_map>

class HelloWorldReceiver : public socket_manager::MsgReceiver {

public:
  HelloWorldReceiver(
      std::string conn_id, std::mutex &mutex,
      std::unordered_map<std::string,
                         std::shared_ptr<socket_manager::MsgSender>> &senders)
      : conn_id(std::move(conn_id)), mutex(mutex), senders(senders) {}

  void on_message(std::string_view data) override {
    try {
      std::unique_lock<std::mutex> my_lock(mutex);
      auto sender = senders.at(conn_id);
      sender->send_block("HTTP/1.1 200 OK\r\nContent-Length: 12\r\nConnection: "
                         "close\r\n\r\nHello, world");
      senders.erase(conn_id);
    } catch (const std::out_of_range &e) {
      std::cerr << "Exception at " << e.what() << std::endl;
    }
  }

private:
  std::string conn_id;
  std::mutex &mutex;
  std::unordered_map<std::string, std::shared_ptr<socket_manager::MsgSender>>
      &senders;
};

class MyCallback : public socket_manager::ConnCallback {
public:
  void on_connect(std::shared_ptr<socket_manager::Connection> conn,
                  std::shared_ptr<socket_manager::MsgSender> sender) override {
    auto id = conn->local_address() + "->" + conn->peer_address();
    conn->start(std::make_unique<HelloWorldReceiver>(id, mutex, senders));
    {
      std::unique_lock<std::mutex> my_lock(mutex);
      senders[id] = sender;
    }
  }

  void on_connection_close(const std::string &local_addr,
                           const std::string &peer_addr) override {
    auto id = local_addr + "->" + peer_addr;
    {
      std::unique_lock<std::mutex> my_lock(mutex);
      senders.erase(id);
    }
  }

  void on_listen_error(const std::string &addr,
                       const std::string &err) override {
    std::cout << "error listening to=(" << addr << "), err=(" << err << ")\n";
  }

  void on_connect_error(const std::string &addr,
                        const std::string &err) override {
    std::cout << "error connecting to=(" << addr << "), err=(" << err << ")\n";
  }

private:
  std::mutex mutex;
  std::unordered_map<std::string, std::shared_ptr<socket_manager::MsgSender>>
      senders;
};

int main() {
  auto manager_cb = std::make_shared<MyCallback>();
  socket_manager::SocketManager manager(manager_cb);
  manager.listen_on_addr("127.0.0.1:49999");
  manager.join();
}
