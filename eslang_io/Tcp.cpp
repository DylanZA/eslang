#include "Tcp.h"
#include <folly/io/async/AsyncServerSocket.h>
#include <folly/io/async/AsyncSocket.h>

namespace s {

struct SocketProcess : public Process,
                       public folly::AsyncSocket::ReadCallback,
                       public folly::AsyncSocket::WriteCallback {
  int const fd;
  bool eof_ = false;

  folly::Promise<folly::Unit> p_;
  Slot<std::unique_ptr<folly::IOBuf>> send_data{this};
  Slot<TSendAddress<Tcp::ReceiveData>> init{this};

  std::optional<TSendAddress<Tcp::ReceiveData>> to_send;

  SocketProcess(ProcessArgs i, int fd) : Process(std::move(i)), fd(fd) {
    VLOG(3) << "Socket " << fd << " created";
  }

  ~SocketProcess() { VLOG(3) << "~Socket " << fd; }

  char buff[16000];
  void getReadBuffer(void** bufReturn, size_t* lenReturn) override {
    *bufReturn = &buff;
    *lenReturn = sizeof(buff);
  }

  void readDataAvailable(size_t len) noexcept override {
    VLOG(3) << "Read " << len;
    if (len > 0 && to_send) {
      // doesnt really work, if we have multiple calls
      Tcp::ReceiveData r(pid());
      r.data = folly::IOBuf::copyBuffer(&buff, len);
      send(*to_send, std::move(r));
    }
  }

  bool isBufferMovable() noexcept override { return false; }

  size_t maxBufferSize() const override {
    return 64 * 1024; // 64K
  }

  void
  readBufferAvailable(std::unique_ptr<folly::IOBuf> readBuf) noexcept override {
    ESLANGEXCEPT("Unexpected");
  }

  void readEOF() noexcept override {
    eof_ = true;
    VLOG(3) << "EOF";
    p_.setValue();
  }

  void readErr(const folly::AsyncSocketException& ex) noexcept override {
    p_.setException(ex);
  }

  void writeSuccess() noexcept override {
    // do we care?
  }

  void writeErr(size_t bytesWritten,
                const folly::AsyncSocketException& ex) noexcept override {
    p_.setException(ex);
  }

  ProcessTask run() {
    folly::AsyncSocket::UniquePtr socket(
        new folly::AsyncSocket(c_->eventBase(), fd));
    to_send = co_await recv(init);
    socket->setReadCB(this);

    while (!eof_) {
      p_ = folly::Promise<folly::Unit>{};
      auto ret =
          co_await makeWithWaitingFuture(p_.getFuture(), tryRecv(send_data));
      if (std::get<0>(ret)) {
        socket->writeChain(this, std::move(std::get<0>(ret).value()));
      }
    }
  }
};

struct ListenerProcess : public Process,
                         public folly::AsyncServerSocket::AcceptCallback {
  TSendAddress<Tcp::Socket> newSocket;
  uint32_t port;
  folly::Promise<folly::Unit> error_;
  ListenerProcess(ProcessArgs i, TSendAddress<Tcp::Socket> new_socket_address,
                  uint32_t port)
      : Process(std::move(i)), newSocket(std::move(new_socket_address)),
        port(port) {}

  void acceptError(const std::exception& ex) noexcept override {
    LOG(ERROR) << "Error " << ex.what();
    error_.setException(ex);
  }

  void
  connectionAccepted(int fd,
                     const folly::SocketAddress& clientAddr) noexcept override {
    VLOG(3) << "New connection " << fd << " from " << clientAddr.describe();
    send(newSocket, Tcp::Socket(spawn<SocketProcess>(fd)));
  }

  ProcessTask run() {
    folly::AsyncServerSocket::UniquePtr socket(
        new folly::AsyncServerSocket(c_->eventBase()));
    socket->bind({folly::IPAddress("127.0.0.1")}, port);
    socket->listen(10000);
    socket->addAcceptCallback(this, c_->eventBase());
    VLOG(2) << "Listening on " << socket->getAddress().describe();
    socket->startAccepting();
    co_await WaitOnFuture(error_.getFuture());
  }
};

Pid Tcp::makeListener(Process* parent, TSendAddress<Socket> new_socket_address,
                      Tcp::ListenerOptions options) {
  return parent->spawnLink<ListenerProcess>(std::move(new_socket_address),
                                            options.port);
}

void Tcp::initRecvSocket(Process* sender, Socket socket,
                         TSendAddress<ReceiveData> new_socket_address) {
  VLOG(2) << "Init " << socket.pid;
  sender->send(socket.pid, &SocketProcess::init, std::move(new_socket_address));
}

void Tcp::send(Process* sender, Socket socket,
               std::unique_ptr<folly::IOBuf> data) {
  sender->send(socket.pid, &SocketProcess::send_data, std::move(data));
}
}