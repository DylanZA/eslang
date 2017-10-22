#include "Tcp.h"
#include <boost/asio.hpp>
#include <numeric>

namespace s {
using namespace boost::asio;
struct SocketProcess : public Process {
  ip::tcp::socket socket;
  bool eof_ = false;

  EslangPromise p_;
  std::optional<folly::exception_wrapper> except_;
  Slot<Buffer> send_data{this};
  Slot<BufferCollection> send_many_data{this};
  Slot<TSendAddress<Tcp::ReceiveData>> init{this};

  std::optional<TSendAddress<Tcp::ReceiveData>> to_send;

  SocketProcess(ProcessArgs i, ip::tcp::socket socket)
      : Process(std::move(i)), socket(std::move(socket)) {
    VLOG(3) << "Socket " << socket.native_handle() << " created";
  }

  ~SocketProcess() {
    VLOG(3) << pid() << ": ~SocketProcess fd=" << socket.native_handle();
  }

  void checkExcept() {
    if (except_) {
      except_->throw_exception();
    }
  }

  void setValue() { p_.setIfUnset(); }

  void setError(boost::system::error_code const& ec) {
    setException(std::runtime_error(
        folly::to<std::string>("Listener threw ", ec.message())));
  }

  template <class T> void setException(T const& e) {
    setValue();
    if (!except_) {
      except_ = folly::make_exception_wrapper<T>(e);
    }
  }

  char readBuff[16000];
  void asyncRead() {
    socket.async_read_some(
        buffer(readBuff, sizeof(readBuff)),
        [this](const boost::system::error_code& error, std::size_t bytes) {
          if (error == error::operation_aborted) {
            return;
          }
          if (error == error::eof) {
            eof_ = true;
            VLOG(3) << pid() << ": EOF";
            setValue();
            return;
          }
          if (error) {
            setError(error);
            return;
          }
          if (bytes > 0) {
            send(*to_send,
                 Tcp::ReceiveData(pid(), Buffer::makeCopy(&readBuff, bytes)));
          }
          asyncRead();
        });
  }

  void write(Buffer buff) {
    async_write(socket, buffer(buff.data(), buff.size()),
                [buff, this](const boost::system::error_code& ec,
                             std::size_t bytes_transferred) {
                  if (ec == error::operation_aborted) {
                    return;
                  }
                  if (ec) {
                    setError(ec);
                  }
                });
  }

  ProcessTask run() {
    to_send = co_await recv(init);
    asyncRead();
    while (!eof_) {
      auto ret = co_await makeWithWaitingFuture(
          &p_, tryRecv(send_data, send_many_data));
      checkExcept();
      if (std::get<0>(ret)) {
        write(std::get<0>(ret).value());
      } else if (std::get<1>(ret)) {
        // windows doesn't do chains well unfortunately, one day we should use
        // the chaining
        write(std::get<1>(ret).value().combine());
      }
    }
  }
};

struct ListenerProcess : public Process {
  TSendAddress<Tcp::Socket> newSocket;
  uint32_t port;
  EslangPromise error_;
  ip::tcp const protocol = ip::tcp::v4();
  ListenerProcess(ProcessArgs i, TSendAddress<Tcp::Socket> new_socket_address,
                  uint32_t port)
      : Process(std::move(i)), newSocket(std::move(new_socket_address)),
        port(port) {}
  void setException(boost::system::error_code const& ec) {
    error_.setException(std::runtime_error(
        folly::to<std::string>("Listener threw ", ec.message())));
  }

  ip::tcp::socket next{c_->ioService()};
  void asyncAccept(ip::tcp::acceptor& socket) {
    socket.async_accept(
        next, [this, &socket](const boost::system::error_code& ec) {
          if (ec == error::operation_aborted) {
            return;
          }
          if (ec) {
            setException(ec);
          } else {
            send(newSocket, Tcp::Socket(spawn<SocketProcess>(std::move(next))));
            next = ip::tcp::socket(c_->ioService());
            asyncAccept(socket);
          }
        });
  }
  ProcessTask run() {
    ip::tcp::acceptor socket(c_->ioService());
    LOG(INFO) << "Bind to " << port;
    socket.open(protocol);
    socket.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    socket.bind(ip::tcp::endpoint(protocol, port));
    socket.listen(10000);
    asyncAccept(socket);
    VLOG(2) << "Listening on " << socket.local_endpoint();
    co_await WaitOnFuture(&error_);
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

void Tcp::send(Process* sender, Socket socket, Buffer data) {
  sender->send(socket.pid, &SocketProcess::send_data, std::move(data));
}

void Tcp::sendMany(Process* sender, Socket socket, BufferCollection data) {
  sender->send(socket.pid, &SocketProcess::send_many_data, std::move(data));
}
}