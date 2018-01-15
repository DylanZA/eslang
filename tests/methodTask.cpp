#include <eslang/Context.h>

#include <eslang/Logging.h>
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

class MethodThrows : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;
  MethodTask<int> throwCoro(int i) {
    LIFETIMECHECK;
    co_await WaitingYield{};
    ESLANGEXCEPT();
    co_return i;
  }

  ProcessTask run() {
    LIFETIMECHECK;
    co_await throwCoro(5);
    FAIL();
  }
};

class MethodStackInversion : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;

  MethodTask<> C(bool should_sleep, int& finished) {
    LIFETIMECHECK;
    if (should_sleep) {
      co_await sleep(std::chrono::milliseconds(1));
    }
    ++finished;
  }

  MethodTask<> B(bool should_sleep, int& finished) {
    LIFETIMECHECK;
    int i = 0;
    co_await C(true, i);
    EXPECT_EQ(1, i);
    if (should_sleep) {
      co_await sleep(std::chrono::milliseconds(1));
    }
    co_await C(false, i);
    EXPECT_EQ(2, i);
    ++finished;
  }

  MethodTask<> A(int& finished) {
    LIFETIMECHECK;
    int i = 0;
    co_await B(false, i);
    EXPECT_EQ(1, i);
    co_await sleep(std::chrono::milliseconds(1));
    co_await B(true, i);
    EXPECT_EQ(2, i);
    co_await B(true, i);
    EXPECT_EQ(3, i);
    ++finished;
  }

  ProcessTask run() {
    LIFETIMECHECK;
    int i = 0;
    ScopeRun sr([&] { EXPECT_EQ(i, 1); });
    co_await A(i);
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
TEST(MethodTask, Throws) { run<s::MethodThrows>(); }
TEST(MethodTask, StackInversion) { run<s::MethodStackInversion>(); }
