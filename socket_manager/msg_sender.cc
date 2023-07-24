#include "socket_manager/msg_sender.h"
#include <stdexcept>
#include <iostream>

namespace socket_manager {

  void MsgSender::send(std::string_view data) {
    char *err = nullptr;
    if (msg_sender_send(inner.get(), data.data(), data.length(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  long MsgSender::try_send(std::string_view data, size_t offset, const std::shared_ptr<Waker> &waker) {
    auto dat_view = data.substr(offset);
    char *err = nullptr;
    long n = msg_sender_try_send(
            inner.get(),
            dat_view.data(),
            dat_view.length(),
            WakerObj{waker.get()},
            &err);
    if (err) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
    // keep waker alive
    std::cout << "keep waker alive" << std::endl;
    std::cout << "keep waker alive (after clone)" << std::endl;
//    conn->waker = waker;
    std::cout << "keep waker alive (after move)" << std::endl;
    return n;
  }

  void MsgSender::flush() {
    char *err = nullptr;
    if (msg_sender_flush(inner.get(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  MsgSender::MsgSender(CMsgSender *inner, const std::shared_ptr<Connection> &conn)
          : conn(conn),
            inner(inner, [](CMsgSender *ptr) { msg_sender_free(ptr); }) {
  }

} // namespace socket_manager
