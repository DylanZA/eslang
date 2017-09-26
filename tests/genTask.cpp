#include <eslang/Context.h>
#include <folly/Conv.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

#include "TestCommon.h"

namespace s {

class GenBasic : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;

  GenTask<int> yieldInts() {
    LIFETIMECHECK;
    LOG(INFO) << "Starting";
    LOG(INFO) << "Yield 99";
    co_yield 99;
    LOG(INFO) << "Start sleep";
    co_await sleep(std::chrono::milliseconds(2000));
    LOG(INFO) << "Done  sleep";
    for (int i = 0; i < 10; ++i) {
      LOG(INFO) << "Yield " << i;
      co_yield i;
    }
    co_return;
  }

  GenTask<int> yieldNoInts() {
    LIFETIMECHECK;
    co_return;
  }

  ProcessTask run() {
    auto ret = yieldInts();
    while (co_await ret.next()) {
      LOG(INFO) << "Got " << ret.take();
    }
    {
      auto ret2 = yieldNoInts();
      EXPECT_FALSE(co_await ret2.next());
    }
    co_return;
  }
};

class GenMultiTypes : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;

  MethodTask<std::string> getString() {
    LIFETIMECHECK;
    co_await sleep(std::chrono::milliseconds(500));
    co_return "MethodTask";
  }

  GenTask<std::string> yieldStrings() {
    LIFETIMECHECK;
    co_await sleep(std::chrono::milliseconds(500));
    co_yield "there";
    LOG(INFO) << "Waiting";
    auto s = co_await getString();
    EXPECT_EQ("MethodTask", s);
    LOG(INFO) << "Got string";
    co_yield s;
    co_yield "done";
  }

  GenTask<size_t> yieldInts() {
    LIFETIMECHECK;
    auto ret = yieldStrings();
    while (co_await ret.next()) {
      LOG(INFO) << "Got string " << ret.take();
      co_yield ret.take().size();
    }
  }

  ProcessTask run() {
    LIFETIMECHECK;
    auto ret = yieldInts();
    while (co_await ret.next()) {
      LOG(INFO) << "Got " << ret.take();
    }
  }
};

class GenMultiTask : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;

  MethodTask<std::string> getString() {
    LIFETIMECHECK;
    co_await sleep(std::chrono::milliseconds(10));
    co_return "HI";
  }

  GenTask<std::string> yieldStrings() {
    LIFETIMECHECK;
    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ("HI", co_await getString());
    }
    co_yield "done";
  }

  ProcessTask run() {
    LIFETIMECHECK;
    auto run_full = yieldStrings();
    auto run_half = yieldStrings();
    auto run_none = yieldStrings();
    EXPECT_TRUE(co_await run_full.next());
    EXPECT_TRUE(co_await run_half.next());
    EXPECT_EQ("done", run_full.take());
    EXPECT_FALSE(co_await run_full.next());
  }
};

class GenRecursive : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;

  GenTask<int> yieldInts(int n) {
    LIFETIMECHECK;
    if (n == 0) {
      co_yield 0;
      co_yield 1;
      co_yield 2;
    } else {
      auto ret = yieldInts(n - 1);
      int count = 0;
      while (co_await ret.next()) {
        LOG(INFO) << n << ": Got int " << ret.take();
        EXPECT_EQ(count, ret.take());
        ++count;
        co_yield ret.take();
      }
      EXPECT_EQ(count, 3);
    }
  }

  ProcessTask run() {
    LIFETIMECHECK;
    auto ret = yieldInts(10);
    int count = 0;
    while (co_await ret.next()) {
      LOG(INFO) << "Got " << ret.take();
      ++count;
    }
    EXPECT_EQ(3, count);
  }
};

class GenThrows : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;
  ScopeLog scopelog{"GenThrows"};
  GenTask<int> yieldInts() {
    ScopeLog sc("GenThrows::yieldInts");
    LIFETIMECHECK;
    co_yield 0;
    LOG(INFO) << "About to yield " << 1;
    co_yield 1;
    LOG(INFO) << "About to throw";
    ESLANGEXCEPT();
    // should not get here
    assert(false);
    std::terminate();
  }

  ProcessTask run() {
    ScopeLog scopelog{"GenThrows::ProcessTask"};
    LIFETIMECHECK;
    auto ret = yieldInts();
    while (co_await ret.next()) {
      LOG(INFO) << "Got " << ret.take();
    }
  }
};

class ProcToDestroy : public Process {
public:
  LIFETIMECHECK;
  TSendAddress<int> addr;
  ScopeLog scopelog{"~ProcToDestroy"};
  ProcToDestroy(ProcessArgs a, TSendAddress<int> addr)
      : Process(std::move(a)), addr(addr) {}

  GenTask<int> yieldInts() {
    ScopeLog sc("ProcToDestroy::yieldInts");
    LIFETIMECHECK;
    co_yield 0;
    LOG(INFO) << "About to yield " << 1;
    co_yield 1;
    LOG(INFO) << "About to sleep";
    Slot<int> s(this);
    int x = co_await this->recv(s);
    // should not get here
    assert(false);
    std::terminate();
  }

  ProcessTask run() {
    ScopeLog sc("ProcToDestroy::run");
    LIFETIMECHECK;
    auto ret = yieldInts();
    while (co_await ret.next()) {
      LOG(INFO) << "Got " << ret.take();
      send(addr, ret.take());
    }
  }
};

class GenDestroy : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;
  ScopeLog scopelog{"~GenDestroy"};
  ProcessTask run() {
    ScopeLog sc("GenDestroy");
    Slot<int> slot{this};
    auto pid = spawn<ProcToDestroy>(slot.address());
    int msg = co_await recv(slot);
    LOG(INFO) << "Got " << msg;
    queueKill(pid);
  }
};

class ForEach : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;
  GenTask<int> genInts(int n) {
    for (int i = 0; i < n; ++i) {
      co_yield i;
    }
  }
  ProcessTask run() {
    int count = 0;
    int const N = 10;
    for co_await(int i : genInts(N)) {
        count++;
        LOG(INFO) << "Got " << i;
   }
    EXPECT_EQ(N, count);
    co_return;
  }
};
}

template <class T> void run() {
  s::Context c;
  auto starter = c.spawn<T>();
  c.run();
  lifetimeChecker.check();
}

TEST(GenBasic, Basic) { run<s::GenBasic>(); }
TEST(GenBasic, GenMultiTypes) { run<s::GenMultiTypes>(); }
TEST(GenBasic, GenRecursive) { run<s::GenRecursive>(); }
TEST(GenBasic, GenThrows) { run<s::GenThrows>(); }
TEST(GenBasic, GenDestroy) { run<s::GenDestroy>(); }
TEST(GenBasic, ForEach) { run<s::ForEach>(); }
TEST(GenBasic, GenMultiTask) { run<s::GenMultiTask>(); }