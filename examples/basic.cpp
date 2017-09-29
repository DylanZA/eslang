#include <glog/logging.h>

#include <eslang/Context.h>
#include <folly/Conv.h>
#include <folly/init/Init.h>
#include <iostream>

namespace s {
class ThrowingApp : public Process {
public:
  using Process::Process;
  Slot<std::string> message{this};
  ProcessTask run() {
    auto m = co_await recv(message);
    throw std::runtime_error(folly::to<std::string>("Throw ", m).c_str());
  }
};

class EchoApp : public Process {
public:
  using Process::Process;
  Slot<int> echo{this};
  ProcessTask run() {
    co_await send<int>(echo.address(), 5);
    auto res = co_await recv(echo);
    LOG(INFO) << "Got " << res;
    res = co_await recv(echo);
    LOG(INFO) << "Got " << res;

    auto pid = spawn<ThrowingApp>();
    co_await send<std::string>(pid, &s::ThrowingApp::message, "hello there");

    pid = spawnLink<ThrowingApp>();
    co_await send<std::string>(pid, &s::ThrowingApp::message,
                               "hello there LINKED!");

    // nothing will send to this, so we essentially block until our linked app
    // dies
    co_await recv(echo);
    LOG(INFO) << "Should not get here";
  }
};

class SenderApp : public Process {
public:
  ~SenderApp() { LOG(INFO) << "Sender destruct"; }
  TSendAddress<int> s_;
  SenderApp(ProcessArgs i, TSendAddress<int> s)
      : Process(std::move(i)), s_(s) {}

  ProcessTask run() { co_await send<int>(s_, 99); }
};

class SleepingApp : public Process {
public:
  std::chrono::milliseconds s_;
  SleepingApp(ProcessArgs i,
              std::chrono::milliseconds s = std::chrono::milliseconds(5000))
      : Process(std::move(i)), s_(s) {}

  ProcessTask run() {
    co_await sleep(s_);
    co_return;
  }
};

class SpawnedApp : public Process {
public:
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
  ProcessTask run() {
    for (int i = 0; i < 5; ++i) {
      LOG(INFO) << "Spawn " << i;
      Slot<Pid> s(this);
      spawnLinkNotify<SpawnedApp>(s.address(), i);
      auto w = co_await recv(s);
      LOG(INFO) << i << " done";
    }
    Slot<Pid> s(this);
    spawnLinkNotify<BlockForEver>(s.address());
    LOG(INFO) << "Done spawning";
  }
};

class MethodExample : public Process {
public:
  using Process::Process;

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
    for (auto s : subs) {
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

    ProcessTask run() { co_await runSub(); }

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
    if (!*bool_test) {
      ESLANGEXCEPT("failed to clean up");
    }
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
}

template <class T> void runSimple(std::string s) {
  run(s, [](s::Context& c) { c.spawn<T>(); });
}

int main(int argc, char** argv) {
  FLAGS_stderrthreshold = 0;
  FLAGS_v = 2;
  folly::init(&argc, &argv);
  runSimple<s::MethodCleansUpExample>("Method task cleans up");
  runSimple<s::MethodExample>("Method");
  runSimple<s::NotifyApp>("Notify");
  run("Echo with throwing", [](s::Context& c) {
    auto pid = c.spawn<s::EchoApp>();
    c.spawn<s::SenderApp>(c.makeSendAddress(pid, &s::EchoApp::echo));
  });
  runSimple<s::SleepingApp>("Sleeping");
  return 0;
}
