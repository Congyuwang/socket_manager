#include "socket_manager_c_api.h"
#include "socket_manager/msg_receiver.h"
#include "socket_manager/conn_callback.h"

inline
static char *string_dup(const std::string &str) {
  auto size = str.size();
  char *buffer = (char *) malloc(size + 1);
  memcpy(buffer, str.c_str(), size + 1);
  return buffer;
}

/**
 * Waker for the sender.
 */
extern void socket_manager_extern_sender_waker_wake(struct WakerObj this_) {
  auto wr = reinterpret_cast<socket_manager::WakerWrapper *>(this_.This);
  wr->wake();
}

extern void socket_manager_extern_sender_waker_release(struct WakerObj this_) {
  auto wr = reinterpret_cast<socket_manager::WakerWrapper *>(this_.This);
  wr->release();
}

extern void socket_manager_extern_sender_waker_clone(struct WakerObj this_) {
  auto wr = reinterpret_cast<socket_manager::WakerWrapper *>(this_.This);
  wr->clone();
}

extern char *socket_manager_extern_on_msg(struct OnMsgObj this_, ConnMsg msg) {
  auto receiver = reinterpret_cast<socket_manager::MsgReceiver *>(this_.This);
  auto data_ptr = std::make_shared<std::string>(msg.Bytes, msg.Len);
  try {
    receiver->on_message(data_ptr);
  } catch (std::runtime_error &e) {
    return string_dup(e.what());
  } catch (...) {
    return string_dup("unknown error");
  }
  return nullptr;
}

extern char *socket_manager_extern_on_conn(struct OnConnObj this_, ConnStates states) {

  auto conn_cb = static_cast<socket_manager::ConnCallback *>(this_.This);
  switch (states.Code) {
    case ConnStateCode::Connect: {
      auto on_connect = states.Data.OnConnect;
      auto local_addr = std::string(on_connect.Local);
      auto peer_addr = std::string(on_connect.Peer);

      std::shared_ptr<socket_manager::Connection> conn(new socket_manager::Connection(on_connect.Conn));

      // keep the connection alive
      {
        std::unique_lock<std::mutex> lock(conn_cb->lock);
        conn_cb->conns[local_addr + peer_addr] = conn;
      }
      try {
        conn_cb->on_connect(local_addr, peer_addr, conn);
      } catch (std::runtime_error &e) {
        return string_dup(e.what());
      } catch (...) {
        return string_dup("unknown error");
      }
      return nullptr;
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
      } catch (std::runtime_error &e) {
        return string_dup(e.what());
      } catch (...) {
        return string_dup("unknown error");
      }
      return nullptr;
    }
    case ConnStateCode::ListenError: {
      auto listen_error = states.Data.OnListenError;
      auto addr = std::string(listen_error.Addr);
      auto err = std::string(listen_error.Err);
      try {
        conn_cb->on_listen_error(addr, err);
      } catch (std::runtime_error &e) {
        return string_dup(e.what());
      } catch (...) {
        return string_dup("unknown error");
      }
      return nullptr;
    }
    case ConnStateCode::ConnectError: {
      auto connect_error = states.Data.OnConnectError;
      auto addr = std::string(connect_error.Addr);
      auto err = std::string(connect_error.Err);
      try {
        conn_cb->on_connect_error(addr, err);
      } catch (std::runtime_error &e) {
        return string_dup(e.what());
      } catch (...) {
        return string_dup("unknown error");
      }
      return nullptr;
    }
    default: {
      // should never reach here
      return nullptr;
    }
  }
}
