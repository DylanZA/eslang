#include "Www.h"
#include <beast.hpp>
#include <boost/asio/buffer.hpp>

namespace s {

using namespace beast;

std::string Www::Request::toString() const {
  std::stringstream ss;
  auto h = message.find(http::field::host);
  ss << message.method_string() << " " << message.target();
  if (h != message.end()) {
    ss << " host:" << h->value();
  }
  return ss.str();
}

namespace {
void checkOrThrow(error_code& ec) {
  if (ec) {
    ESLANGEXCEPT("Badd error code ", ec.message());
  }
}

template<class T>
Buffer makeBuffer(T buff) {
  return Buffer::makeCopy(
    boost::asio::buffer_cast<char const*>(buff),
    boost::asio::buffer_size(buff));
}

template<class T>
GenTask<Buffer> makeBuffersFromSequence(T seq) {
  for (auto buff : seq) {
    co_yield Buffer::makeCopy(
      boost::asio::buffer_cast<char const*>(buff),
      boost::asio::buffer_size(buff));
  }
}

class WwwParser : NonMovable {
public:
  GenTask<Www::Request> push(Buffer data) {
    error_code ec;
    do {
      boost::asio::buffer_copy(
        currentRead_.prepare(data.size()),
        boost::asio::const_buffer(data.data(), data.size()));
      size_t read =
        parser_->put(boost::asio::buffer(data.data(), data.size()), ec);
      currentRead_.consume(read);
      if (ec == http::error::need_more) {
        co_return;
      }
      checkOrThrow(ec);
      if (parser_->is_done()) {
        co_yield Www::Request{ parser_->release() };
        parser_.reset();
        parser_.emplace();
      }
    } while (currentRead_.size());
  }

  struct SerializeVisitor {
    SerializeVisitor(std::vector<Buffer>& buffs) : buffs(buffs) {}
    std::vector<Buffer>& buffs;
    size_t last = 0;

    template <class T> void operator()(error_code& ec, T b) {
      checkOrThrow(ec);
      last = 0;
      for (auto const& buff : b) {
        last += boost::asio::buffer_size(buff);
        buffs.push_back(makeBuffer(buff));
      }
    }
  };

  GenTask<Buffer> convert(Www::Response& response_in) {
    Www::Response response(std::move(response_in));
    http::response_serializer<http::string_body> serializer(
      std::move(response.message));
    std::vector<Buffer> buffs;
    error_code ec;
    SerializeVisitor visitor(buffs);
    while (!serializer.is_done()) {
      visitor.last = 0;
      serializer.next(ec, visitor);
      for (auto&& b : buffs) {
        co_yield std::move(b);
      }
      buffs.clear();
      serializer.consume(visitor.last);
    }
  }

  GenTask<Buffer> convertHeaderOnly(Www::Response const& response) {
    http::response_serializer<http::string_body> serializer(std::move(response.message));
    std::vector<Buffer> buffs;
    error_code ec;
    SerializeVisitor visitor(buffs);
    serializer.split(true);
    while (!serializer.is_done()) {
      visitor.last = 0;
      serializer.next(ec, visitor);
      for (auto&& b : buffs) {
        co_yield std::move(b);
      }
      buffs.clear();
      serializer.consume(visitor.last);
      if (serializer.is_header_done()) {
        co_return;
      }
    }
  }

private:
  multi_buffer currentRead_;
  Www::Request current_;

  // beast doesnt allow me to recreate parser without the optional trick.
  // SAD!
  std::optional<http::request_parser<http::string_body>> parser_{
      http::request_parser<http::string_body>{} };
};
}

class SessionRunner : public Process {
public:
  Tcp::Socket s_;
  std::shared_ptr<Www::Server::IHandler> handler;
  SessionRunner(ProcessArgs i, Tcp::Socket s, std::shared_ptr<Www::Server::IHandler> h)
    : Process(std::move(i)), s_(std::move(s)), handler(std::move(h)) {
    link(s.pid);
  }

  Slot<Tcp::ReceiveData> recv{ this };
  ProcessTask run() {
    Tcp::initRecvSocket(this, s_, recv.address());
    WwwParser parser;
    while (true) {
      auto r = co_await Process::recv(recv);
      auto recv = parser.push(std::move(r.data));
      while (co_await recv.next()) {
        auto& req = recv.take();
        auto resp = co_await handler->getResponse(this, req);
        if (req.message.keep_alive()) {
          resp.message.keep_alive(true);
        }
        resp.message.set(http::field::server, "Eslang");
        if (req.message.keep_alive()) {
          resp.message.keep_alive(true);
        }
        resp.message.version = req.message.version;
        bool const chunked = resp.message.chunked();
        StreamBatcher sb(this, s_);
        if (!chunked) {
          for co_await(auto buff : parser.convert(std::move(resp))) {
            sb.push(std::move(buff));
          }
        } else {
          for co_await(auto buff : parser.convertHeaderOnly(resp)) {
            sb.push(std::move(buff));
          }
          LOG(INFO) << "C";
          for co_await(auto buff : handler->getChunked(this, req)) {
            LOG(INFO) << "get buffer size " << buff.size();
            auto chunk = beast::http::make_chunk(
              boost::asio::const_buffers_1(buff.data(), buff.size()));
            for co_await(auto b : makeBuffersFromSequence(chunk)) {
              LOG(INFO) << "send buffer size " << b.size();
              sb.push(std::move(b));
            }
            sb.clear();
          }
          for co_await(auto b : makeBuffersFromSequence(beast::http::make_chunk_last())) {
            sb.push(std::move(b));
          }
        }
      }
    }
  }
};

ProcessTask Www::Server::run() {
  Slot<Tcp::Socket> new_socket{ this };
  Tcp::makeListener(this, new_socket.address(), options_);
  while (true) {
    auto new_sock = co_await recv(new_socket);
    VLOG(2) << "New connect " << new_sock.pid;
    spawn<SessionRunner>(std::move(new_sock), handler_);
  }
}

class SimpleHandler : public Www::Server::IHandler {
public:
  std::function<MethodTask<Www::Response>(Process*, Www::Request const&)> fn_;
  SimpleHandler(std::function<MethodTask<Www::Response>(Process*, Www::Request const&)> fn) : fn_(std::move(fn)) {
  }
  MethodTask<Www::Response> getResponse(Process* p, Www::Request const& r) override {
    return fn_(p, r);
  }
  virtual GenTask<Buffer> getChunked(Process*, Www::Request const&) override {
    ESLANGEXCEPT("Should not be chunked");
  }
};

std::unique_ptr<Www::Server::IHandler> Www::Server::IHandler::makeSimple(std::function<MethodTask<Www::Response>(Process*, Www::Request const&)> fn) {
  return std::make_unique<SimpleHandler>(std::move(fn));
}


}