#undef NDEBUG
#ifndef SOCKET_MANAGER_TEST_UTILS_H
#define SOCKET_MANAGER_TEST_UTILS_H

#include "spdlog/spdlog.h"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <socket_manager/socket_manager.h>
#include <utility>
#include <vector>

class SpdLogger {
public:
  static void
  init(spdlog::level::level_enum level = spdlog::level::level_enum::debug) {
    spdlog::set_level(level);
    SOCKET_MANAGER_C_API_TraceLevel log_level =
        SOCKET_MANAGER_C_API_TraceLevel::Off;
    // Socket Manager level is the same as spdlog level from trace to err
    if (level <= spdlog::level::err) {
      log_level = static_cast<SOCKET_MANAGER_C_API_TraceLevel>(level);
    }
    socket_manager::init_logger(print_log, log_level,
                                SOCKET_MANAGER_C_API_TraceLevel::Off);
  };

private:
  static void print_log(SOCKET_MANAGER_C_API_LogData log_data) {
    socket_manager::LogData data = socket_manager::from_c_log_data(log_data);
    spdlog::log(static_cast<spdlog::level::level_enum>(data.level), "{}: {} {}",
                data.target, data.file, data.message);
  }
};

using namespace socket_manager;
const long long WAIT_MILLIS = 10;

/// Flag Signal

enum EventType {
  CONNECTED = 1 << 0,
  CONNECTION_CLOSED = 1 << 1,
  LISTEN_ERROR = 1 << 2,
  CONNECT_ERROR = 1 << 3,
  SEND_ERROR = 1 << 4
};

///
/// Message Receivers
///

class DoNothingReceiver : public MsgReceiver {
  void on_message(std::string_view data) override {}
};

class MsgStoreReceiver : public MsgReceiver {
public:
  MsgStoreReceiver(
      std::string conn_id, const std::shared_ptr<std::mutex> &mutex,
      const std::shared_ptr<std::condition_variable> &cond,
      const std::shared_ptr<
          std::vector<std::tuple<std::string, std::shared_ptr<std::string>>>>
          &buffer)
      : conn_id(std::move(conn_id)), mutex(mutex), cond(cond), buffer(buffer) {}

  void on_message(std::string_view data) override {
    std::unique_lock<std::mutex> lock(*mutex);
    buffer->emplace_back(conn_id, std::make_shared<std::string>(data));
    cond->notify_all();
  }

private:
  std::string conn_id;
  std::shared_ptr<std::mutex> mutex;
  std::shared_ptr<std::condition_variable> cond;
  std::shared_ptr<
      std::vector<std::tuple<std::string, std::shared_ptr<std::string>>>>
      buffer;
};

///
/// Connection Callbacks
///

/// Do Nothing
class DoNothingConnCallback : public ConnCallback {
public:
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn,
                  std::shared_ptr<MsgSender> sender) override {
    conn->close();
  }

  void on_connection_close(const std::string &local_addr,
                           const std::string &peer_addr) override {}

  void on_listen_error(const std::string &addr,
                       const std::string &err) override {}

  void on_connect_error(const std::string &addr,
                        const std::string &err) override {}
};

class BitFlagCallback : public ConnCallback {
public:
  BitFlagCallback(
      const std::shared_ptr<std::mutex> &mutex,
      const std::shared_ptr<std::condition_variable> &cond,
      const std::shared_ptr<std::atomic_int> &sig,
      const std::shared_ptr<
          std::vector<std::tuple<std::string, std::shared_ptr<std::string>>>>
          &buffer)
      : mutex(mutex), cond(cond), sig(sig), buffer(buffer) {}

  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn,
                  std::shared_ptr<MsgSender> sender) override {
    set_sig(CONNECTED);
    auto conn_id = local_addr + "->" + peer_addr;
    auto msg_storer =
        std::make_unique<MsgStoreReceiver>(conn_id, mutex, cond, buffer);
    conn->start(std::move(msg_storer));
  }

  void on_connection_close(const std::string &local_addr,
                           const std::string &peer_addr) override {
    set_sig(CONNECTION_CLOSED);
  }

  void on_listen_error(const std::string &addr,
                       const std::string &err) override {
    set_sig(LISTEN_ERROR);
  }

  void on_connect_error(const std::string &addr,
                        const std::string &err) override {
    set_sig(CONNECT_ERROR);
  }

