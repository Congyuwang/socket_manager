#undef NDEBUG
#include "test_utils.h"
#include <chrono>
#include <thread>
#include <iostream>

int test_hello_world_greetings(int argc, char **argv) {

  const std::string addr = "127.0.0.1:40109";

  // create server
  auto server_cb = std::make_shared<StoreAllEventsConnCallback>();
  auto client_cb = std::make_shared<StoreAllEventsConnCallback>();

  SocketManager server(server_cb);
  SocketManager client(client_cb);

  server.listen_on_addr(addr);
  // Wait for 100ms
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  // create client
  client.connect_to_addr(addr);

  std::cout << "Client connect" << std::endl;

  std::string c_conn_id;
  std::string s_conn_id;

  // wait for connection success (client side)
  while (true) {
    std::unique_lock<std::mutex> u_lock(client_cb->mutex);
    if (client_cb->events.size() == 1) {
      std::cout << "Client connection established: " << std::get<1>(client_cb->events[0]) << std::endl;
      assert(std::get<0>(client_cb->events[0]) == CONNECTED);
      c_conn_id = std::get<1>(client_cb->events[0]);
      break;
    }
    client_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
  }

  // wait for connection success (server side)
  while (true) {
    std::unique_lock<std::mutex> u_lock(server_cb->mutex);
    if (server_cb->events.size() == 1) {
      std::cout << "Server connection established: " << std::get<1>(server_cb->events[0]) << std::endl;
      assert(std::get<0>(server_cb->events[0]) == CONNECTED);
      s_conn_id = std::get<1>(server_cb->events[0]);
      server.cancel_listen_on_addr(addr);
      break;
    }
    server_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
  }

  // send message
  client_cb->send_to(c_conn_id, "hello world");

  // wait for server receive
  while (true) {
    std::unique_lock<std::mutex> u_lock(server_cb->mutex);
    if (server_cb->buffer.size() == 1) {
      std::cout << "Server received: " << *std::get<1>(server_cb->buffer[0])
                << " from connection=" << std::get<0>(server_cb->buffer[0]) << std::endl;
      assert(std::get<0>(server_cb->buffer[0]) == s_conn_id);
      assert(*std::get<1>(server_cb->buffer[0]) == "hello world");
      break;
    }
    server_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
  }

  server_cb->send_to(s_conn_id, "hello world");

  // wait for client receive
  while (true) {
    std::unique_lock<std::mutex> u_lock(client_cb->mutex);
    if (client_cb->buffer.size() == 1) {
      std::cout << "Client received: " << *std::get<1>(client_cb->buffer[0])
                << " from connection=" << std::get<0>(client_cb->buffer[0]) << std::endl;
      assert(std::get<0>(client_cb->buffer[0]) == c_conn_id);
      assert(*std::get<1>(client_cb->buffer[0]) == "hello world");
      break;
    }
    client_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
  }

  // drop sender
  server_cb->drop_connection(s_conn_id);

  // wait for connection close
  while (true) {
    std::unique_lock<std::mutex> u_lock(server_cb->mutex);
    if (server_cb->events.size() == 2) {
      std::cout << "Connection closed: " << std::get<1>(server_cb->events[1]) << std::endl;
      assert(std::get<0>(server_cb->events[1]) == CONNECTION_CLOSED);
      break;
    }
    server_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
  }

  // wait for connection close
  while (true) {
    std::unique_lock<std::mutex> u_lock(client_cb->mutex);
    if (client_cb->events.size() == 2) {
      assert(std::get<0>(client_cb->events[1]) == CONNECTION_CLOSED);
      std::cout << "Connection closed: " << std::get<1>(client_cb->events[1]) << std::endl;
      break;
    }
    client_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
  }

  client_cb->drop_connection(c_conn_id);

  return 0;
}
