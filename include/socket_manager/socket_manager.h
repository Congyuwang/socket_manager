#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include "msg_sender.h"
#include "socket_manager_c_api.h"
#include <stdexcept>
#include <string>
#include <memory>

namespace socket_manager {

  class SocketManager {

  public:

    SocketManager();

    void listen_on_addr(const std::string &addr);

    void connect_to_addr(const std::string &addr);

    void cancel_connection(unsigned long long id);

    void cancel_listen_on_addr(const std::string &addr);

    void join();

    void detach();

    /// virtual methods: must be thread safe

    virtual void on_connect(unsigned long long id,
                            const std::string &local_addr,
                            const std::string &peer_addr,
                            std::shared_ptr<MsgSender> sender) = 0;

    virtual void on_connection_close(unsigned long long id) = 0;

    virtual void on_listen_error(const std::string &addr,
                                 const std::string &err) = 0;

    virtual void on_connect_error(const std::string &addr,
                                  const std::string &err) = 0;

    virtual void on_message(unsigned long long id,
                            const std::string &data) = 0;

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
      auto local_addr = std::string(msg.Bytes);
      manager->on_message(msg.ConnId, local_addr);
    }

    CSocketManager *inner;

  };

} // namespace socket_manager

#endif // SOCKET_MANAGER_H
