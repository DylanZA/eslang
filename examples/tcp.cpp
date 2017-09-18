#include <eslang/Context.h>
#include <eslang_io/Tcp.h>
#include <folly/Conv.h>
#include <folly/init/Init.h>
#include <glog/logging.h>

namespace s {

class TcpEchoRunner : public Process {
public:
  Tcp::Socket s_;
  TcpEchoRunner(ProcessArgs i, Tcp::Socket s)
      : Process(std::move(i)), s_(std::move(s)) {
    link(s.pid);
  }

  Slot<Tcp::ReceiveData> recv{this};
  ProcessTask run() {
    Tcp::initRecvSocket(this, s_, recv.address());
    while (true) {
      auto r = co_await Process::recv(recv);
      Tcp::send(this, s_, std::move(r.data));
    }
  }
};

class TcpEchoServer : public Process {
public:
  using Process::Process;
  Slot<Tcp::Socket> new_socket{this};
  uint32_t port_ = 0;
  TcpEchoServer(ProcessArgs i, uint32_t p) : Process(std::move(i)), port_(p) {}

  ProcessTask run() {
    s::Tcp::makeListener(this, new_socket.address(), port_);
    while (true) {
      auto new_sock = co_await recv(new_socket);
      VLOG(2) << "New SOCK" << new_sock.pid;
      spawn<TcpEchoRunner>(std::move(new_sock));
    }
  }
};
}

int main(int argc, char** argv) {
  FLAGS_stderrthreshold = 0;
  FLAGS_v = 3;
  folly::init(&argc, &argv);
  s::Context c;
  c.spawn<s::TcpEchoServer>(25123);
  c.run();
  return 0;
}
