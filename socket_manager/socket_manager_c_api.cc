#include "socket_manager_c_api.h"
#include "socket_manager/common/waker.h"
#include "socket_manager/conn_callback.h"
#include "socket_manager/msg_receiver.h"
#include "socket_manager/msg_sender.h"

inline char *string_dup(const std::string &str) {
  auto size = str.size();
  char *buffer = static_cast<char *>(malloc(size + 1));
  memcpy(buffer, str.c_str(), size + 1);
  return buffer;
}

#define SOCKET_MANAGER_CATCH_ERROR(err, expr)                                  \
  try {                                                                        \
    *(err) = nullptr;                                                          \
    expr;                                                                      \
  } catch (std::runtime_error & e) {                                           \
    *(err) = string_dup(e.what());                                             \
  } catch (...) {                                                              \
    *(err) = string_dup("unknown error");                                      \
  }

/**
 * RecvWaker for the sender.
 */
extern "C" void
socket_manager_extern_notifier_wake(SOCKET_MANAGER_C_API_Notifier this_) {
  auto *notifier = reinterpret_cast<socket_manager::Notifier *>(this_.This);
  notifier->wake();
}

extern "C" long
socket_manager_extern_on_msg(SOCKET_MANAGER_C_API_OnMsgObj this_,
                             SOCKET_MANAGER_C_API_ConnMsg msg,
                             SOCKET_MANAGER_C_API_CWaker waker, char **err) {
  auto *receiver =
      reinterpret_cast<socket_manager::MsgReceiverAsync *>(this_.This);
  SOCKET_MANAGER_CATCH_ERROR(err, return receiver->on_message_async(
                                      std::string_view(msg.Bytes, msg.Len),
                                      socket_manager::Waker(waker)))
  // on error, return error and no-byte read, the runtime will close the
  // connection.
  return 0;
}

extern "C" void
socket_manager_extern_on_conn(SOCKET_MANAGER_C_API_OnConnObj this_,
                              SOCKET_MANAGER_C_API_ConnStates states,
                              char **error) {

  auto *conn_cb = static_cast<socket_manager::ConnCallback *>(this_.This);
  switch (states.Code) {
  case SOCKET_MANAGER_C_API_ConnStateCode::Connect: {
    auto on_connect = states.Data.OnConnect;

    std::shared_ptr<socket_manager::Connection> conn(
        new socket_manager::Connection(on_connect.Conn));
    std::shared_ptr<socket_manager::MsgSender> sender(
        new socket_manager::MsgSender(on_connect.Send, conn));

    // keep the connection alive
    {
      std::unique_lock<std::mutex> const lock(conn_cb->lock);
      conn_cb->conns[conn->local_address() + conn->peer_address()] = conn;
    }
    SOCKET_MANAGER_CATCH_ERROR(
        error, conn_cb->on_connect(std::move(conn), std::move(sender)))
    break;
  }
  case SOCKET_MANAGER_C_API_ConnStateCode::ConnectionClose: {
    auto on_connection_close = states.Data.OnConnectionClose;
    auto local_addr = std::string(on_connection_close.Local);
    auto peer_addr = std::string(on_connection_close.Peer);

    // remove the connection from the map
    {
      std::unique_lock<std::mutex> const lock(conn_cb->lock);
      conn_cb->conns.erase(local_addr + peer_addr);
    }
    SOCKET_MANAGER_CATCH_ERROR(
        error, conn_cb->on_connection_close(local_addr, peer_addr))
    break;
  }
  case SOCKET_MANAGER_C_API_ConnStateCode::ListenError: {
    auto listen_error = states.Data.OnListenError;
    auto addr = std::string(listen_error.Addr);
    auto err = std::string(listen_error.Err);
    SOCKET_MANAGER_CATCH_ERROR(error, conn_cb->on_listen_error(addr, err))
    break;
  }
  case SOCKET_MANAGER_C_API_ConnStateCode::ConnectError: {
    auto connect_error = states.Data.OnConnectError;
    auto addr = std::string(connect_error.Addr);
    auto err = std::string(connect_error.Err);
    SOCKET_MANAGER_CATCH_ERROR(error, conn_cb->on_connect_error(addr, err))
    break;
  }
  case SOCKET_MANAGER_C_API_ConnStateCode::RemoteClose: {
    auto on_remote_close = states.Data.OnRemoteClose;
    auto local_addr = std::string(on_remote_close.Local);
    auto peer_addr = std::string(on_remote_close.Peer);
    SOCKET_MANAGER_CATCH_ERROR(error,
                               conn_cb->on_remote_close(local_addr, peer_addr))
    break;
  }
  default: {
    // should never reach here
    *error = nullptr;
  }
  }
}
