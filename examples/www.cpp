#include <eslang/Logging.h>

#include <eslang/Context.h>
#include <eslang_www/Www.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <random>


namespace s {

class ExampleWwwHandler : public Www::Server::IHandler {
  std::mt19937 gen{ 123456 };
  
  MethodTask<Www::Response> getResponse(Process* proc,
                                        Www::Request const& req) override {
    Www::Response resp{};
    ESLOG(LL::INFO, "Received ", req.toString());
    resp.message.set(boost::beast::http::field::content_type, "text/plain");
    if (req.message.target().find("favicon") != std::string::npos) {
      resp.message.result(boost::beast::http::status::not_found);
      resp.message.body() = "404 not found";
      resp.message.prepare_payload();
      co_return resp;
    }
    auto& r = resp.message;
    auto dist = std::uniform_real_distribution<double>{ 0, 0.75 };
    double const sleep = dist(gen);
    ESLOG(LL::INFO, "Sleep ", sleep, "s");
    co_await proc->sleep(std::chrono::milliseconds((int)(sleep * 1000)));
    ESLOG(LL::INFO, "Sleep ", sleep, "s done");
    r.result(boost::beast::http::status::ok);
    r.body() = concatString("Hello, world! from ",
                          std::string(req.message.target()), " slept ", sleep);
    if (req.message.target().find("chunk") != std::string::npos) {
      r.chunked(true);
    }
    else {
      r.prepare_payload();
    }
    co_return resp;
  }

  GenTask<Buffer> getChunked(Process* proc, Www::Request const&) override {
    for (int i = 0; i < 10; ++i) {
      co_await proc->sleep(std::chrono::milliseconds(10));
      ESLOG(LL::INFO, "Send chunk ", i);
      co_yield Buffer::makeCopy(concatString("hello ", i, "\n"));
    }
  }
};
}

struct Options {
  std::string ca;
  std::string key;
  std::string cert;
  uint32_t plainPort = 12345;
  uint32_t sslPort = 12346;
};
namespace po = boost::program_options;

int main(int argc, char** argv) {
  Options o;
  po::options_description desc{ "Options" };
  desc.add_options()
    ("help,h", "Help screen")
    ("ca", po::value<std::string>())
    ("key", po::value<std::string>())
    ("cert", po::value<std::string>())
    ("plainPort", po::value<uint32_t>()->default_value(12345))
    ("sslPort", po::value<uint32_t>()->default_value(12346))
    ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  s::Context c;
  if (auto v = vm["plainPort"].as<uint32_t>()) {
    s::Tcp::ListenerOptions listener_options(v);
    c.spawn<s::Www::Server>(std::make_unique<s::ExampleWwwHandler>(),
                            listener_options);
  }
  if (auto v = vm["sslPort"].as<uint32_t>()) {
    s::Tcp::ListenerOptions listener_options(v);
    listener_options =
      listener_options.withSslFiles(vm["ca"].as<std::string>(), vm["cert"].as<std::string>(), vm["key"].as<std::string>());
    c.spawn<s::Www::Server>(std::make_unique<s::ExampleWwwHandler>(),
                            listener_options);
  }
  c.run();
  return 0;
}
