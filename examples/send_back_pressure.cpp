#include <eslang/Context.h>
#include <folly/init/Init.h>
#include <glog/logging.h>

/// Example of sending messages but suspending the sender to allow the consumer
/// to catch up

namespace s {

class Listener : public Process {
public:
  using Process::Process;

  Slot<std::string> r{this};
  ProcessTask run() {
    int i = 0;
    while (true) {
      LOG(INFO) << "Receive " << i++ << " with " << (co_await recv(r)).size()
                << " bytes";
    }
  }
};

class Sender : public Process {
public:
  using Process::Process;

  ProcessTask run() {
    auto l = spawnLink<Listener>();
    // would probably run out of memory if we didnt have back pressure
    for (int i = 0; i < 1000; ++i) {
      int const k = 100 * 1024 * 1024;
      LOG(INFO) << "Send " << i << " with " << k << " bytes";
      co_await send(l, &Listener::r, std::string(k, '?'));
    }
  }
};
}

int main(int argc, char** argv) {
  FLAGS_stderrthreshold = 0;
  FLAGS_v = 3;
  folly::init(&argc, &argv);
  s::Context c;
  c.spawn<s::Sender>();
  c.run();
  return 0;
}
