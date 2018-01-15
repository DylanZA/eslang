#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <eslang/Context.h>
#include <eslang/Logging.h>

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
    static uint64_t counter = 0;
    bool const log_every = (++counter % 100000 == 0);
    if (log_every) {
      ESLOG(LL::INFO, "recvd ", our_value, " from parent");
    }
    if (our_value < kMax) {
      // spawn a counter that will send back to us
      auto pid = spawn<Counter>(kMax, this->pid());
      send<int>(pid, &s::Counter::from_parent_process, our_value);
      auto subprocess_result = co_await recv(from_sub_process);
      if (our_value + 1 != subprocess_result) {
        throw std::runtime_error("unexpected");
      }
      if (log_every) {
        ESLOG(LL::INFO, "Subprocess added 1 and got ", subprocess_result);
      }
    }
    if (target) {
      // send back upstream
      send<int>(*target, &s::Counter::from_sub_process, our_value);
    }
  }
};

class MethodCounter : public Process {
public:
  int const kMax;
  MethodCounter(ProcessArgs i, int m) : Process(std::move(i)), kMax(m) {}

  MethodTask<void> subRun(int n, int& out) {
    if (n > 0) {
      co_await subRun(n - 1, out);
      ++out;
    }
    co_return;
  }

  MethodTask<int> retIntCoro(int i) { co_return i; }

  MethodTask<> doNothingCoro() { co_return; }

  ProcessTask run() {
    co_await doNothingCoro();
    ESLOG(LL::INFO, "Got ", co_await retIntCoro(5));
    int our_value = 0;
    co_await subRun(kMax, our_value);
    ESLOG(LL::INFO, "Counted to ", our_value, " wanted ", kMax);
  }
};

class SleepProfiler : public Process {
public:
  int const kMax;
  SleepProfiler(ProcessArgs i, int m) : Process(std::move(i)), kMax(m) {}

  class Sleeper : public Process {
  public:
    TSendAddress<int> a;
    Sleeper(ProcessArgs i, TSendAddress<int> a) : Process(std::move(i)), a(a) {}
    ProcessTask run() {
      co_await sleep(std::chrono::milliseconds(1000));
      co_await send(a, 1);
    }
  };

  ProcessTask run() {
    Slot<int> s(this);
    for (int i = 0; i < kMax; ++i) {
      spawn<Sleeper>(s.address());
    }
    for (int i = 0; i < kMax; ++i) {
      co_await recv(s);
    }
    ESLOG(LL::INFO, "Counted to ", kMax);
  }
};
}

template <class T> void run(std::string type, int const k) {
  ESLOG(s::LL::INFO, "----------------------", " start ", type);
  s::Context c;
  auto start = std::chrono::steady_clock::now();
  auto starter = c.spawn<T>(k);
  c.run();
  ESLOG(s::LL::INFO, "Took ",
        0.001 *
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start)
                .count(),
        "s to count to ", k, " by spawning that many ", type);
  ESLOG(s::LL::INFO, "----------------------", " end ", type);
}

int main(int argc, char** argv) {
  boost::log::core::get()->set_filter(boost::log::trivial::severity >=
                                      boost::log::trivial::info);

  // run<s::SleepProfiler>("sleep profiler", 3000000);
  // submethods run on the same stack, so cannot have too many
  // run<s::MethodCounter>("methods", 256);
  run<s::Counter>("processes", 5000000);
  return 0;
}
