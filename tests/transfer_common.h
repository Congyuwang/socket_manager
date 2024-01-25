#include "connection.h"
#include <memory>
#undef NDEBUG

#ifndef SOCKET_MANAGER_TEST_TRANSFER_COMMON_H
#define SOCKET_MANAGER_TEST_TRANSFER_COMMON_H

#include "concurrentqueue/concurrentqueue.h"
#include "concurrentqueue/lightweightsemaphore.h"
#include "test_utils.h"
#include <socket_manager/common/notifier.h>
#include <socket_manager/msg_sender.h>
#include <thread>

const int PRINT_INTERVAL = 100;
const size_t TOTAL_SIZE = static_cast<size_t>(1024) * 1024 * 1000;
const size_t LARGE_MSG_SIZE = static_cast<size_t>(10) * 1024 * 1024;
const size_t MID_MSG_SIZE = static_cast<size_t>(100) * 1024;
const size_t SMALL_MSG_SIZE = static_cast<size_t>(100);

class transfer_private {
public:
  static std::string make_test_message(size_t msg_size) {
    const std::string TEST_MSG = "helloworld";
    std::string data;
    data.reserve(msg_size);
    for (int i = 0; i < msg_size / TEST_MSG.size(); i++) {
      data.append(TEST_MSG);
    }
    return data;
  }
};

class SendBlockCB : public DoNothingConnCallback {
public:
  explicit SendBlockCB(size_t msg_size, size_t total_size)
      : msg_size(msg_size), total_size(total_size) {}

  void on_connect(std::shared_ptr<Connection> conn,
                  std::unique_ptr<MsgSender> sender) override {
    auto rcv = std::make_unique<DoNothingReceiver>();

    std::string data = transfer_private::make_test_message(msg_size);
    size_t msg_count = total_size / msg_size;

    conn->start(std::move(rcv));
    std::thread([sender = std::move(sender), data, conn, msg_count]() {
      for (int i = 0; i < msg_count; ++i) {
        sender->send_block(data);
      }
    }).detach();
  }

private:
  size_t msg_size;
  size_t total_size;
};

class CondWaker : public Notifier {
public:
  explicit CondWaker(
      const std::shared_ptr<moodycamel::LightweightSemaphore> &sem)
      : sem(sem) {}

  void wake() override { sem->signal(); }

private:
  std::shared_ptr<moodycamel::LightweightSemaphore> sem;
};

class SendAsyncCB : public DoNothingConnCallback {
public:
  explicit SendAsyncCB(size_t msg_size, size_t total_size)
      : msg_size(msg_size), total_size(total_size) {}

  void on_connect(std::shared_ptr<Connection> conn,
                  std::unique_ptr<MsgSender> sender) override {
    auto rcv = std::make_unique<DoNothingReceiver>();
    auto sem = std::make_shared<moodycamel::LightweightSemaphore>();
    auto waker = std::make_shared<CondWaker>(sem);

    std::string data = transfer_private::make_test_message(msg_size);
    size_t msg_count = total_size / msg_size;

    conn->start(std::move(rcv), waker);
    std::thread([sender = std::move(sender), conn, msg_count, data, sem]() {
      int progress = 0;
      size_t offset = 0;
      std::string_view data_view(data);
      while (progress < msg_count) {
        auto sent = sender->send_async(data_view.substr(offset));
        if (sent == PENDING) {
          sem->wait();
        } else {
          offset += sent;
        }
        if (offset == data.size()) {
          offset = 0;
          progress += 1;
        }
      }
    }).detach();
  }

private:
  size_t msg_size;
  size_t total_size;
};

class SendNoFlushCB : public DoNothingConnCallback {
public:
  explicit SendNoFlushCB(size_t msg_size, size_t total_size)
      : msg_size(msg_size), total_size(total_size) {}

  void on_connect(std::shared_ptr<Connection> conn,
                  std::unique_ptr<MsgSender> sender) override {
    auto rcv = std::make_unique<DoNothingReceiver>();

    std::string data = transfer_private::make_test_message(msg_size);
    size_t msg_count = total_size / msg_size;

    conn->start(std::move(rcv), nullptr, DEFAULT_MSG_BUF_SIZE,
                DEFAULT_READ_MSG_FLUSH_MILLI_SEC, 0);
    std::thread([sender = std::move(sender), data, msg_count, conn]() {
      for (int i = 0; i < msg_count; ++i) {
        sender->send_block(data);
      }
    }).detach();
  }

private:
  size_t msg_size;
  size_t total_size;
};

class SendNonBlockCB : public DoNothingConnCallback {
public:
  explicit SendNonBlockCB(size_t msg_size, size_t total_size)
      : msg_size(msg_size), total_size(total_size) {}

  void on_connect(std::shared_ptr<Connection> conn,
                  std::unique_ptr<MsgSender> sender) override {
    auto rcv = std::make_unique<DoNothingReceiver>();

    std::string data = transfer_private::make_test_message(msg_size);
    size_t msg_count = total_size / msg_size;

    conn->start(std::move(rcv));
    std::thread([sender = std::move(sender), data, msg_count, conn]() {
      for (int i = 0; i < msg_count; ++i) {
        sender->send_nonblock(data);
      }
    }).detach();
  }

private:
  size_t msg_size;
  size_t total_size;
};

class CountReceived : public MsgReceiver {
public:
  explicit CountReceived(const std::shared_ptr<size_t> &buffer,
                         const std::shared_ptr<size_t> &count)
      : buffer(buffer), count(count) {}

  void on_message(std::string_view data) override {
    if (*count % PRINT_INTERVAL == 0) {
      std::cout << "received " << *count << " messages "
                << ",size = " << *buffer << std::endl;
    }
    *buffer += data.length();
    *count += 1;
  }

  std::shared_ptr<size_t> buffer;
  std::shared_ptr<size_t> count;
};

class CountDataNotifyOnCloseCallback : public ConnCallback {
public:
  CountDataNotifyOnCloseCallback()
      : has_closed(false), add_data(std::make_shared<size_t>(0)),
        count(std::make_shared<size_t>(0)) {}

  void on_connect(std::shared_ptr<Connection> conn,
                  std::unique_ptr<MsgSender> send) override {
    auto rcv = std::make_unique<CountReceived>(add_data, count);
    conn->start(std::move(rcv));
    this->sender = std::move(send);
  }

  void on_connection_close(const std::string &local_addr,
                           const std::string &peer_addr) override {
    has_closed.store(true);
    std::cout << "on_connection_close" << std::endl;
    cond.notify_all();
  }

  void on_remote_close(const std::string &local_addr,
                       const std::string &peer_addr) override {
    std::cout << "on_remote_close" << std::endl;
    this->sender.reset();
  }

  void on_listen_error(const std::string &addr,
                       const std::string &err) override {}

  void on_connect_error(const std::string &addr,
                        const std::string &err) override {}

  std::mutex mutex;
  std::condition_variable cond;
  std::atomic_bool has_closed;
  std::shared_ptr<size_t> add_data;
  std::shared_ptr<size_t> count;
  std::unique_ptr<MsgSender> sender;
};

#endif // SOCKET_MANAGER_TEST_TRANSFER_COMMON_H
