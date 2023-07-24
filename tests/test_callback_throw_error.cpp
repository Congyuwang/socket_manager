#undef NDEBUG

#include "test_utils.h"
#include <stdexcept>
#include <chrono>
#include <thread>

using namespace socket_manager;

class OnConnectErrorBeforeStartCallback : public DoNothingConnCallback {
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn, std::shared_ptr<MsgSender> sender) override {
    throw std::runtime_error("throw some error before calling start");
  }
};

class OnConnectErrorAfterStartCallback : public DoNothingConnCallback {
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn, std::shared_ptr<MsgSender> sender) override {
    conn->start(std::make_unique<DoNothingReceiver>());
    throw std::runtime_error("throw some error after calling start");
  }
};

class OnMsgErrorReceiver : public MsgReceiver {
  long on_message(std::string_view data, std::shared_ptr<RcvWaker> waker) override {
    throw std::runtime_error("throw some error on receiving message");
  }
};

class OnMsgErrorCallback : public ConnCallback {
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn, std::shared_ptr<MsgSender> send) override {
    conn->start(std::make_unique<OnMsgErrorReceiver>());
    this->sender = send;
    sender.use_count();
  }

  void on_connection_close(const std::string &local_addr, const std::string &peer_addr) override {}

  void on_listen_error(const std::string &addr, const std::string &err) override {}

  void on_connect_error(const std::string &addr, const std::string &err) override {}

private:
  std::shared_ptr<MsgSender> sender;
};

class StoreAllEventsConnHelloCallback : public StoreAllEventsConnCallback {
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn, std::shared_ptr<MsgSender> sender) override {
    std::unique_lock<std::mutex> lock(mutex);
    auto conn_id = local_addr + "->" + peer_addr;
    events.emplace_back(CONNECTED, conn_id);
    auto msg_storer = std::make_unique<MsgStoreReceiver>(conn_id, mutex, cond, buffer);
    conn->start(std::move(msg_storer));
    std::thread t1([sender]() {
      sender->send("hello");
    });
    t1.detach();
    senders.emplace(conn_id, sender);
    connected_count.fetch_add(1, std::memory_order_seq_cst);
    cond.notify_all();
  }
};

int test_callback_throw_error(int argc, char **argv) {
  const std::string addr = "127.0.0.1:40102";

  auto err_before_cb = std::make_shared<OnConnectErrorBeforeStartCallback>();
  auto err_after_cb = std::make_shared<OnConnectErrorAfterStartCallback>();
  auto err_on_msg_cb = std::make_shared<OnMsgErrorCallback>();
  auto store_record_cb = std::make_shared<StoreAllEventsConnHelloCallback>();

  SocketManager err_before(err_before_cb);
  SocketManager err_after(err_after_cb);
  SocketManager err_on_msg(err_on_msg_cb);
  SocketManager store_record(store_record_cb);

  store_record.listen_on_addr(addr);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  err_before.connect_to_addr(addr);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  err_after.connect_to_addr(addr);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  err_on_msg.connect_to_addr(addr);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  while (true) {
    std::unique_lock<std::mutex> u_lock(store_record_cb->mutex);
    if (store_record_cb->events.size() == 6) {
      assert(std::get<0>(store_record_cb->events[0]) == CONNECTED);
      assert(std::get<0>(store_record_cb->events[1]) == CONNECTION_CLOSED);
      assert(std::get<0>(store_record_cb->events[2]) == CONNECTED);
      assert(std::get<0>(store_record_cb->events[3]) == CONNECTION_CLOSED);
      assert(std::get<0>(store_record_cb->events[4]) == CONNECTED);
      assert(std::get<0>(store_record_cb->events[5]) == CONNECTION_CLOSED);
      break;
    }
    store_record_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
  }

  return 0;
}
