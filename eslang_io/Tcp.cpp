#include "Tcp.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <numeric>

#include <eslang/Logging.h>

namespace s {
using namespace boost::asio;

Tcp::ListenerOptions Tcp::ListenerOptions::withSslFiles(std::string ca,
                                                        std::string cert,
                                                        std::string key) const {
  auto r = *this;
  r.sslContextFactory = [ca, cert, key](boost::asio::io_service& svc) {
    auto ret = std::make_unique<ssl::context>(ssl::context::sslv23_server);
    ret->set_options(ssl::context::default_workarounds |
                     ssl::context::no_sslv2);
    ret->use_certificate_chain_file(ca);
    ret->use_certificate_file(cert, boost::asio::ssl::context::pem);
    ret->use_private_key_file(key, boost::asio::ssl::context::pem);
    return ret;
  };
  return r;
}

struct SocketProcess : public Process {
  Slot<Buffer> send_data{this};
  Slot<BufferCollection> send_many_data{this};
  Slot<TSendAddress<Tcp::ReceiveData>> init{this};
  using Process::Process;
};

struct SslSocketTraits : SocketProcess {
  // sigh - boost ssl socket is non-movable :(
  using Socket = std::unique_ptr<ssl::stream<ip::tcp::socket>>;
  SslSocketTraits(ProcessArgs i, Socket socket)
      : SocketProcess(std::move(i)), socket_(std::move(socket)) {}

  ssl::stream<ip::tcp::socket>& socket() { return *socket_; }
  auto toId() { return socket().native_handle(); }

  MethodTask<void> start() {
    EslangPromise p_;
    ESLOG(LL::TRACE, "Handshaking ", toId());
    socket().async_handshake(
        boost::asio::ssl::stream_base::server,
        [&](const boost::system::error_code& error) {
          if (error == error::operation_aborted) {
            return;
          }
          if (error) {
            ESLOG(LL::TRACE, "Handshaking threw ", toId());
            p_.setException(std::runtime_error(
                concatString("SSL handshake threw ", error.message())));
            return;
          }
          p_.setIfUnset();
        });
    co_await WaitOnFuture(&p_);
  }
  ~SslSocketTraits() {
    boost::system::error_code ec;
    socket().lowest_layer().cancel(ec);
    socket().lowest_layer().close(ec);
  }

private:
  Socket socket_;
};

struct PlainSocketTraits : SocketProcess {
public:
  using Socket = ip::tcp::socket;

  PlainSocketTraits(ProcessArgs i, Socket socket)
      : SocketProcess(std::move(i)), socket_(std::move(socket)) {}

  Socket& socket() { return socket_; }
  auto toId() { return socket().native_handle(); }

  MethodTask<void> start() { co_return; }

