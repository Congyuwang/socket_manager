#include "socket_manager_c_api.h"
#include "socket_manager/msg_receiver.h"
#include "socket_manager/conn_callback.h"
#include "socket_manager/recv_waker.h"

inline
static char *string_dup(const std::string &str) {
  auto size = str.size();
  char *buffer = (char *) malloc(size + 1);
  memcpy(buffer, str.c_str(), size + 1);
  return buffer;
}

/**
 * RcvWaker for the sender.
 */
extern void socket_manager_extern_sender_waker_wake(struct WakerObj this_) {
  auto wr = reinterpret_cast<socket_manager::Waker *>(this_.This);
  wr->wake();
}

extern void socket_manager_extern_sender_waker_release(struct WakerObj this_) {
  auto wr = reinterpret_cast<socket_manager::Waker *>(this_.This);
  wr->release();
}

extern void socket_manager_extern_sender_waker_clone(struct WakerObj this_) {
  auto wr = reinterpret_cast<socket_manager::Waker *>(this_.This);
  wr->clone();
}

extern long socket_manager_extern_on_msg(struct OnMsgObj this_, ConnMsg msg, CWaker *waker, char **err) {
  auto receiver = reinterpret_cast<socket_manager::MsgReceiverAsync *>(this_.This);
  try {
    auto recv = receiver->on_message_async(
            std::string_view(msg.Bytes, msg.Len),
            std::shared_ptr<RcvWaker>(new RcvWaker(waker))
    );
    *err = nullptr;
    return recv;
  } catch (std::runtime_error &e) {
    *err = string_dup(e.what());
    return 0;
  } catch (...) {
    *err = string_dup("unknown error");
    return 0;
  }
}

extern void socket_manager_extern_on_conn(struct OnConnObj this_, ConnStates states, char **error) {

  auto conn_cb = static_cast<socket_manager::ConnCallback *>(this_.This);
  switch (states.Code) {
    case ConnStateCode::Connect: {
      auto on_connect = states.Data.OnConnect;
      auto local_addr = std::string(on_connect.Local);
      auto peer_addr = std::string(on_connect.Peer);

      std::shared_ptr<socket_manager::Connection> conn(new socket_manager::Connection(on_connect.Conn));
      std::shared_ptr<socket_manager::MsgSender> sender(new socket_manager::MsgSender(on_connect.Send, conn));

      // keep the connection alive
      {
        std::unique_lock<std::mutex> lock(conn_cb->lock);
        conn_cb->conns[local_addr + peer_addr] = conn;
      }
      try {
        conn_cb->on_connect(local_addr, peer_addr, std::move(conn), std::move(sender));
        *error = nullptr;
      } catch (std::runtime_error &e) {
        *error = string_dup(e.what());
      } catch (...) {
        *error = string_dup("unknown error");
      }
      break;
    }
    case ConnStateCode::ConnectionClose: {
      auto on_connection_close = states.Data.OnConnectionClose;
      auto local_addr = std::string(on_connection_close.Local);
      auto peer_addr = std::string(on_connection_close.Peer);

      // remove the connection from the map
      {
        std::unique_lock<std::mutex> lock(conn_cb->lock);
        conn_cb->conns.erase(local_addr + peer_addr);
      }
      try {
        conn_cb->on_connection_close(local_addr, peer_addr);
        *error = nullptr;
      } catch (std::runtime_error &e) {
        *error = string_dup(e.what());
      } catch (...) {
        *error = string_dup("unknown error");
      }
      break;
    }
    case ConnStateCode::ListenError: {
      auto listen_error = states.Data.OnListenError;
      auto addr = std::string(listen_error.Addr);
      auto err = std::string(listen_error.Err);
      try {
        conn_cb->on_listen_error(addr, err);
        *error = nullptr;
      } catch (std::runtime_error &e) {
        *error = string_dup(e.what());
      } catch (...) {
        *error = string_dup("unknown error");
      }
      break;
    }
    case ConnStateCode::ConnectError: {
      auto connect_error = states.Data.OnConnectError;
      auto addr = std::string(connect_error.Addr);
      auto err = std::string(connect_error.Err);
      try {
        conn_cb->on_connect_error(addr, err);
        *error = nullptr;
      } catch (std::runtime_error &e) {
        *error = string_dup(e.what());
      } catch (...) {
        *error = string_dup("unknown error");
      }
      break;
    }
    default: {
      // should never reach here
      *error = nullptr;
    }
  }
}
