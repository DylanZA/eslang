#include <glog/logging.h>

#include <gtest/gtest.h>

#include "TestCommon.h"
#include <eslang/Context.h>
#include <folly/Conv.h>

namespace s {

class ThrowingApp : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;
  Slot<std::string> message{this};
  ProcessTask run() {
    LIFETIMECHECK;
    auto m = co_await recv(message);
    throw std::runtime_error(folly::to<std::string>("Throw ", m).c_str());
  }
};

class SenderApp : public Process {
public:
  LIFETIMECHECK;
  ~SenderApp() { LOG(INFO) << "Sender destruct"; }
  TSendAddress<int> s_;
  SenderApp(ProcessArgs i, TSendAddress<int> s)
      : Process(std::move(i)), s_(s) {}
  static constexpr int N = 99;
  ProcessTask run() { co_await send<int>(s_, N); }
};

class EchoApp : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;

  Slot<int> echo{this};
  ProcessTask run() {
    LIFETIMECHECK;

    co_await send<int>(echo.address(), 5);
    auto res = co_await recv(echo);
    ASSERT_EQ(5, res);
    res = co_await recv(echo);
    ASSERT_EQ(SenderApp::N, res);

    auto pid = spawn<ThrowingApp>();
    co_await send<std::string>(pid, &s::ThrowingApp::message, "hello there");

    pid = spawnLink<ThrowingApp>();
    co_await send<std::string>(pid, &s::ThrowingApp::message,
                               "hello there LINKED!");

    // nothing will send to this, so we essentially block until our linked app
    // dies
    co_await recv(echo);
    FAIL() << "Should not get here";
  }
};
class SleepingApp : public Process {
public:
  std::chrono::milliseconds s_;
  LIFETIMECHECK;
  SleepingApp(ProcessArgs i,
              std::chrono::milliseconds s = std::chrono::milliseconds(100))
      : Process(std::move(i)), s_(s) {}

  ProcessTask run() {
    auto now = std::chrono::steady_clock::now();
    co_await sleep(s_);
    ASSERT_TRUE(std::chrono::steady_clock::now() - now >= s_);
    co_return;
  }
};

class SpawnedApp : public Process {
public:
  LIFETIMECHECK;
  int i_;
  SpawnedApp(ProcessArgs a, int i) : Process(std::move(a)), i_(i) {}
  ProcessTask run() {
    LOG(INFO) << "Run " << i_;
    co_return;
  }
};

class BlockForEver : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;
  ~BlockForEver() { LOG(INFO) << "Cleanly destroy"; }
  ProcessTask run() {
    Slot<int> s(this);
    recv(s);
    co_return;
  }
};

class NotifyApp : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;
  ProcessTask run() {
    for (int i = 0; i < 5; ++i) {
      LOG(INFO) << "Spawn " << i;
      Slot<Pid> s(this);
      auto pid = spawnLinkNotify<SpawnedApp>(s.address(), i);
      auto w = co_await recv(s);
      ASSERT_EQ(w, pid);
    }
    Slot<Pid> s(this);
    spawnLinkNotify<BlockForEver>(s.address());
    LOG(INFO) << "Done spawning";
  }
};

class MethodExample : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;
  static MethodTask<> subfn(Process* parent, int i) {
    LOG(INFO) << "Start sleep " << i;
    co_await parent->sleep(std::chrono::milliseconds(1000 + i * 100));
    LOG(INFO) << "Done sleep " << i;
  }

  static MethodTask<> fn(Process* parent, TSendAddress<int> to_send, int i) {
    LOG(INFO) << "Start sleep";
    std::vector<MethodTask<>> subs;
    for (int i = 3; i >= 0; i--) {
      subs.push_back(subfn(parent, i));
    }
    LOG(INFO) << "Queued sleeps";
    for (auto& s : subs) {
      co_await s;
    }
    LOG(INFO) << "Done sleeps";
    parent->send(to_send, i);
    LOG(INFO) << "Done send";
  }

  ProcessTask run() {
    LOG(INFO) << "Run " << 7;
    Slot<int> s(this);
    co_await fn(this, s.address(), 7);
    LOG(INFO) << "Done calling method, waiting for message";
    auto msg = co_await recv(s);
    LOG(INFO) << msg << " done, expected " << 7;
  }
};

class MethodCleansUpExample : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;
  struct Method : Process {
    std::shared_ptr<bool> b;
    Method(ProcessArgs a, std::shared_ptr<bool> b)
        : Process(std::move(a)), b(b) {}

    MethodTask<> runSub() {
      auto mv_b = std::move(b);
      SCOPE_EXIT {
        *mv_b = true;
        LOG(INFO) << "Sub process sub task dead";
      };
      co_await WaitingAlways{};
    }

    ProcessTask run() {
      ScopeLog sl("Method::run()");
      co_await runSub();
    }

    ~Method() { LOG(INFO) << "Method main task dead"; }
  };

  ProcessTask run() {
    // spawn a submethod, and make sure that if we kill it it cleans up the
    // coroutine
    auto bool_test = std::make_shared<bool>(false);
    auto sub_two = spawn<Method>(bool_test);
    co_await WaitingYield{};
    queueKill(sub_two);
    co_await WaitingYield{};
    LOG(INFO) << "Bool is " << *bool_test << " expected " << true;
    ASSERT_TRUE(*bool_test);
  }
};
}

template <class T> void run(std::string s, T fn) {
  LOG(INFO) << std::string(30, '-');
  LOG(INFO) << "Running example " << s;
  s::Context c;
  fn(c);
  c.run();
  LOG(INFO) << "Done running example " << s;
  lifetimeChecker.check();
}

template <class T> void runSimple(std::string s) {
  run(s, [](s::Context& c) { c.spawn<T>(); });
}

TEST(Basic, CleansUp) {
  runSimple<s::MethodCleansUpExample>("Method task cleans up");
}
TEST(Basic, Method) { runSimple<s::MethodExample>("Method"); }

TEST(Basic, Notify) { runSimple<s::NotifyApp>("Notify"); }

TEST(Basic, Echo) {
  run("Echo with throwing", [](s::Context& c) {
    auto pid = c.spawn<s::EchoApp>();
    c.spawn<s::SenderApp>(c.makeSendAddress(pid, &s::EchoApp::echo));
  });
}

TEST(Basic, Sleeping) { runSimple<s::SleepingApp>("Sleeping"); }
