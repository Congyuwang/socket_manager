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
 * Callback function for receiving messages.
 *
 * `callback_self` is feed to the first argument of the callback.
 *
 * # Error Handling
 * Returns null_ptr on success, otherwise returns a pointer to a malloced
 * C string containing the error message (the c string should be freed by the
 * caller).
 *
 * # Safety
 * The callback pointer must be valid before connection is closed!!
 *
 * # Thread Safety
 * Must be thread safe!
 */
typedef struct OnMsgObj {
  void *This;
} OnMsgObj;

/**
 * The data pointer is only valid for the duration of the callback.
 */
typedef struct ConnMsg {
  const char *Bytes;
  size_t Len;
} ConnMsg;

/**
 * Do not use this struct directly.
 * Properly wrap it in c++ code.
 *
 * # Safety
 * This struct is equivalent to a raw pointer.
 * Manager with care.
 */
typedef struct CWaker {
  const void *Data;
  const void *Vtable;
} CWaker;

/**
 * Callback function for connection state changes.
 *
 * `callback_self` is feed to the first argument of the callback.
 *
 * # Error Handling
 * Returns null_ptr on success, otherwise returns a pointer to a malloced
 * C string containing the error message (the c string should be freed by the
 * caller).
 *
 * # Safety
 * The callback pointer must be valid for the entire runtime lifetime!!
 * (i.e., before the runtime is aborted and joined).
 *
 * # Thread Safety
 * Must be thread safe!
 */
typedef struct OnConnObj {
  void *This;
} OnConnObj;

typedef struct OnConnect {
  const char *Local;
  const char *Peer;
  struct CMsgSender *Send;
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
 * Send the msg sender obj to receive
 * writable notification.
 */
typedef struct WakerObj {
  void *This;
} WakerObj;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Start a connection with the given `OnMsgCallback`, and return a pointer to a `CMsgSender`.
 *
 * Only one of `connection_start` or `connection_close` should be called,
 * or it will result in runtime error.
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
 * * `msg_buffer_size` - The size of the message buffer in bytes.
 *    Set to 0 to use no buffer (i.e., call `on_msg` immediately on receiving
 *    any data). The minimum is 8KB, and the maximum is 8MB.
 * * `read_msg_flush_interval` - The interval in `milliseconds` of read message buffer
 *    auto flushing. The value is ignored when `msg_buffer_size` is 0.
 *    Set to 0 to disable auto flush (which is not recommended since there is no
 *    manual flush, and small messages might get stuck in buffer).
 * * `write_flush_interval` - The interval in `milliseconds` of write buffer auto flushing.
 *    Set to 0 to disable auto flush.
 * * `err` - A pointer to a pointer to a C string allocated by `malloc` on error.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`,
 */
int connection_start(struct CConnection *conn,
                     struct OnMsgObj on_msg,
                     size_t msg_buffer_size,
                     unsigned long long read_msg_flush_interval,
                     unsigned long long write_flush_interval,
                     char **err);

/**
 * Close the connection without using it.
 *
 * Only one of `connection_start` or `connection_close` should be called,
 * or it will result in runtime error.
 *
 * # Thread Safety
 * Thread safe.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int connection_close(struct CConnection *conn, char **err);

/**
 * Destructor of `Connection`.
 */
void connection_free(struct CConnection *conn);

/**
 * Callback function for receiving messages.
 * Return positive number for `Ready`,
 * and negative number for `Pending`.
 */
extern long socket_manager_extern_on_msg(struct OnMsgObj this_,
                                         struct ConnMsg msg,
                                         struct CWaker waker,
                                         char **err);

/**
 * Callback function for connection state changes.
 */
extern void socket_manager_extern_on_conn(struct OnConnObj this_,
                                          struct ConnStates conn,
                                          char **err);

/**
 * Waker for the try_send method.
 */
extern void socket_manager_extern_sender_waker_wake(struct WakerObj this_);

/**
 * Decrement ref count of the waker.
 */
extern void socket_manager_extern_sender_waker_release(struct WakerObj this_);

/**
 * Increment ref count of the waker.
 */
extern void socket_manager_extern_sender_waker_clone(struct WakerObj this_);

/**
 * Send a message via the given `CMsgSender`.
 *
 * # Thread Safety
 * Thread safe.
 *
 * This function should never be called within the context of the async callbacks
 * since it might block.
 *
 * # Errors
 * If the connection is closed, the function will return -1 and set `err` to a pointer
 * with WriteZero error.
 *
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int msg_sender_send(struct CMsgSender *sender, const char *msg, size_t len, char **err);

/**
 * Try to send a message via the given `CMsgSender`.
 *
 * # Thread Safety
 * Thread safe.
 *
 * This function is non-blocking, pass the MsgSender class
 * to the waker_obj to receive notification to continue
 * sending the message.
 *
 * # Return
 * If waker is provided, returns the number of bytes sent on success,
 * and 0 on connection closed, -1 on pending.
 *
 * If waker is not provided, returns the number of bytes sent.
 * 0 might indicate the connection is closed, or the message buffer is full.
 *
 * # Errors
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
long msg_sender_try_send(struct CMsgSender *sender,
                         const char *msg,
                         size_t len,
                         struct WakerObj waker_obj,
                         char **err);

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
int msg_sender_flush(struct CMsgSender *sender, char **err);

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
struct CSocketManager *socket_manager_init(struct OnConnObj on_conn, size_t n_threads, char **err);

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
 * # Arguments
 * - `wait`: if true, wait for the background runtime to finish.
 *
 * # Errors
 * Returns -1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_abort(struct CSocketManager *manager, bool wait, char **err);

/**
 * Join and wait on the `SocketManager`.
 *
 * # Thread Safety
 * Thread safe. Calling a second time will return immediately.
 *
 * This function will block until the `SocketManager`'s background runtime finishes,
 * (i.e., `abort` is called from another thread).
 *
 * # Errors
 * Join returns error if the runtime panicked.
 */
int socket_manager_join(struct CSocketManager *manager, char **err);

/**
 * Calling this function will abort all background runtime and join on them,
 * and free the `SocketManager`.
 */
void socket_manager_free(struct CSocketManager *manager);

void msg_waker_wake(const struct CWaker *waker);

void msg_waker_free(struct CWaker waker);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif /* SOCKET_MANAGER_C_API_H */
