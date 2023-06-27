#ifndef SOCKET_MANAGER_C_API_H
#define SOCKET_MANAGER_C_API_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum ConnStateCode {
  Connect = 0,
  ConnectionClose = 1,
  ListenError = 2,
  ConnectError = 3,
} ConnStateCode;

typedef struct CConnection CConnection;

typedef struct CMsgSender CMsgSender;

/**
 * The Main Struct of the Library
 */
typedef struct CSocketManager CSocketManager;

/**
 * The data pointer is only valid for the duration of the callback.
 */
typedef struct ConnMsg {
  unsigned long long ConnId;
  const char *Bytes;
  size_t Len;
} ConnMsg;

/**
 * Callback function for receiving messages.
 *
 * `callback_self` is feed to the first argument of the callback.
 *
 * # Thread Safety
 * Must be thread safe!
 */
typedef struct OnMsgCallback {
  void *CallbackSelf;
  void (*Callback)(void*, struct ConnMsg);
} OnMsgCallback;

typedef struct OnConnect {
  unsigned long long ConnId;
  const char *Local;
  const char *Peer;
  struct CConnection *Conn;
} OnConnect;

typedef struct OnConnectionClose {
  unsigned long long ConnId;
} OnConnectionClose;

typedef struct OnListenError {
  const char *Addr;
  const char *Err;
} OnListenError;

typedef struct OnConnectError {
  const char *Addr;
  const char *Err;
} OnConnectError;

typedef union ConnStateData {
  struct OnConnect OnConnect;
  struct OnConnectionClose OnConnectionClose;
  struct OnListenError OnListenError;
  struct OnConnectError OnConnectError;
} ConnStateData;

/**
 * All data is only valid for the duration of the callback
 * except for the `sender` field of `OnConnect`.
 *
 * Do not manually free any of the data except `sender`!!
 */
typedef struct ConnStates {
  enum ConnStateCode Code;
  union ConnStateData Data;
} ConnStates;

/**
 * Callback function for connection state changes.
 *
 * `callback_self` is feed to the first argument of the callback.
 *
 * # Thread Safety
 * Must be thread safe!
 */
typedef struct OnConnCallback {
  void *CallbackSelf;
  void (*Callback)(void*, struct ConnStates);
} OnConnCallback;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Start a connection with the given `OnMsgCallback`, and return a pointer to a `CMsgSender`.
 *
 * This function can only be called once per `CConnection`,
 * otherwise it returns error.
 *
 * # Safety
 * The passed in callback must live as long as the connection is not closed !!
 *
 * # Arguments
 * * `conn` - A pointer to a `CConnection`.
 * * `on_msg` - A callback function that will be called when a message is received.
 * * `err` - A pointer to a pointer to a C string allocated by `malloc` on error.
 *
 * # Returns
 * A pointer to a `CMsgSender` on success, null on error.
 *
 * # Errors
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`,
 * and the returned pointer will be null.
 */
struct CMsgSender *connection_start(struct CConnection *conn,
                                    struct OnMsgCallback on_msg,
                                    char **err);

/**
 * Destructor of `Connection`.
 */
void connection_free(struct CConnection *conn);

/**
 * Send a message via the given `CMsgSender`.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int msg_sender_send(const struct CMsgSender *sender, const char *msg, size_t len, char **err);

/**
 * Destructor of `MsgSender`.
 */
void msg_sender_free(struct CMsgSender *sender);

/**
 * Initialize a new `SocketManager` and return a pointer to it.
 *
 * # Number of workers
 * If `n_threads` is 0, the number of workers will be set to the number of logical cores.
 * If `n_threads` is 1, uses single-threaded runtime.
 * `n_threads` is capped at 256.
 *
 * # connection callback
 * `on_conn_self` is passed to the callback function `on_conn` as the first argument.
 *
 * # Safety
 * The passed in callback pointers must live as long as the `SocketManager` does.
 *
 * # Thread Safety
 * Must ensure that the callback functions of `callback_obj` is thread safe! (i.e., synced).
 *
 * # Errors
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`,
 * and the returned pointer will be null.
 */
struct CSocketManager *socket_manager_init(struct OnConnCallback on_conn,
                                           size_t n_threads,
                                           char **err);

/**
 * Listen on the given address.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_listen_on_addr(struct CSocketManager *manager, const char *addr, char **err);

/**
 * Connect to the given address.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_connect_to_addr(struct CSocketManager *manager, const char *addr, char **err);

/**
 * Cancel listening on the given address.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_cancel_listen_on_addr(struct CSocketManager *manager,
                                         const char *addr,
                                         char **err);

/**
 * Cancel (abort) a connection.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_cancel_connection(struct CSocketManager *manager,
                                     unsigned long long id,
                                     char **err);

/**
 * Detach the `SocketManager`'s background runtime.
 */
int socket_manager_detach(struct CSocketManager *manager, char **err);

/**
 * Join and wait on the `SocketManager`.
 */
int socket_manager_join(struct CSocketManager *manager, char **err);

/**
 * Destroy a `SocketManager` and free its memory.
 */
void socket_manager_free(struct CSocketManager *manager);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif /* SOCKET_MANAGER_C_API_H */
