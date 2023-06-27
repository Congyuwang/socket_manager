#ifndef SOCKET_MANAGER_TEST_UTILS_H
#define SOCKET_MANAGER_TEST_UTILS_H

#include <socket_manager/socket_manager.h>
#include <mutex>
#include <atomic>
#include <utility>
#include <iostream>
#include <vector>
#include <condition_variable>

using namespace socket_manager;

/// Do Nothing

class DoNothingSocketManager : public SocketManager {
public:
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn) override {}

  void on_connection_close(const std::string &local_addr, const std::string &peer_addr) override {}

  void on_listen_error(const std::string &addr, const std::string &err) override {}

  void on_connect_error(const std::string &addr, const std::string &err) override {}
};

/// Flag Signal

enum EventType {
  CONNECTED = 1 << 0,
  CONNECTION_CLOSED = 1 << 1,
  LISTEN_ERROR = 1 << 2,
  CONNECT_ERROR = 1 << 3,
  SEND_ERROR = 1 << 4
};

class MsgStorerSocketManager : public MsgReceiver {
public:
  MsgStorerSocketManager(std::string conn_id,
                         std::mutex &mutex,
                         std::condition_variable &cond,
                         std::vector<std::tuple<std::string, std::shared_ptr<std::string>>> &buffer)
          : conn_id(std::move(conn_id)), mutex(mutex), cond(cond), buffer(buffer) {}

  void on_message(std::shared_ptr<std::string> data) override {
    std::unique_lock<std::mutex> lock(mutex);
    buffer.emplace_back(conn_id, data);
    cond.notify_all();
  }

private:
  std::string conn_id;
  std::mutex &mutex;
  std::condition_variable &cond;
  std::vector<std::tuple<std::string, std::shared_ptr<std::string>>> &buffer;
};

class BitFlagSocketManager : public SocketManager {
public:
  BitFlagSocketManager(std::mutex &mutex, std::condition_variable &cond, std::atomic_int &sig,
                       std::vector<std::tuple<std::string, std::shared_ptr<std::string>>> &buffer)
          : mutex(mutex), cond(cond), sig(sig), buffer(buffer) {}

  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn) override {
    set_sig(CONNECTED);
    auto conn_id = local_addr + "->" + peer_addr;
    auto msg_storer = std::make_unique<MsgStorerSocketManager>(conn_id, mutex, cond, buffer);
    conn->start(std::move(msg_storer));
  }

  void on_connection_close(const std::string &local_addr, const std::string &peer_addr) override {
    set_sig(CONNECTION_CLOSED);
  }

  void on_listen_error(const std::string &addr, const std::string &err) override {
    set_sig(LISTEN_ERROR);
  }

  void on_connect_error(const std::string &addr, const std::string &err) override {
    set_sig(CONNECT_ERROR);
  }

private:

  void set_sig(int flag) {
    std::lock_guard<std::mutex> lock(mutex);
    sig.fetch_or(flag, std::memory_order_seq_cst);
    cond.notify_all();
  }

  std::mutex &mutex;
  std::condition_variable &cond;
  std::atomic_int &sig;
  std::vector<std::tuple<std::string, std::shared_ptr<std::string>>> &buffer;
};

/// Store all events in order

class StoreAllEventsSocketManager : public SocketManager {

public:

  explicit StoreAllEventsSocketManager(bool clean_sender_on_close = true)
          : clean_sender_on_close(clean_sender_on_close), connected_count(0) {}

  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn) override {
    std::unique_lock<std::mutex> lock(mutex);
    auto conn_id = local_addr + "->" + peer_addr;
    events.emplace_back(CONNECTED, conn_id);
    auto msg_storer = std::make_unique<MsgStorerSocketManager>(conn_id, mutex, cond, buffer);
    auto sender = conn->start(std::move(msg_storer));
    senders.emplace(conn_id, sender);
    connected_count.fetch_add(1, std::memory_order_seq_cst);
    cond.notify_all();
  }

  void on_connection_close(const std::string &local_addr, const std::string &peer_addr) override {
    std::unique_lock<std::mutex> lock(mutex);
    auto conn_id = local_addr + "->" + peer_addr;
    events.emplace_back(CONNECTION_CLOSED, conn_id);
    if (clean_sender_on_close) {
      senders.erase(conn_id);
    }
    connected_count.fetch_sub(1, std::memory_order_seq_cst);
    cond.notify_all();
  }

  void on_listen_error(const std::string &addr, const std::string &err) override {
    std::unique_lock<std::mutex> lock(mutex);
    events.emplace_back(LISTEN_ERROR, "listening to " + addr + " failed: " + err);
    cond.notify_all();
  }

  void on_connect_error(const std::string &addr, const std::string &err) override {
    std::unique_lock<std::mutex> lock(mutex);
    events.emplace_back(CONNECT_ERROR, "connecting to " + addr + " failed: " + err);
    cond.notify_all();
  }

  void send_to(std::string &conn_id, const std::string &data) {
    std::unique_lock<std::mutex> lock(mutex);
    try {
      senders.at(conn_id)->send(data);
    } catch (std::out_of_range &e) {
      std::cout << "connection " << conn_id << " not found during send" << std::endl;
    }
  }

  void drop_connection(std::string &conn_id) {
    std::unique_lock<std::mutex> lock(mutex);
    senders.erase(conn_id);
  }

  std::mutex mutex;
  std::atomic_int connected_count;
  std::condition_variable cond;
  std::vector<std::tuple<EventType, std::string>> events;
  std::vector<std::tuple<std::string, std::shared_ptr<std::string>>> buffer;
  std::unordered_map<std::string, std::shared_ptr<MsgSender>> senders;
  bool clean_sender_on_close;

};

#endif //SOCKET_MANAGER_TEST_UTILS_H
