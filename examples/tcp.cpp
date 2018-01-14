#include <eslang/Context.h>
#include <eslang/Logging.h>
#include <eslang_io/Tcp.h>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>

namespace s {

class TcpEchoRunner : public Process {
public:
  Tcp::Socket s_;
  TcpEchoRunner(ProcessArgs i, Tcp::Socket s)
    : Process(std::move(i)), s_(std::move(s)) {
    link(s.pid);
  }

  Slot<Tcp::ReceiveData> recv{ this };
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
  Slot<Tcp::Socket> new_socket{ this };
  Tcp::ListenerOptions options_{ 0 };
  TcpEchoServer(ProcessArgs i, uint32_t p) : Process(std::move(i)), options_(p) {}

  ProcessTask run() {
    ESLOG(LL::DEBUG, "Start echo at ", pid());

    s::Tcp::makeListener(this, new_socket.address(), options_);
    while (true) {
      auto new_sock = co_await recv(new_socket);
      ESLOG(LL::DEBUG, "New SOCK", new_sock.pid);
      spawn<TcpEchoRunner>(std::move(new_sock));
    }
  }
};
}

int main(int argc, char** argv) {
  s::Context c;
  c.spawn<s::TcpEchoServer>(25123);
  c.run();
  boost::log::core::get()->flush();
  return 0;
}
