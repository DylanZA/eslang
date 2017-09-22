#pragma once

#include "Except.h"
#include "Process.h"
#include "Slot.h"
#include <deque>
#include <map>
#include <memory>
#include <unordered_map>

#include <folly/io/async/EventBase.h>
#include <folly/io/async/HHWheelTimer.h>

namespace s {
class Context {
public:
  TimePoint now() const;

  void run();

  TimePoint now() { return std::chrono::steady_clock::now(); }

  bool waitOnQueue() const { return queue_.size() > 10; }

  template <class T, class Y>
  TSendAddress<T> makeSendAddress(Pid pid, Slot<T> Y::*slot) {
    auto it = findProc(pid);
    if (it == processes_.end()) {
      ESLANGEXCEPT("No process found for pid ", pid.toString());
    }
    Y* proc = dynamic_cast<Y*>((*it)->process.get());
    if (!proc) {
      // sanity check
      ESLANGEXCEPT("Bad process type for ", pid.toString());
    }
    Slot<T>& ss = proc->*slot;
    SlotId id = ss.id();
    return TSendAddress<T>(pid, id);
  }

  template <class T, class... Args> Pid spawn(Args... args) {
    ProcessArgs a(nextPid());
    a.c = this;
    auto p = std::make_unique<T>(a, std::forward<Args>(args)...);
    auto res = p->run();
    if (res.waiting()) {
      ESLANGEXCEPT("Expected waiting to be null");
    }
    if (res.done()) {
      ESLANGEXCEPT("Expected done to be false");
    }
    processes_[a.pid.idx()] = std::make_unique<RunningProcess>(
        a.pid, std::move(p), std::move(res), this);
    queueResume(a.pid, 0);
    return a.pid;
  }

  void link(Process* running, Pid b) {
    auto procb = findProc(b);
    if (procb != processes_.end()) {
      running->addLink(b);
      (*procb)->process->addLink(running->pid());
    } else {
      ESLANGEXCEPT("Proc is already dead");
    }
  }

  // for now, processes don't move, so can keep event base pointers
  folly::EventBase* eventBase() { return &eventBase_; }

private:
  friend class Process;
  void queueResume(Pid p, uint64_t expected_resumes);
  void queueSend(SendAddress a, MessageBase m);

  Pid nextPid();

  struct RunningProcess : public folly::HHWheelTimer::Callback {
    RunningProcess(Pid pid, std::unique_ptr<Process> proc, ProcessTask t,
                   Context* parent);
    void send(SendAddress s, MessageBase m);
    bool waitingFor(SlotId s) const;
    void timeoutExpired() noexcept override;
    void callbackCanceled() noexcept override {}
    void resume();
    bool dead = false;
    Pid pid;
    std::unique_ptr<Process> process;
    ProcessTask task;
    Context* parent;
    uint64_t resumes = 0;
  };

  struct ToProcessItem {
    explicit ToProcessItem(Pid p) : pid(p) {}
    Pid pid;
    std::optional<uint64_t> resume;
    std::optional<std::pair<SendAddress, MessageBase>> message;
  };

  void addtoDestroy(Pid p, std::string s);
  void destroy(std::unique_ptr<RunningProcess> p, std::string reason);
  void processQueueItem(ToProcessItem i);
  std::vector<std::unique_ptr<RunningProcess>>::const_iterator
  findProc(Pid a) const;
  std::vector<std::unique_ptr<RunningProcess>>::iterator findProc(Pid a);

  std::vector<Pid> tombstones_;
  std::vector<std::unique_ptr<RunningProcess>> processes_;
  std::deque<std::pair<std::unique_ptr<RunningProcess>, std::string>>
      toDestroy_;
  std::deque<ToProcessItem> queue_;
  folly::EventBase eventBase_;
  folly::HHWheelTimer::UniquePtr wheelTimer_{
      folly::HHWheelTimer::newTimer(&eventBase_)};
};
}
