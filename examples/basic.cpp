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
    co_await send<std::string>(
        pid, &s::ThrowingApp::message,
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
  SleepingApp(ProcessArgs i, std::chrono::milliseconds s)
      : Process(std::move(i)), s_(s) {}

  ProcessTask run() {
    co_await sleep(s_);
    co_return;
  }
};

class SpawnedApp : public Process {
public:
  int i_;
  SpawnedApp(ProcessArgs a, int i)
    : Process(std::move(a)), i_(i) {}
  ProcessTask run() {
    LOG(INFO) << "Run " << i_;
    co_return;
  }
};

class BlockForEver: public Process {
public:
  using Process::Process;
  ~BlockForEver() {
    LOG(INFO) << "Cleanly destroy";
  }
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

}

int main(int argc, char** argv) {
  FLAGS_stderrthreshold = 0;
  FLAGS_v = 2;
  folly::init(&argc, &argv);
  {
    LOG(INFO) << "Notify app";
    s::Context c;
    c.spawn<s::NotifyApp>();
    c.run();
  }
  {
    LOG(INFO) << "Echo app (with throwing)";
    s::Context c;
    auto pid = c.spawn<s::EchoApp>();
    c.spawn<s::SenderApp>(c.makeSendAddress(pid, &s::EchoApp::echo));
    c.run();
  }
  {
    LOG(INFO) << "Sleeping example";
    s::Context c;
    c.spawn<s::SleepingApp>(std::chrono::milliseconds(5000));
    c.run();
    LOG(INFO) << "Sleeping example done";
  }
  return 0;
}
