#include <eslang/Context.h>
#include <folly/Conv.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

#include "TestCommon.h"

namespace s {

class MethodCounter : public Process {
public:
  int const kMax;
  MethodCounter(ProcessArgs i, int m = 256) : Process(std::move(i)), kMax(m) {}
  LIFETIMECHECK;
  MethodTask<int> subRun(int n) {
    LIFETIMECHECK;
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

class MethodBasic : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;
  MethodTask<int> retIntCoro(int i) {
    LIFETIMECHECK;
    co_return i;
  }

  MethodTask<> doNothingCoro() {
    LIFETIMECHECK;
    co_return;
  }

  ProcessTask run() {
    auto a = WaitingYield{};
    auto c = doNothingCoro();
    ASSERT_EQ(5, co_await retIntCoro(5));
    co_await a;
    co_await c;
  }
};
}

template <class T> void run() {
  s::Context c;
  auto starter = c.spawn<T>();
  c.run();
  lifetimeChecker.check();
}

TEST(MethodTask, Counter) { run<s::MethodCounter>(); }

TEST(MethodTask, Basic) { run<s::MethodBasic>(); }
