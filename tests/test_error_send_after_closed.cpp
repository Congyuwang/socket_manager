#include "test_utils.h"
#include <chrono>
#include <thread>

int test_error_send_after_closed(int argc, char **argv) {
  const std::string addr = "127.0.0.1:12347";

  auto server_cb = std::make_shared<StoreAllEventsConnCallback>();
  auto client_cb = std::make_shared<StoreAllEventsConnCallback>(false);

  SocketManager server(server_cb);
  SocketManager client(client_cb);

  server.listen_on_addr(addr);

  // Wait for 100ms
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  client.connect_to_addr(addr);

  // wait for connection success (server)
  std::string s_conn_id;
  while (true) {
    std::unique_lock<std::mutex> u_lock(server_cb->mutex);
    if (server_cb->events.size() == 1) {
      assert(std::get<0>(server_cb->events[0]) == CONNECTED);
      s_conn_id = std::get<1>(server_cb->events[0]);
      break;
    }
    server_cb->cond.wait(u_lock);
  }

  // close connection from server (by dropping sender)
  server_cb->drop_connection(s_conn_id);

  // wait for connection closed (client)
  std::string c_conn_id;
  while (true) {
    std::unique_lock<std::mutex> u_lock(client_cb->mutex);
    if (client_cb->events.size() == 2) {
      assert(std::get<0>(client_cb->events[0]) == CONNECTED);
      assert(std::get<0>(client_cb->events[1]) == CONNECTION_CLOSED);
      c_conn_id = std::get<1>(client_cb->events[0]);
      assert(std::get<1>(client_cb->events[1]) == c_conn_id);
      break;
    }
    client_cb->cond.wait(u_lock);
  }

  // Wait for 100ms
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // should emit runtime error if attempt to send from client after closed
  try {
    client_cb->send_to(c_conn_id, "hello world");
  } catch (std::runtime_error &e) {
    std::cout << "Caught runtime error: " << e.what() << std::endl;
    return 0;
  }

  // should not reach here
  return -1;

}
