#include <thread>
#include <iostream>
#include "echo_server.h"

/**
 * A send task can be polled,
 * and call `wake` to enqueued.
 * This implementation uses socket_manager's
 * `poll_send` async interface.
 */
class SendTask : public std::enable_shared_from_this<SendTask>,
                 public socket_manager::Waker {
public:
  SendTask(std::string_view data,
           const std::shared_ptr<socket_manager::MsgSender> &sender,
           const std::shared_ptr<moodycamel::LightweightSemaphore> &send_sem,
           const std::shared_ptr<moodycamel::ConcurrentQueue<std::shared_ptr<SendTask>>> &send_queue)
          : data(data), offset(0), sender(sender), send_sem(send_sem), send_queue(send_queue) {}

  /**
   * Attempt to send all data of the task.
   * If `sent < 0`, the task is pending,
   * wait until the waker to enqueue the task.
   */
  void poll_send() {
    while (true) {
      try {
        // attempt to send some bytes
        long sent = sender->try_send(data, offset, shared_from_this());
        if (sent > 0) {
          offset += sent;
        } else {
          // sent = 0 -> message complete, or connection closed by remote.
          // sent < 0 -> pending
          break;
        }
      } catch (const std::runtime_error &e) {
        // might be connection closed
        break;
      }
    }
  }

  /**
   * enqueue the send task into send_queue,
   * and send signal.
   */
  void wake() override {
    send_queue->enqueue(shared_from_this());
    send_sem->signal();
  }

private:
  void release() override {}

  void clone() override {}

  std::string data;
  long offset;
  std::shared_ptr<socket_manager::MsgSender> sender;
  std::shared_ptr<moodycamel::LightweightSemaphore> send_sem;
  std::shared_ptr<moodycamel::ConcurrentQueue<std::shared_ptr<SendTask>>> send_queue;
};

/**
 * The echo receiver on receiving messages,
 * create a task of sending message, and
 * place the task in the sending queue.
 */
class EchoReceiver : public socket_manager::MsgReceiver {
public:
  EchoReceiver(
          const std::shared_ptr<moodycamel::LightweightSemaphore> &send_sem,
          const std::shared_ptr<moodycamel::ConcurrentQueue<std::shared_ptr<SendTask>>> &send_queue,
          std::shared_ptr<socket_manager::MsgSender> &&sender
  ) : send_sem(send_sem), send_queue(send_queue), sender(sender) {}

private:
  void on_message(std::string_view data) override {
    auto task = std::make_shared<SendTask>(data, sender, send_sem, send_queue);
    // initial wake
    task->wake();
  }

  std::shared_ptr<moodycamel::LightweightSemaphore> send_sem;
  std::shared_ptr<moodycamel::ConcurrentQueue<std::shared_ptr<SendTask>>> send_queue;
  std::shared_ptr<socket_manager::MsgSender> sender;
};

/**
 * The callback sends most of the events outside through
 * `next()` API.
 */
class EchoCallback : public socket_manager::ConnCallback {
public:
  EchoCallback(const std::shared_ptr<moodycamel::LightweightSemaphore> &sendSem,
               const std::shared_ptr<moodycamel::ConcurrentQueue<std::shared_ptr<SendTask>>> &sendQueue)
          : send_sem(sendSem), send_queue(sendQueue) {}

  /**
   * This is a blocking api, which relies on semaphore.
   * @param cb
   * @return
   */
  CB next() {
    sem.wait();
    CB cb;
    event.try_dequeue(cb);
    return cb;
  }

private:
  void on_connect(const std::string &local_addr, const std::string &peer_addr,
                  std::shared_ptr<socket_manager::Connection> conn,
                  std::shared_ptr<socket_manager::MsgSender> sender) override {
    auto recv = std::make_unique<EchoReceiver>(send_sem, send_queue, std::move(sender));
    conn->start(std::move(recv), 256 * 1024);
  }

  void on_connection_close(const std::string &local_addr, const std::string &peer_addr) override {
    event.enqueue(ConnClose{
            local_addr,
            peer_addr,
    });
    sem.signal();
  }

  void on_listen_error(const std::string &addr, const std::string &err) override {
    event.enqueue(ListenErr{
            addr,
            err,
    });
    sem.signal();
  }

  void on_connect_error(const std::string &addr, const std::string &err) override {
    event.enqueue(ConnErr{
            addr,
            err,
    });
    sem.signal();
  }

  moodycamel::LightweightSemaphore sem;
  moodycamel::ConcurrentQueue<CB> event;
  std::shared_ptr<moodycamel::LightweightSemaphore> send_sem;
  std::shared_ptr<moodycamel::ConcurrentQueue<std::shared_ptr<SendTask>>> send_queue;
};

int main() {
  // send tasks send_queue
  auto send_sem = std::make_shared<moodycamel::LightweightSemaphore>();
  auto send_queue = std::make_shared<moodycamel::ConcurrentQueue<std::shared_ptr<SendTask>>>();

  // start the server
  auto callback = std::make_shared<EchoCallback>(send_sem, send_queue);
  auto manager = socket_manager::SocketManager(callback);
  manager.listen_on_addr("127.0.0.1:10101");

  // message sending loop (most work is done here).
  // This is a mini runtime specialized in doing echoing.
  std::thread([=]() {
    while (true) {
      // wait for a task
      send_sem->wait();
      std::shared_ptr<SendTask> task;
      if (!send_queue->try_dequeue(task)) {
        [[unlikely]]
        std::cerr << "internal error" << std::endl;
        break;
      } else {
        [[likely]]
        task->poll_send();
      }
    }
  }).detach();

  // process callback events
  while (true) {
    CB cb = callback->next();
    int ret = std::visit(overloaded{
            [](const ConnErr &arg) {
              std::cerr << "connection error: addr=" << arg.addr << ", " << arg.err << std::endl;
              return -1;
            },
            [](const ListenErr &arg) {
              std::cerr << "listen error: addr" << arg.addr << ", " << arg.err << std::endl;
              return -1;
            },
            [](const ConnClose &arg) {
              // just do some simple logging,
              // resources are automatically managed.
              std::cerr << "connection closed: " << arg.local << " -> " << arg.peer << std::endl;
              return 0;
            }
    }, cb);

    // on error, stop the server
    if (ret != 0) {
      return ret;
    }
  }
}
