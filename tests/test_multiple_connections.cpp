#undef NDEBUG
#include "test_utils.h"
#include <chrono>
#include <thread>

int test_multiple_connections(int argc, char **argv) {

  // establish 3 connections from p0 (client) -> p1 (server) port 0
  // and 2 connections from p0 (client) -> p1 (server) port 1
  // and 2 connections from p1 (client) -> p0 (server)

  const int TOTAL_CONN = 7;

  const std::string p1_addr_0 = "127.0.0.1:40010";
  const std::string p1_addr_1 = "127.0.0.1:40011";
  const std::string p0_addr_0 = "127.0.0.1:40012";

  auto p0_cb = std::make_shared<StoreAllEventsConnCallback>();
  auto p1_cb = std::make_shared<StoreAllEventsConnCallback>();

  SocketManager party0(p0_cb);
  SocketManager party1(p1_cb);

  // listen
  party1.listen_on_addr(p1_addr_0);
  party1.listen_on_addr(p1_addr_1);
  party0.listen_on_addr(p0_addr_0);

  // wait for 100ms
  std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_MILLIS));

  // establish connections
  party0.connect_to_addr(p1_addr_0);
  party0.connect_to_addr(p1_addr_0);
  party0.connect_to_addr(p1_addr_0);

  party0.connect_to_addr(p1_addr_1);
  party0.connect_to_addr(p1_addr_1);

  party1.connect_to_addr(p0_addr_0);
  party1.connect_to_addr(p0_addr_0);

  // wait for all connections established (7 in total)
  {
    std::unique_lock<std::mutex> u_lock(*p0_cb->mutex);
    p0_cb->cond->wait_for(
        u_lock, std::chrono::milliseconds(WAIT_MILLIS),
        [p0_cb]() { return p0_cb->events->size() == TOTAL_CONN; });
  }

  {
    std::unique_lock<std::mutex> u_lock(*p1_cb->mutex);
    p1_cb->cond->wait_for(
        u_lock, std::chrono::milliseconds(WAIT_MILLIS),
        [p1_cb]() { return p1_cb->events->size() == TOTAL_CONN; });
  }

  // send messages from p0 to p1 and vice versa
  for (auto &event : *p0_cb->events) {
    p0_cb->send_to(std::get<1>(event), "hello world");
  }
  for (auto &event : *p1_cb->events) {
    p1_cb->send_to(std::get<1>(event), "hello world");
  }

  // check messages received in buffer
  // confirm all connections from p1 are closed
  {
    std::unique_lock<std::mutex> u_lock(*p0_cb->mutex);
    p0_cb->cond->wait_for(
        u_lock, std::chrono::milliseconds(WAIT_MILLIS),
        [p0_cb]() { return p0_cb->buffer->size() == TOTAL_CONN; });
  }
  {
    std::unique_lock<std::mutex> u_lock(*p1_cb->mutex);
    p1_cb->cond->wait_for(
        u_lock, std::chrono::milliseconds(WAIT_MILLIS),
        [p1_cb]() { return p1_cb->buffer->size() == TOTAL_CONN; });
  }

  // shutdown all connections from p0
  std::vector<std::string> connections;
  {
    std::unique_lock<std::mutex> u_lock(*p0_cb->mutex);
    for (auto &event : *p0_cb->events) {
      if (std::get<0>(event) == CONNECTED) {
        connections.push_back(std::get<1>(event));
      }
    }
  }
  assert(connections.size() == 7);
  for (auto &connect : connections) {
    p0_cb->drop_connection(connect);
  }

  // confirm all connections from p1 are closed
  {
    std::unique_lock<std::mutex> u_lock(*p0_cb->mutex);
    p0_cb->cond->wait_for(
        u_lock, std::chrono::milliseconds(WAIT_MILLIS),
        [p0_cb]() { return p0_cb->connected_count->load() == 0; });
  }
  {
    std::unique_lock<std::mutex> u_lock(*p1_cb->mutex);
    p1_cb->cond->wait_for(
        u_lock, std::chrono::milliseconds(WAIT_MILLIS),
        [p1_cb]() { return p1_cb->connected_count->load() == 0; });
  }

  return 0;
}
