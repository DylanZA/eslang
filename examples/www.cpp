#include <folly/Conv.h>
#include <folly/Random.h>
#include <folly/init/Init.h>
#include <glog/logging.h>

#include <eslang/Context.h>
#include <eslang_www/Www.h>

DEFINE_string(ca, "", "ssl certificate authority");
DEFINE_string(key, "", "ssl server key");
DEFINE_string(cert, "", "ssl server cert");
DEFINE_int32(plainPort, 12345, "server port");
DEFINE_int32(sslPort, 12346, "ssl port");

namespace s {

class ExampleWwwHandler : public Www::Server::IHandler {
  MethodTask<Www::Response> getResponse(Process* proc,
                                        Www::Request const& req) override {
    Www::Response resp{};
    LOG(INFO) << "Received " << req.toString();
    resp.message.set(beast::http::field::content_type, "text/plain");
    if (req.message.target().find("favicon") != std::string::npos) {
      resp.message.result(beast::http::status::not_found);
      resp.message.body = "404 not found";
      resp.message.prepare_payload();
      co_return resp;
    }
    auto& r = resp.message;
    double const sleep = folly::Random::randDouble01() * 0.75;
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
  if (FLAGS_plainPort) {
    s::Tcp::ListenerOptions listener_options(FLAGS_plainPort);
    c.spawn<s::Www::Server>(std::make_unique<s::ExampleWwwHandler>(),
                            listener_options);
  }
  if (FLAGS_sslPort) {
    s::Tcp::ListenerOptions listener_options(FLAGS_sslPort);
    listener_options =
        listener_options.withSslFiles(FLAGS_ca, FLAGS_cert, FLAGS_key);
    c.spawn<s::Www::Server>(std::make_unique<s::ExampleWwwHandler>(),
                            listener_options);
  }
  c.run();
  return 0;
}
