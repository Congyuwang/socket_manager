#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include "msg_sender.h"
#include "socket_manager_c_api.h"
#include <stdexcept>
#include <string>
#include <memory>

namespace socket_manager {

  /**
   * Manages a set of sockets.
   *
   * Inherit from SocketManager and implement the virtual methods:
   * `on_connect`, `on_connection_close`, `on_listen_error`, `on_connect_error`,
   * `on_message` as callbacks. Note that these callbacks shouldn't block.
   */
  class SocketManager {

  public:

    SocketManager();

    /**
     * Listen on the given address.
     *
     * # Errors
     * Throws `std::runtime_error` if the address is invalid.
     *
     * @param addr: the ip address to listen to (support both ipv4 and ipv6).
     */
    void listen_on_addr(const std::string &addr);

    /**
     * Connect to the given address.
     *
     * # Errors
     * Throws `std::runtime_error` if the address is invalid.
     *
     * @param addr: the ip address to listen to (support both ipv4 and ipv6).
     */
    void connect_to_addr(const std::string &addr);

    /**
     * Abort a connection.
     *
     * Does nothing if the connection is already closed or the id is invalid.
     *
     * @param id the id of the connection to abort.
     */
    void cancel_connection(unsigned long long id);

    /**
     * Cancel listening on the given address.
     *
     * # Errors
     * Throw `std::runtime_error` if the address is invalid.
     *
     * @param addr cancel listening on this address.
     */
    void cancel_listen_on_addr(const std::string &addr);

    /**
     * Join and wait on the `SocketManager` background runtime.
     */
    void join();

    /**
     * Detach the `SocketManager`'s background runtime.
     */
    void detach();

    /// virtual methods: must be thread safe

    /**
     * Called when a new connection is established.
     *
     * It should be non-blocking.
     *
     * @param id the id of the connection.
     * @param local_addr the local address of the connection.
     * @param peer_addr the peer address of the connection.
     * @param sender a `MsgSender` object that can be used to send messages to
     *              the peer.
     */
    virtual void on_connect(unsigned long long id,
                            const std::string &local_addr,
                            const std::string &peer_addr,
                            std::shared_ptr<MsgSender> sender) = 0;

    /**
     * Called when a connection is closed.
     *
     * It should be non-blocking.
     *
     * @param id the id of the connection.
     */
    virtual void on_connection_close(unsigned long long id) = 0;

    /**
     * Called when an error occurs when listening on the given address.
     *
     * Should be non-blocking.
     *
     * @param addr the address that failed to listen on.
     * @param err the error message.
     */
    virtual void on_listen_error(const std::string &addr,
                                 const std::string &err) = 0;

    /**
     * Called when an error occurs when connecting to the given address.
     *
     * Should be non-blocking.
     *
     * @param addr the address that failed to connect to.
     * @param err the error message.
     */
    virtual void on_connect_error(const std::string &addr,
                                  const std::string &err) = 0;

    /**
     * Called when a message is received.
     *
     * Should be non-blocking.
     *
     * @param id the id of the connection.
     * @param data the message received.
     */
    virtual void on_message(unsigned long long id,
                            std::shared_ptr<std::string> data) = 0;

    ~SocketManager();

  private:

    static void on_conn(void *manager_ptr, ConnStates states) {
      auto manager = static_cast<SocketManager *>(manager_ptr);
      switch (states.Code) {
        case ConnStateCode::Connect: {
          auto on_connect = states.Data.OnConnect;
          auto local_addr = std::string(on_connect.Local);
          auto peer_addr = std::string(on_connect.Peer);
          std::shared_ptr<MsgSender> sender(new MsgSender(on_connect.Sender));
          manager->on_connect(on_connect.ConnId, local_addr, peer_addr, sender);
          break;
        }
        case ConnStateCode::ConnectionClose: {
          auto on_connection_close = states.Data.OnConnectionClose;
          manager->on_connection_close(on_connection_close.ConnId);
          break;
        }
        case ConnStateCode::ListenError: {
          auto listen_error = states.Data.OnListenError;
          auto addr = std::string(listen_error.Addr);
          auto err = std::string(listen_error.Err);
          manager->on_listen_error(addr, err);
          break;
        }
        case ConnStateCode::ConnectError: {
          auto connect_error = states.Data.OnConnectError;
          auto addr = std::string(connect_error.Addr);
          auto err = std::string(connect_error.Err);
          manager->on_connect_error(addr, err);
          break;
        }
      }
    }

    static void on_msg(void *manager_ptr, ConnMsg msg) {
      auto manager = static_cast<SocketManager *>(manager_ptr);
      auto data = std::make_shared<std::string>(msg.Bytes, msg.Len);
      manager->on_message(msg.ConnId, data);
    }

    CSocketManager *inner;

  };

} // namespace socket_manager

#endif // SOCKET_MANAGER_H