protected:
  void set_sig(int flag) {
    std::lock_guard<std::mutex> lock(*mutex);
    sig->fetch_or(flag, std::memory_order_seq_cst);
    cond->notify_all();
  }

private:
  std::shared_ptr<std::mutex> mutex;
  std::shared_ptr<std::condition_variable> cond;
  std::shared_ptr<std::atomic_int> sig;
  std::shared_ptr<
      std::vector<std::tuple<std::string, std::shared_ptr<std::string>>>>
      buffer;
};

/// Store all events in order

class StoreAllEventsConnCallback : public ConnCallback {

public:
  explicit StoreAllEventsConnCallback(bool clean_sender_on_close = true)
      : mutex(std::make_shared<std::mutex>()),
        connected_count(std::make_shared<std::atomic_int>()),
        cond(std::make_shared<std::condition_variable>()),
        events(std::make_shared<
               std::vector<std::tuple<EventType, std::string>>>()),
        buffer(std::make_shared<std::vector<
                   std::tuple<std::string, std::shared_ptr<std::string>>>>()),
        clean_sender_on_close(clean_sender_on_close) {}

  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<Connection> conn,
                  std::shared_ptr<MsgSender> sender) override {
    std::unique_lock<std::mutex> lock(*mutex);
    auto conn_id = local_addr + "->" + peer_addr;
    events->emplace_back(CONNECTED, conn_id);
    auto msg_storer =
        std::make_unique<MsgStoreReceiver>(conn_id, mutex, cond, buffer);
    conn->start(std::move(msg_storer));
    senders.emplace(conn_id, sender);
    connected_count->fetch_add(1, std::memory_order_seq_cst);
    cond->notify_all();
  }

  void on_connection_close(const std::string &local_addr,
                           const std::string &peer_addr) override {
    std::unique_lock<std::mutex> lock(*mutex);
    auto conn_id = local_addr + "->" + peer_addr;
    events->emplace_back(CONNECTION_CLOSED, conn_id);
    if (clean_sender_on_close) {
      senders.erase(conn_id);
    }
    connected_count->fetch_sub(1, std::memory_order_seq_cst);
    cond->notify_all();
  }

  void on_listen_error(const std::string &addr,
                       const std::string &err) override {
    std::unique_lock<std::mutex> lock(*mutex);
    events->emplace_back(LISTEN_ERROR,
                         "listening to " + addr + " failed: " + err);
    cond->notify_all();
  }

  void on_connect_error(const std::string &addr,
                        const std::string &err) override {
    std::unique_lock<std::mutex> lock(*mutex);
    events->emplace_back(CONNECT_ERROR,
                         "connecting to " + addr + " failed: " + err);
    cond->notify_all();
  }

  void send_to(std::string &conn_id, const std::string &data) {
    std::unique_lock<std::mutex> lock(*mutex);
    try {
      auto sender = senders.at(conn_id);
      sender->send_block(data);
      sender->flush();
    } catch (std::out_of_range &e) {
      std::cout << "connection " << conn_id << " not found during send"
                << std::endl;
    }
  }

  void drop_connection(std::string &conn_id) {
    std::unique_lock<std::mutex> lock(*mutex);
    senders.erase(conn_id);
  }

  std::shared_ptr<std::mutex> mutex;
  std::shared_ptr<std::atomic_int> connected_count;
  std::shared_ptr<std::condition_variable> cond;
  std::shared_ptr<std::vector<std::tuple<EventType, std::string>>> events;
  std::shared_ptr<
      std::vector<std::tuple<std::string, std::shared_ptr<std::string>>>>
      buffer;
  std::unordered_map<std::string, std::shared_ptr<MsgSender>> senders;
  bool clean_sender_on_close;
};

#endif // SOCKET_MANAGER_TEST_UTILS_H
