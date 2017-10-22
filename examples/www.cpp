#include <folly/Conv.h>
#include <folly/Random.h>
#include <folly/init/Init.h>
#include <glog/logging.h>

#include <eslang/Context.h>
#include <eslang_www/Www.h>

namespace s {

class ExampleWwwHandler : public Www::Server::IHandler {
  MethodTask<Www::Response> getResponse(Process* proc,
                                        Www::Request const& req) override {
    Www::Response resp{};
    LOG(INFO) << "Received " << req.toString();
    if (req.message.target().find("favicon") != std::string::npos) {
      resp.message.result(beast::http::status::not_found);
      resp.message.body = "404 not found";
      resp.message.prepare_payload();
      co_return resp;
    }
    auto& r = resp.message;
    double const sleep = folly::Random::randDouble01() * 5;
    LOG(INFO) << "Sleep " << sleep << "s";
    co_await proc->sleep(std::chrono::milliseconds((int)(sleep * 1000)));
    LOG(INFO) << "Sleep " << sleep << "s done";
    r.result(beast::http::status::ok);
    r.body = folly::to<std::string>("Hello, world! from ",
                                    std::string(req.message.target()),
                                    " slept ", sleep);
    if (req.message.target().find("chunk") != std::string::npos) {
      r.chunked(true);
    } else {
      r.prepare_payload();
    }
    co_return resp;
  }

  GenTask<Buffer> getChunked(Process* proc, Www::Request const&) override {
    for (int i = 0; i < 10; ++i) {
      co_await proc->sleep(std::chrono::milliseconds(10));
      LOG(INFO) << "Send chunk " << i;
      co_yield Buffer::makeCopy(folly::to<std::string>("hello ", i, "\n"));
    }
  }
};
}

int main(int argc, char** argv) {
  FLAGS_stderrthreshold = 0;
  FLAGS_v = 3;
  folly::init(&argc, &argv);
  s::Context c;
  s::Tcp::ListenerOptions listener_options(12345);
  c.spawn<s::Www::Server>(std::make_unique<s::ExampleWwwHandler>(),
                          listener_options);
  c.run();
  return 0;
}
