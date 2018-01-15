#pragma once

#include "Except.h"
#include "Process.h"
#include "Slot.h"
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <deque>
#include <map>
#include <memory>
#include <unordered_map>

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
    return spawnArgs<T, Args...>(ProcessArgs(nextPid()),
                                 std::forward<Args>(args)...);
  }

  template <class T, class Fn, class... Args>
  Pid spawnWith(Fn f, Args... args) {
    ProcessArgs a(nextPid());
    f(a);
    return spawnArgs<T, Args...>(std::move(a), std::forward<Args>(args)...);
  }

  void link(Process* running, Pid b) {
    auto procb = findProc(b);
    if (procb != processes_.end()) {
      running->addKillOnDie(b);
      (*procb)->process->addKillOnDie(running->pid());
    } else {
      ESLANGEXCEPT("Proc is already dead");
    }
  }

  // for now, processes don't move, so can keep event base pointers
  boost::asio::io_service& ioService() { return ioService_; }

private:
  friend class Process;
  void queueResume(Pid p, uint64_t expected_resumes);
  void queueSend(SendAddress a, MessageBase m);

  Pid nextPid();

  template <class T, class... Args> Pid spawnArgs(ProcessArgs a, Args... args) {
    a.c = this;
    auto p = std::make_unique<T>(a, std::forward<Args>(args)...);
    auto res = p->run();
    if (res.done()) {
      ESLANGEXCEPT("Expected done to be false");
    }
    processes_[a.pid.idx()] = std::make_unique<RunningProcess>(
        a.pid, std::move(p), std::move(res), this);
    queueResume(a.pid, 0);
    return a.pid;
  }

  struct RunningProcess {
    RunningProcess(Pid pid, std::unique_ptr<Process> proc, ProcessTask t,
                   Context* parent);
    void send(SendAddress s, MessageBase m);
    bool waitingFor(SlotId s) const;
    void resume();
    bool dead = false;
    Pid pid;
    std::unique_ptr<Process> process;
    ProcessTask task;
    IWaiting* lastWaiting = nullptr;
    Context* parent;
    uint64_t resumes = 0;
    boost::asio::deadline_timer timer;
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
  boost::asio::io_service ioService_;
};

   template <class T, class... Args> Pid Process::spawn(Args... args) {
    return c_->spawn<T>(std::forward<Args>(args)...);
  }

  template <class T, class... Args> Pid Process::spawnLink(Args... args) {
    auto new_pid =
        c_->spawnWith<T>([p = this->pid_](ProcessArgs & a) { a.killOnDie = p; },
                         std::forward<Args>(args)...);
    addKillOnDie(new_pid);
    return new_pid;
  }

  // spawn a process, link it to us (if we die), but notify us if they die
  template <class T, class... Args>
    Pid Process::spawnLinkNotify(TSendAddress<Pid> send_address, Args... args) {
    auto new_pid =
        c_->spawnWith<T>([s = std::move(send_address)](
                             ProcessArgs & a) { a.notifyOnDead = s; },
                         std::forward<Args>(args)...);
    addKillOnDie(new_pid);
    return new_pid;
  }

  template <class T, class... Args>
  WaitingMaybe Process::send(TSendAddress<T> p, Args&&... params) {
    c_->queueSend(p, Message<T>(std::forward<Args>(params)...));
    return WaitingMaybe(c_->waitOnQueue());
  }

  template <class T, class Y, class... Args>
    WaitingMaybe Process::send(Pid pid, Slot<T> Y::*slot, Args&&... params) {
    return send(c_->makeSendAddress(pid, slot), std::forward<Args>(params)...);
  }

}
