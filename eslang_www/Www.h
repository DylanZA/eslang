#include <eslang/Context.h>
#include <eslang_io/Tcp.h>

#include <beast/http/message.hpp>
#include <beast/http/string_body.hpp>

namespace s {

class Www {
public:
  struct Session {};

  struct Request {
    beast::http::request<beast::http::string_body> message;
    std::string toString() const;
  };

  struct Response {
    beast::http::response<beast::http::string_body> message;
  };

  class Server : public Process {
  public:
    class IHandler {
    public:
      virtual ~IHandler() = default;
      virtual MethodTask<Response> getResponse(Process*, Request const&) = 0;
      virtual GenTask<Buffer> getChunked(Process*, Request const&) = 0;
      static std::unique_ptr<IHandler> makeSimple(std::function<MethodTask<Response>(Process*, Request const&)> f);
    };
    Server(ProcessArgs i, std::shared_ptr<IHandler> handler, Tcp::ListenerOptions options)
        : Process(std::move(i)), handler_(std::move(handler)),
          options_(std::move(options)) {}

    ProcessTask run();

  private:
    std::shared_ptr<IHandler> handler_;
    Tcp::ListenerOptions options_;
  };
};
}