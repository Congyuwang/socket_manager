#undef NDEBUG

#include "transfer_common.h"
#include <chrono>
#include <thread>

int test_transfer_data_large_nonblock(int argc, char **argv) {
  const std::string addr = "127.0.0.1:40013";

  auto send_cb = std::make_shared<SendNonBlockCB>(LARGE_MSG_SIZE, TOTAL_SIZE);
  auto store_cb = std::make_shared<CountDataNotifyOnCloseCallback>();
  SocketManager send(send_cb);
  SocketManager store(store_cb);

  send.listen_on_addr(addr);

  std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_MILLIS));

  store.connect_to_addr(addr);

  // Wait for the connection to close
  while (true) {
    if (store_cb->has_closed.load()) {
      auto avg_size = *store_cb->add_data / *store_cb->count;
      std::cout << "received " << *store_cb->count << " messages ,"
                << "total size = " << *store_cb->add_data << " bytes, "
                << "average size = " << avg_size << " bytes" << std::endl;
      assert(*store_cb->add_data == TOTAL_SIZE);
      return 0;
    }
    {
      std::unique_lock<std::mutex> u_lock(store_cb->mutex);
      store_cb->cond.wait_for(u_lock, std::chrono::milliseconds(WAIT_MILLIS));
    }
  }
}
