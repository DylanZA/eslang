#include <eslang/Context.h>
#include <folly/Conv.h>
#include <folly/init/Init.h>
#include <glog/logging.h>

namespace s {

class Counter : public Process {
public:
  int const kMax;
  std::optional<Pid> const target;
  Slot<int> from_parent_process{this};
  Slot<int> from_sub_process{this};
  Counter(ProcessArgs i, int m, Pid target)
      : Process(std::move(i)), kMax(m), target(target) {}

  Counter(ProcessArgs i, int m) : Process(std::move(i)), kMax(m) {}

  ProcessTask run() {
    int our_value = 0;
    if (target) {
      // we are not the first instance
      our_value = co_await recv(from_parent_process) + 1;
    }
    LOG_EVERY_N(INFO, 100000) << "recvd " << our_value << " from parent";
    if (our_value < kMax) {
      // spawn a counter that will send back to us
      auto pid = spawn<Counter>(kMax, this->pid());
      send<int>(pid, &s::Counter::from_parent_process, our_value);
      auto subprocess_result = co_await recv(from_sub_process);
      if (our_value + 1 != subprocess_result) {
        throw std::runtime_error("unexpected");
      }
      LOG_EVERY_N(INFO, 100000)
          << "Subprocess added 1 and got " << subprocess_result;
    }
    if (target) {
      // send back upstream
      send<int>(*target, &s::Counter::from_sub_process, our_value);
    }
  }
};
}

int main(int argc, char** argv) {
  FLAGS_stderrthreshold = 0;
  folly::init(&argc, &argv);
  s::Context c;
  auto start = std::chrono::steady_clock::now();
  int const k = 5000000;
  auto starter = c.spawn<s::Counter>(k);
  c.run();
  LOG(INFO) << "Took "
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start)
                   .count()
            << "ms to count to " << k << " by spawning that many processes";
  return 0;
}
