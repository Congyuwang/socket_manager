#ifndef SOCKET_MANAGER_CONNECTION_H
#define SOCKET_MANAGER_CONNECTION_H

#include <atomic>
#include "msg_receiver.h"
#include "msg_sender.h"
#include "socket_manager_c_api.h"

namespace socket_manager {

  /**
   * Use Connection to send and receive messages from
   * established connections.
   */
  class Connection {

  public:

    /**
     * Start a connection.
     *
     * This function can only be called once,
     * otherwise it will throw runtime_error.
     *
     * To close the connection, drop the returned
     * MsgSender object.
     *
     * @param msg_receiver the message receiver callback to
     *                    receive messages from the peer.
     */
    std::shared_ptr<MsgSender> start(std::unique_ptr<MsgReceiver> msg_receiver);

    ~Connection();

  private:

    friend class SocketManager;

    // keep the msg_receiver alive
    std::unique_ptr<MsgReceiver> receiver;
    std::atomic_bool started;

    explicit Connection(CConnection *inner);

    CConnection *inner;

  };

} // namespace socket_manager

#endif //SOCKET_MANAGER_CONNECTION_H
