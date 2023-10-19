#ifndef SOCKET_MANAGER_C_API_H
#define SOCKET_MANAGER_C_API_H

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

enum class SOCKET_MANAGER_C_API_ConnStateCode {
  Connect = 0,
  ConnectionClose = 1,
  ListenError = 2,
  ConnectError = 3,
};

/**
 * Trace Level
 */
enum class SOCKET_MANAGER_C_API_TraceLevel {
  /**
   * The "trace" level.
   *
   * Designates very low priority, often extremely verbose, information.
   */
  Trace = 0,
  /**
   * The "debug" level.
   *
   * Designates lower priority information.
   */
  Debug = 1,
  /**
   * The "info" level.
   *
   * Designates useful information.
   */
  Info = 2,
  /**
   * The "warn" level.
   *
   * Designates hazardous situations.
   */
  Warn = 3,
  /**
   * The "error" level.
   *
   * Designates very serious errors.
   */
  Error = 4,
  /**
   * Turn off all levels.
   *
   * Disable log output.
   */
  Off = 5,
};

struct SOCKET_MANAGER_C_API_Connection;

/**
 * Drop the sender to close the connection.
 */
struct SOCKET_MANAGER_C_API_MsgSender;

/**
 * The Main Struct of the Library.
 *
 * This struct is thread safe.
 */
struct SOCKET_MANAGER_C_API_SocketManager;

/**
 * The Notifier is constructed by the c/c++ code,
 * and passed to the rust code.
 *
 * # Task Resume
 * When `wake` callback is called by rust, the c/c++ task
 * should resume its execution.
 *
 * # Lifetime Management.
 * The Notifier has `clone` and `release` callbacks.
 * Say a Notifier start with ref_count = 1,
 * and when `clone` is called, increment its ref_count,
 * and when `release` is called, decrement its ref_count.
 *
 * The notifier can be released when its ref_count falls back to 1.
 *
 * The c/c++ code must carefully manage the lifetime of the waker.
 * to ensure that the waker is not dropped before the rust code
 * is done with it.
 */
struct SOCKET_MANAGER_C_API_Notifier {
  void *This;
};

/**
 * # Safety
 * Do not use this struct directly.
 * Properly wrap it in c++ class.
 *
 * This struct is equivalent to a raw pointer.
 * Manager with care.
 *
 * Note that the CWaker must be properly dropped.
 * Otherwise, the associated task will leak.
 */
struct SOCKET_MANAGER_C_API_CWaker {
  const void *Data;
  const void *Vtable;
};

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
 * # Non-blocking
 * Must be non-blocking!
 */
struct SOCKET_MANAGER_C_API_OnMsgObj {
  void *This;
};

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
struct SOCKET_MANAGER_C_API_OnConnObj {
  void *This;
};

struct SOCKET_MANAGER_C_API_OnConnect {
  const char *Local;
  const char *Peer;
  SOCKET_MANAGER_C_API_MsgSender *Send;
  SOCKET_MANAGER_C_API_Connection *Conn;
};

struct SOCKET_MANAGER_C_API_OnConnectionClose {
  const char *Local;
  const char *Peer;
};

struct SOCKET_MANAGER_C_API_OnListenError {
  const char *Addr;
  const char *Err;
};

struct SOCKET_MANAGER_C_API_OnConnectError {
  const char *Addr;
  const char *Err;
};

union SOCKET_MANAGER_C_API_ConnStateData {
  SOCKET_MANAGER_C_API_OnConnect OnConnect;
  SOCKET_MANAGER_C_API_OnConnectionClose OnConnectionClose;
  SOCKET_MANAGER_C_API_OnListenError OnListenError;
  SOCKET_MANAGER_C_API_OnConnectError OnConnectError;
};

/**
 * All data is only valid for the duration of the callback
 * except for the `sender` field of `OnConnect`.
 *
 * Do not manually free any of the data except `sender`!!
 */