  ~PlainSocketTraits() {
    boost::system::error_code ec;
    socket().cancel(ec);
    socket().close(ec);
  }

private:
  Socket socket_;
};

template <class Traits> struct TSocketProcess : public Traits {
  bool eof_ = false;

  EslangPromise p_;
  std::optional<ExceptionWrapper> except_;

  std::optional<TSendAddress<Tcp::ReceiveData>> toSend;

  TSendAddress<Tcp::Socket> onReady;

  TSocketProcess(ProcessArgs i, typename Traits::Socket socket,
                 TSendAddress<Tcp::Socket> onReady)
      : Traits(std::move(i), std::move(socket)), onReady(std::move(onReady)) {
    ESLOG(LL::TRACE, "Socket ", toId(), " created");
  }

  ~TSocketProcess() { ESLOG(LL::TRACE, pid(), ": ~SocketProcess fd=", toId()); }

  void checkExcept() {
    if (except_) {
      except_->maybeThrowException();
    }
  }

  void setValue() { p_.setIfUnset(); }

  void setError(boost::system::error_code const& ec) {
    setException(
        std::runtime_error(concatString("Listener threw ", ec.message())));
  }

  template <class T> void setException(T const& e) {
    setValue();
    if (!except_) {
      except_ = ExceptionWrapper::make(e);
    }
  }

  char readBuff[16000];
  void asyncRead() {
    socket().async_read_some(
        buffer(readBuff, sizeof(readBuff)),
        [this](const boost::system::error_code& error, std::size_t bytes) {
          if (error == error::operation_aborted) {
            return;
          }
          if (error == error::eof) {
            eof_ = true;
            ESLOG(LL::TRACE, pid(), ": EOF");
            setValue();
            return;
          }
          if (error) {
            setError(error);
            return;
          }
          if (bytes > 0) {
            send(*toSend,
                 Tcp::ReceiveData(pid(), Buffer::makeCopy(&readBuff, bytes)));
          }
          asyncRead();
        });
  }

  bool isWriting = false;
  void write(Buffer buff) {
    isWriting = true;
    p_ = EslangPromise();
    async_write(socket(), buffer(buff.data(), buff.size()),
                [this](const boost::system::error_code& ec,
                       std::size_t bytes_transferred) {
                  if (ec == error::operation_aborted) {
                    return;
                  }
                  isWriting = false;
                  if (ec) {
                    ESLOG(LL::INFO, "Write error ", ec.message());
                    setError(ec);
                  } else {
                    ESLOG(LL::INFO, "Wrote ", bytes_transferred);
                    setValue();
                  }
                });
  }

  ProcessTask run() {
    // init the socket
    co_await start();
    // tell our owner about us
    this->send(onReady, Tcp::Socket(pid()));

    // now can process
    toSend = co_await recv(init);
    asyncRead();
    while (!eof_) {
      if (isWriting) {
        // don't get a message to write until we are done with writing the last
        // one
        co_await WaitOnFuture(&p_);
        checkExcept();
      } else {
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
  }
};

struct ListenerProcess : public Process {
  TSendAddress<Tcp::Socket> newSocket;
  Tcp::ListenerOptions options;
  EslangPromise error_;
  ip::tcp const protocol = ip::tcp::v4();
  std::shared_ptr<ssl::context> sslContext;
  ListenerProcess(ProcessArgs i, TSendAddress<Tcp::Socket> new_socket_address,
                  Tcp::ListenerOptions options)
      : Process(std::move(i)), newSocket(std::move(new_socket_address)),
        options(options) {
    if (options.sslContextFactory) {
      sslContext = (*options.sslContextFactory)(c_->ioService());
    }
  }

  void setException(boost::system::error_code const& ec) {
    error_.setException(
        std::runtime_error(concatString("Listener threw ", ec.message())));
  }

  ip::tcp::socket nextSocket() {
    auto ret = ip::tcp::socket(c_->ioService());
    return ret;
  }

  ip::tcp::socket next{nextSocket()};
  void asyncAccept(ip::tcp::acceptor& socket) {
    socket.async_accept(next, [this,
                               &socket](const boost::system::error_code& ec) {
      if (ec == error::operation_aborted) {
        return;
      }
      if (ec) {
        setException(ec);
      } else {
        // linking so it never gets lost. maybe should rethink that.
        ip::tcp::no_delay option(true);
        next.set_option(option);

        if (sslContext) {
          auto ssl = std::make_unique<ssl::stream<ip::tcp::socket>>(
              c_->ioService(), *sslContext);
          ssl->lowest_layer() = std::move(next);
          addKillOnDie(spawn<TSocketProcess<SslSocketTraits>>(std::move(ssl),
                                                              newSocket));
        } else {
          addKillOnDie(spawn<TSocketProcess<PlainSocketTraits>>(std::move(next),
                                                                newSocket));
        }
        next = nextSocket();
        asyncAccept(socket);
      }
    });
  }
  ProcessTask run() {
    ip::tcp::acceptor socket(c_->ioService());
    ESLOG(LL::INFO, "Bind to ", options.port);
    socket.open(protocol);
    socket.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    socket.bind(ip::tcp::endpoint(protocol, options.port));
    socket.listen(10000);
    asyncAccept(socket);
    ESLOG(LL::DEBUG, "Listening on ", socket.local_endpoint());
    co_await WaitOnFuture(&error_);
  }
};

Pid Tcp::makeListener(Process* parent, TSendAddress<Socket> new_socket_address,
                      Tcp::ListenerOptions options) {
  return parent->spawnLink<ListenerProcess>(std::move(new_socket_address),
                                            options);
}

void Tcp::initRecvSocket(Process* sender, Socket socket,
                         TSendAddress<ReceiveData> new_socket_address) {
  ESLOG(LL::DEBUG, "Init ", socket.pid);
  sender->send(socket.pid, &SocketProcess::init, std::move(new_socket_address));
}

void Tcp::send(Process* sender, Socket socket, Buffer data) {
  sender->send(socket.pid, &SocketProcess::send_data, std::move(data));
}

void Tcp::sendMany(Process* sender, Socket socket, BufferCollection data) {
  sender->send(socket.pid, &SocketProcess::send_many_data, std::move(data));
}
}