#include <eslang/Context.h>
#include <folly/Conv.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

namespace s {

class MethodCounter : public Process {
public:
  int const kMax;
  MethodCounter(ProcessArgs i, int m) : Process(std::move(i)), kMax(m) {}

  MethodTask<int> subRun(int n) {
    if (n > 0) {
      co_return co_await subRun(n - 1) + 1;
    }
    co_return 0;
  }

  ProcessTask run() {
    int our_value = co_await subRun(kMax);
    ASSERT_EQ(our_value, kMax);
  }
};

class MethodBasic: public Process {
public:
  using Process::Process;

  MethodTask<int> retIntCoro(int i) {
    co_return i;
  }

  MethodTask<int> retIntFn(int i) {
    return i;
  }

  MethodTask<> doNothingCoro() {
    co_return;
  }

  MethodTask<> doNothingFn() {
    return MethodTask<>{};
  }

  ProcessTask run() {
    auto a = WaitingYield{};
    auto b= doNothingFn();
    auto c = doNothingCoro();
    ASSERT_EQ(5, co_await retIntCoro(5));
    ASSERT_EQ(5, co_await retIntFn(5));
    co_await a;
    co_await b;
    co_await c;
  }
};

}

template<class T>
void run(std::string type, int const k) {
  s::Context c;
  auto start = std::chrono::steady_clock::now();
  auto starter = c.spawn<T>(k);
  c.run();
  LOG(INFO) << "Took "
    << std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start)
    .count()
    << "ms to count to " << k << " by spawning that many " << type;
}

TEST(MethodTask, Basic) {
  // submethods run on the same stack, so cannot have too many
  run<s::MethodCounter>("methods", 256);
}
