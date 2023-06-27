#include "test_utils.h"
#include <chrono>
#include <thread>
#include <iostream>

int test_hello_world_greetings(int argc, char **argv) {

  const std::string addr = "127.0.0.1:8080";

  // create server
  StoreAllEventsSocketManager server;
  server.listen_on_addr(addr);
  server.detach();

  // Wait for 100ms
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // create client
  StoreAllEventsSocketManager client;
  client.connect_to_addr(addr);
  client.detach();

  std::string c_conn_id;
  std::string s_conn_id;

  // wait for connection success (client side)
  while (true) {
    std::unique_lock<std::mutex> u_lock(client.mutex);
    if (client.events.size() == 1) {
      std::cout << "Client connection established: " << std::get<1>(client.events[0]) << std::endl;
      assert(std::get<0>(client.events[0]) == CONNECTED);
      c_conn_id = std::get<1>(client.events[0]);
      server.cancel_listen_on_addr(addr);
      break;
    }
    client.cond.wait(u_lock);
  }

  // wait for connection success (server side)
  while (true) {
    std::unique_lock<std::mutex> u_lock(server.mutex);
    if (server.events.size() == 1) {
      std::cout << "Server connection established: " << std::get<1>(server.events[0]) << std::endl;
      assert(std::get<0>(server.events[0]) == CONNECTED);
      s_conn_id = std::get<1>(server.events[0]);
      server.cancel_listen_on_addr(addr);
      break;
    }
    server.cond.wait(u_lock);
  }

  // send message
  client.send_to(c_conn_id, "hello world");

  // wait for server receive
  while (true) {
    std::unique_lock<std::mutex> u_lock(server.mutex);
    if (server.buffer.size() == 1) {
      std::cout << "Server received: " << *std::get<1>(server.buffer[0])
                << " from connection=" << std::get<0>(server.buffer[0]) << std::endl;
      assert(std::get<0>(server.buffer[0]) == s_conn_id);
      assert(*std::get<1>(server.buffer[0]) == "hello world");
      break;
    }
    server.cond.wait(u_lock);
  }

  server.send_to(s_conn_id, "hello world");

  // wait for client receive
  while (true) {
    std::unique_lock<std::mutex> u_lock(client.mutex);
    if (client.buffer.size() == 1) {
      std::cout << "Client received: " << *std::get<1>(client.buffer[0])
                << " from connection=" << std::get<0>(client.buffer[0]) << std::endl;
      assert(std::get<0>(client.buffer[0]) == c_conn_id);
      assert(*std::get<1>(client.buffer[0]) == "hello world");
      break;
    }
    client.cond.wait(u_lock);
  }

  // drop sender
  server.drop_connection(s_conn_id);
  server.send_to(s_conn_id, "hello world");

  // wait for connection close
  while (true) {
    std::unique_lock<std::mutex> u_lock(server.mutex);
    if (server.events.size() == 2) {
      std::cout << "code " << std::get<0>(server.events[1]) << std::endl;
      std::cout << "Connection closed: " << std::get<1>(server.events[1]) << std::endl;
      assert(std::get<0>(server.events[1]) == CONNECTION_CLOSED);
      break;
    }
    server.cond.wait(u_lock);
  }

  // wait for connection close
  while (true) {
    std::unique_lock<std::mutex> u_lock(client.mutex);
    if (client.events.size() == 2) {
      assert(std::get<0>(client.events[1]) == CONNECTION_CLOSED);
      std::cout << "Connection closed: " << std::get<1>(client.events[1]) << std::endl;
      break;
    }
    client.cond.wait(u_lock);
  }

  client.drop_connection(c_conn_id);

  return 0;
}
