#include <eslang/Context.h>
#include <folly/io/IOBuf.h>

namespace s {

class Tcp {
public:
  struct ListenerOptions {
    ListenerOptions(uint32_t port) : port(port) {}
    uint32_t port;
  };
  struct Socket {
    explicit Socket(Pid p) : pid(std::move(p)) {}
    Pid pid;
  };
  struct ReceiveData {
    explicit ReceiveData(Pid p) : sender(p) {}
    Pid sender;
    std::unique_ptr<folly::IOBuf> data;
  };
  static Pid makeListener(Process* parent,
                          TSendAddress<Socket> new_socket_address,
                          ListenerOptions options);

  static void initRecvSocket(Process* sender, Socket socket,
                             TSendAddress<ReceiveData> new_socket_address);

  static void send(Process* sender, Socket socket,
                   std::unique_ptr<folly::IOBuf> data);
};
}