struct SOCKET_MANAGER_C_API_ConnStates {
  SOCKET_MANAGER_C_API_ConnStateCode Code;
  SOCKET_MANAGER_C_API_ConnStateData Data;
};

/**
 * The data pointer is only valid for the duration of the callback.
 */
struct SOCKET_MANAGER_C_API_ConnMsg {
  const char *Bytes;
  size_t Len;
};

/**
 * Log Data
 */
struct SOCKET_MANAGER_C_API_LogData {
  SOCKET_MANAGER_C_API_TraceLevel Level;
  const char *Target;
  size_t TargetN;
  /**
   * The `message` pointer is only valid for the duration of the callback.
   */
  const char *Message;
  size_t MessageN;
};

extern "C" {

/**
 * Waker for the try_send method.
 */
extern void socket_manager_extern_notifier_wake(SOCKET_MANAGER_C_API_Notifier this_);

/**
 * Call the waker to wake the relevant task of context.
 */
void socket_manager_waker_wake(const SOCKET_MANAGER_C_API_CWaker *waker);

/**
 * Release the waker.
 */
void socket_manager_waker_free(SOCKET_MANAGER_C_API_CWaker waker);

/**
 * Start a connection with the given `OnMsgCallback`, and return a pointer to a `MsgSender`.
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
 * Returns 1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`,
 */
int socket_manager_connection_start(SOCKET_MANAGER_C_API_Connection *conn,
                                    SOCKET_MANAGER_C_API_OnMsgObj on_msg,
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
 * Returns 1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_connection_close(SOCKET_MANAGER_C_API_Connection *conn, char **err);

/**
 * Destructor of `Connection`.
 */
void socket_manager_connection_free(SOCKET_MANAGER_C_API_Connection *conn);

/**
 * Send a message via the given `MsgSender` synchronously.
 * This is a blocking API.
 *
 * # Thread Safety
 * Thread safe.
 *
 * This function should never be called within the context of the async callbacks
 * since it might block.
 *
 * # Errors
 * If the connection is closed, the function will return 1 and set `err` to a pointer
 * with WriteZero error.
 *
 * Returns 1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_msg_sender_send_block(SOCKET_MANAGER_C_API_MsgSender *sender,
                                         const char *msg,
                                         size_t len,
                                         char **err);

/**
 * Send a message via the given `MsgSender` .
 * This is a non-blocking API.
 * All sent data is buffered in a chain of ring buffer.
 * This method does not implement back pressure since it
 * caches all received data.
 * Use `send_async` or `send_block` for back pressure.
 *
 * # Thread Safety
 * Thread safe.
 *
 * This function can be called within the context of the async callbacks.
 *
 * # Errors
 * If the connection is closed, the function will return 1 and set `err` to a pointer
 * with WriteZero error.
 *
 * Returns 1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_msg_sender_send_nonblock(SOCKET_MANAGER_C_API_MsgSender *sender,
                                            const char *msg,
                                            size_t len,
                                            char **err);

/**
 * Try to send a message via the given `MsgSender` asynchronously.
 *
 * # Thread Safety
 * Thread safe.
 *
 * # Async control flow (IMPORTANT)
 *
 * This function is non-blocking, it returns `PENDING = -1`
 * if the send buffer is full. So the caller should wait
 * by passing a `Notifier` which will be called when the
 * buffer is ready.
 *
 * When the buffer is ready, the function returns number of bytes sent.
 *
 * # Errors
 * Use `err` pointer to check for error.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
long socket_manager_msg_sender_send_async(SOCKET_MANAGER_C_API_MsgSender *sender,
                                          const char *msg,
                                          size_t len,
                                          SOCKET_MANAGER_C_API_Notifier notifier,
                                          char **err);

/**
 * Manually flush the message sender.
 *
 * # Thread Safety
 * Thread safe.
 *
 * # Errors
 * Returns 1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_msg_sender_flush(SOCKET_MANAGER_C_API_MsgSender *sender, char **err);

/**
 * Destructor of `MsgSender`.
 * Drop sender to actively close the connection.
 */
void socket_manager_msg_sender_free(SOCKET_MANAGER_C_API_MsgSender *sender);

/**
 * Rust calls this function to send `conn: ConnStates`
 * to the `this: OnConnObj`. If the process has any error,
 * pass error to `err` pointer.
 * Set `err` to null_ptr if there is no error.
 */
extern void socket_manager_extern_on_conn(SOCKET_MANAGER_C_API_OnConnObj this_,
                                          SOCKET_MANAGER_C_API_ConnStates states,
                                          char **err);

/**
 * Rust calls this function to send `msg: ConnMsg`
 * to `OnMsgObj`. If the process has any error,
 * pass error to `err` pointer.
 * Set `err` to null_ptr if there is no error.
 *
 * # Async control flow (IMPORTANT)
 *
 * The caller should return the exact number of bytes written
 * to the runtime if some bytes are written. The runtime
 * will increment the read offset accordingly.
 *
 * If the caller is unable to receive any bytes,
 * it should return `PENDING = -1` to the runtime
 * to interrupt message receiving task. The read offset
 * will not be incremented.
 *
 * When the caller is able to receive bytes again,
 * it should call `waker.wake()` to wake up the runtime.
 */
extern long socket_manager_extern_on_msg(SOCKET_MANAGER_C_API_OnMsgObj this_,
                                         SOCKET_MANAGER_C_API_ConnMsg msg,
                                         SOCKET_MANAGER_C_API_CWaker waker,
                                         char **err);

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
 * # Non-blocking
 * Must ensure that the callback functions of `callback_obj` are non-blocking.
 *
 * # Errors
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`,
 * and the returned pointer will be null.
 */
SOCKET_MANAGER_C_API_SocketManager *socket_manager_init(SOCKET_MANAGER_C_API_OnConnObj on_conn,
                                                        size_t n_threads,
                                                        char **err);

/**
 * Listen on the given address.
 *
 * # ThreadSafety
 * Thread safe.
 *
 * # Errors
 * Returns 1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_listen_on_addr(SOCKET_MANAGER_C_API_SocketManager *manager,
                                  const char *addr,
                                  char **err);

/**
 * Connect to the given address.
 *
 * # Arguments
 * - `delay`: delay in milliseconds before connecting.
 *
 * # Thread Safety
 * Thread safe.
 *
 * # Errors
 * Returns 1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_connect_to_addr(SOCKET_MANAGER_C_API_SocketManager *manager,
                                   const char *addr,
                                   uint64_t delay,
                                   char **err);

/**
 * Cancel listening on the given address.
 *
 * # Thread Safety
 * Thread safe.
 *
 * # Errors
 * Returns 1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_cancel_listen_on_addr(SOCKET_MANAGER_C_API_SocketManager *manager,
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
 * Returns 1 on error, 0 on success.
 * On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
 */
int socket_manager_abort(SOCKET_MANAGER_C_API_SocketManager *manager, bool wait, char **err);

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
int socket_manager_join(SOCKET_MANAGER_C_API_SocketManager *manager, char **err);

/**
 * Calling this function will abort all background runtime and join on them,
 * and free the `SocketManager`.
 */
void socket_manager_free(SOCKET_MANAGER_C_API_SocketManager *manager);

/**
 * Init logger.
 *
 * # Arguments
 * - `tracer`: The tracer object.
 * - `tracer_max_level`: The max level of the tracer.
 * - `log_print_level`: The level of the log to print.
 */
void socket_manager_logger_init(void (*tracer)(SOCKET_MANAGER_C_API_LogData),
                                SOCKET_MANAGER_C_API_TraceLevel tracer_max_level,
                                SOCKET_MANAGER_C_API_TraceLevel log_print_level,
                                char **err);

} // extern "C"

#endif // SOCKET_MANAGER_C_API_H
