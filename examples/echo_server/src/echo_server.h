#ifndef ECHO_SERVER_ECHO_SERVER_H
#define ECHO_SERVER_ECHO_SERVER_H

#include <socket_manager.h>
#include <variant>
#include <memory>
#include <concurrentqueue.h>
#include <lightweightsemaphore.h>

template<class... Ts>
struct overloaded : Ts ... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

struct ConnClose {
  std::string local;
  std::string peer;
};

struct ConnErr {
  std::string addr;
  std::string err;
};

struct ListenErr {
  std::string addr;
  std::string err;
};

typedef std::variant<
        ConnClose,
        ConnErr,
        ListenErr
> CB;

#endif //ECHO_SERVER_ECHO_SERVER_H
