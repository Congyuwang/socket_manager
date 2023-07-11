#include "socket_manager/msg_sender.h"
#include <stdexcept>

namespace socket_manager {

  WakerWrapper::WakerWrapper(std::unique_ptr<Waker> waker) : waker_ref_count(0) {
    this->waker = waker.release();
  }

  void WakerWrapper::wake() {
    this->waker->wake();
  }

  void WakerWrapper::release() {
    if (this->waker_ref_count.fetch_sub(1) <= 0) {
      delete this->waker;
    }
  }

  void WakerWrapper::clone() {
    this->waker_ref_count.fetch_add(1);
  }

  void MsgSender::send(const std::string &data) {
    char *err = nullptr;
    if (msg_sender_send(inner, data.data(), data.length(), &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  long MsgSender::try_send(const std::string &data, size_t offset, std::unique_ptr<Waker> waker) {
    // check length
    if (offset >= data.length()) {
      throw std::runtime_error("offset >= data.length()");
    }
    char *err = nullptr;
    long n = msg_sender_try_send(
            inner,
            data.data() + offset,
            data.length() - offset,
            WakerObj { waker.get() },
            &err);
    if (err) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
    // keep waker alive
    auto wrapper = std::unique_ptr<WakerWrapper>(new WakerWrapper(std::move(waker)));
    conn->waker = std::move(wrapper);
    return n;
  }

  void MsgSender::flush() {
    char *err = nullptr;
    if (msg_sender_flush(inner, &err)) {
      const std::string err_str(err);
      free(err);
      throw std::runtime_error(err_str);
    }
  }

  MsgSender::MsgSender(CMsgSender *inner) : inner(inner) {}

  MsgSender::~MsgSender() {
    msg_sender_free(inner);
  }

} // namespace socket_manager
