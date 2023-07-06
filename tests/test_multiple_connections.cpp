#include "test_utils.h"
#include <chrono>
#include <thread>

int test_multiple_connections(int argc, char **argv) {

  // establish 3 connections from p0 (client) -> p1 (server) port 0
  // and 2 connections from p0 (client) -> p1 (server) port 1
  // and 2 connections from p1 (client) -> p0 (server)

  const std::string p1_addr_0 = "127.0.0.1:12350";
  const std::string p1_addr_1 = "127.0.0.1:12351";
  const std::string p0_addr_0 = "127.0.0.1:12352";

  auto p0_cb = std::make_shared<StoreAllEventsConnCallback>();
  auto p1_cb = std::make_shared<StoreAllEventsConnCallback>();

  SocketManager p0(p0_cb);
  SocketManager p1(p1_cb);

  // listen
  p1.listen_on_addr(p1_addr_0);
  p1.listen_on_addr(p1_addr_1);
  p0.listen_on_addr(p0_addr_0);

  // wait for 100ms
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // establish connections
  for (int i = 0; i < 3; i++) {
    p0.connect_to_addr(p1_addr_0);
  }
  for (int i = 0; i < 2; i++) {
    p0.connect_to_addr(p1_addr_1);
  }
  for (int i = 0; i < 2; i++) {
    p1.connect_to_addr(p0_addr_0);
  }

  // wait for all connections established (7 in total)
  while (true) {
    std::unique_lock<std::mutex> u_lock(p0_cb->mutex);
    if (p0_cb->events.size() == 7) {
      for (auto &e: p0_cb->events) {
        std::cout << "p0 connection established: " << std::get<1>(e) << std::endl;
        assert(std::get<0>(e) == CONNECTED);
      }
      break;
    }
    p0_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
  }
  while (true) {
    std::unique_lock<std::mutex> u_lock(p1_cb->mutex);
    if (p1_cb->events.size() == 7) {
      for (auto &e: p1_cb->events) {
        std::cout << "p0 connection established: " << std::get<1>(e) << std::endl;
        assert(std::get<0>(e) == CONNECTED);
      }
      break;
    }
    p1_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
  }

  // send messages from p0 to p1 and vice versa
  for (auto &e: p0_cb->events) {
    p0_cb->send_to(std::get<1>(e), "hello world");
  }
  for (auto &e: p1_cb->events) {
    p1_cb->send_to(std::get<1>(e), "hello world");
  }

  // check messages received in buffer
  while (true) {
    std::unique_lock<std::mutex> u_lock(p0_cb->mutex);
    if (p0_cb->buffer.size() == 7) {
      for (auto &m: p0_cb->buffer) {
        std::cout << "p0 received from connection " << std::get<0>(m)
                  << ": " << *std::get<1>(m) << std::endl;
        assert(*std::get<1>(m) == "hello world");
      }
      break;
    }
    p0_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
  }
  while (true) {
    std::unique_lock<std::mutex> u_lock(p1_cb->mutex);
    if (p1_cb->buffer.size() == 7) {
      for (auto &m: p1_cb->buffer) {
        std::cout << "p1 received from connection " << std::get<0>(m)
                  << ": " << *std::get<1>(m) << std::endl;
        assert(*std::get<1>(m) == "hello world");
      }
      break;
    }
    p1_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
  }

  // shutdown all connections from p0
  std::vector<std::string> connections;
  {
    std::unique_lock<std::mutex> u_lock(p0_cb->mutex);
    for (auto &e: p0_cb->events) {
      if (std::get<0>(e) == CONNECTED) {
        connections.push_back(std::get<1>(e));
      }
    }
  }
  assert(connections.size() == 7);
  for (auto &c: connections) {
    p0_cb->drop_connection(c);
  }

  // confirm all connections from p1 are closed
  while (true) {
    std::cout << "p1 connected count: " << p1_cb->connected_count.load() << std::endl;
    if (p1_cb->connected_count.load() == 0) {
      break;
    }
    {
      std::unique_lock<std::mutex> u_lock(p1_cb->mutex);
      p1_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
    }
  }
  while (true) {
    std::cout << "p0 connected count: " << p0_cb->connected_count.load() << std::endl;
    if (p0_cb->connected_count.load() == 0) {
      break;
    }
    {
      std::unique_lock<std::mutex> u_lock(p0_cb->mutex);
      p0_cb->cond.wait_for(u_lock, std::chrono::milliseconds(10));
    }
  }

  return 0;
}
