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

/**
 * Drop the sender to close the connection.
 */
typedef struct CMsgSender CMsgSender;

/**
 * The Main Struct of the Library.
 *
 * This struct is thread safe.
 */
typedef struct CSocketManager CSocketManager;

/**
 * The data pointer is only valid for the duration of the callback.
 */
typedef struct ConnMsg {
  const char *Bytes;
  size_t Len;
} ConnMsg;

/**
 * Callback function for receiving messages.
 *
 * `callback_self` is feed to the first argument of the callback.
 *
 * # Safety
 * The callback pointer must be valid before connection is closed!!
 *
 * # Thread Safety
 * Must be thread safe!
 */
typedef struct OnMsgCallback {
  void *CallbackSelf;
  void (*Callback)(void*, struct ConnMsg);
} OnMsgCallback;

typedef struct OnConnect {
  const char *Local;
  const char *Peer;
  struct CConnection *Conn;
} OnConnect;

typedef struct OnConnectionClose {
  const char *Local;
  const char *Peer;
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
 * # Safety
 * The callback pointer must be valid for the entire runtime lifetime!!
 * (i.e., before the runtime is aborted and joined).
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
 * The `start` function must be called exactly once.
 * Calling it twice will result in runtime error.
 * Not calling it will result in resource leak
 * (i.e. the connection will likely hangs).
 *
 * # Safety
 * The passed in callback must live as long as the connection is not closed !!
 *
 * # ThreadSafety
 * Thread safe, but should be called exactly once.
 *
 * # Arguments
 * * `conn` - A pointer to a `CConnection`.
 * * `on_msg` - A callback function that will be called when a message is received.
 * * `write_flush_interval` - The interval in `milliseconds` of write buffer auto flushing.
 *    Set to 0 to disable auto flush.
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
                                    unsigned long long write_flush_interval,
                                    char **err);

/**
 * Destructor of `Connection`.
 */
void connection_free(struct CConnection *conn);

/**
 * Send a message via the given `CMsgSender`.
 *
 * # Thread Safety
 * Thread safe.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int msg_sender_send(const struct CMsgSender *sender, const char *msg, size_t len, char **err);

/**
 * Manually flush the message sender.
 *
 * # Thread Safety
 * Thread safe.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int msg_sender_flush(const struct CMsgSender *sender, char **err);

/**
 * Destructor of `MsgSender`.
 * Drop sender to actively close the connection.
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
 * # ThreadSafety
 * Thread safe.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_listen_on_addr(struct CSocketManager *manager, const char *addr, char **err);

/**
 * Connect to the given address.
 *
 * # Thread Safety
 * Thread safe.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_connect_to_addr(struct CSocketManager *manager, const char *addr, char **err);

/**
 * Cancel listening on the given address.
 *
 * # Thread Safety
 * Thread safe.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_cancel_listen_on_addr(struct CSocketManager *manager,
                                         const char *addr,
                                         char **err);

/**
 * Abort the `SocketManager`'s background runtime.
 *
 * # Thread Safety
 * Thread safe.
 *
 * Does not wait for the runtime to finish.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_abort(struct CSocketManager *manager, char **err);

/**
 * Join and wait on the `SocketManager`.
 *
 * # Thread Safety
 * Thread safe. But should be called no more than once,
 * otherwise throws runtime error.
 *
 * This function will block until the `SocketManager`'s background runtime finishes,
 * (i.e., `abort` is called from another thread).
 */
int socket_manager_join(struct CSocketManager *manager, char **err);

/**
 * Calling this function will abort all background runtime and join on them,
 * and free the `SocketManager`.
 */
void socket_manager_free(struct CSocketManager *manager);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif /* SOCKET_MANAGER_C_API_H */
