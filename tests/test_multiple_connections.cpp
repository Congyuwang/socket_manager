#include "test_utils.h"
#include <chrono>
#include <thread>

int test_multiple_connections(int argc, char **argv) {

  // establish 3 connections from p0 (client) -> p1 (server) port 0
  // and 2 connections from p0 (client) -> p1 (server) port 1
  // and 2 connections from p1 (client) -> p0 (server)

  const std::string p1_addr_0 = "127.0.0.1:8080";
  const std::string p1_addr_1 = "127.0.0.1:9090";
  const std::string p0_addr_0 = "127.0.0.1:12345";

  StoreAllEventsSocketManager p0;
  StoreAllEventsSocketManager p1;

  // listen
  p1.listen_on_addr(p1_addr_0);
  p1.listen_on_addr(p1_addr_1);
  p0.listen_on_addr(p0_addr_0);

  // wait for 100ms
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

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
    std::unique_lock<std::mutex> u_lock(p0.mutex);
    if (p0.events.size() == 7) {
      for (auto &e: p0.events) {
        std::cout << "p0 connection established: " << std::get<1>(e) << std::endl;
        assert(std::get<0>(e) == CONNECTED);
      }
      break;
    }
    p0.cond.wait(u_lock);
  }
  while (true) {
    std::unique_lock<std::mutex> u_lock(p1.mutex);
    if (p1.events.size() == 7) {
      for (auto &e: p1.events) {
        std::cout << "p0 connection established: " << std::get<1>(e) << std::endl;
        assert(std::get<0>(e) == CONNECTED);
      }
      break;
    }
    p1.cond.wait(u_lock);
  }

  // send messages from p0 to p1 and vice versa
  for (auto &e: p0.events) {
    p0.send_to(std::get<1>(e), "hello world");
  }
  for (auto &e: p1.events) {
    p1.send_to(std::get<1>(e), "hello world");
  }

  // check messages received in buffer
  while (true) {
    std::unique_lock<std::mutex> u_lock(p0.mutex);
    if (p0.buffer.size() == 7) {
      for (auto &m: p0.buffer) {
        std::cout << "p0 received from connection " << std::get<0>(m)
                  << ": " << *std::get<1>(m) << std::endl;
        assert(*std::get<1>(m) == "hello world");
      }
      break;
    }
    p0.cond.wait(u_lock);
  }
  while (true) {
    std::unique_lock<std::mutex> u_lock(p1.mutex);
    if (p1.buffer.size() == 7) {
      for (auto &m: p1.buffer) {
        std::cout << "p1 received from connection " << std::get<0>(m)
                  << ": " << *std::get<1>(m) << std::endl;
        assert(*std::get<1>(m) == "hello world");
      }
      break;
    }
    p1.cond.wait(u_lock);
  }

  // shutdown all connections from p0
  std::vector<std::string> connections;
  {
    std::unique_lock<std::mutex> u_lock(p0.mutex);
    for (auto &e: p0.events) {
      if (std::get<0>(e) == CONNECTED) {
        connections.push_back(std::get<1>(e));
      }
    }
  }
  assert(connections.size() == 7);
  for (auto &c: connections) {
    p0.drop_connection(c);
  }

  // confirm all connections from p1 are closed
  while (true) {
    std::cout << "p1 connected count: " << p1.connected_count.load() << std::endl;
    if (p1.connected_count.load() == 0) {
      break;
    }
    {
      std::unique_lock<std::mutex> u_lock(p1.mutex);
      p1.cond.wait(u_lock);
    }
  }
  while (true) {
    std::cout << "p0 connected count: " << p0.connected_count.load() << std::endl;
    if (p0.connected_count.load() == 0) {
      break;
    }
    {
      std::unique_lock<std::mutex> u_lock(p0.mutex);
      p0.cond.wait(u_lock);
    }
  }


  return 0;
}
