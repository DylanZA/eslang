#pragma once
#include <chrono>
#include <cstdint>
#include <experimental/coroutine>
#include <optional>
#include <tuple>
#include <unordered_set>

#include "BaseTypes.h"
#include "Except.h"
#include "Slot.h"
#include "Waiting.h"

namespace s {

class Context;
class SlotBase;
struct ProcessArgs {
  explicit ProcessArgs(Pid p) : pid(p) {}
  Context* c = nullptr;
  Pid pid;
  std::optional<TSendAddress<Pid>> notifyOnDead;
  std::optional<Pid> killOnDie;
};

class Process {
public:
  Process(ProcessArgs args) : c_(args.c), pid_(args.pid) {
    if (args.killOnDie) {
      addKillOnDie(*args.killOnDie);
    }
    if (args.notifyOnDead) {
      notifyOnDie_.push_back(std::move(*args.notifyOnDead));
    }
  }

  virtual ~Process() = default;
  void* getSlotId(SlotBase* slot) { return slot; }

  void push(SlotId slot, MessageBase m) {
    static_cast<SlotBase*>(slot)->push(std::move(m));
  }

  Pid pid() { return pid_; }

  TimePoint now() const;

  void addKillOnDie(Pid b) { killOnDie_.push_back(b); }
  std::vector<Pid> const& killOnDie() const { return killOnDie_; }
  std::vector<TSendAddress<Pid>> const& notifyOnDie() const {
    return notifyOnDie_;
  }

  template <class T, class... Args> Pid spawn(Args... args);
  template <class T, class... Args> Pid spawnLink(Args... args);
  // spawn a process, link it to us (if we die), but notify us if they die
  template <class T, class... Args>
  Pid spawnLinkNotify(TSendAddress<Pid> send_address, Args... args);

  template <class T, class... Args>
  WaitingMaybe send(TSendAddress<T> p, Args&&... params);

  template <class T, class Y, class... Args>
  WaitingMaybe send(Pid pid, Slot<T> Y::*slot, Args&&... params);

  Context* c() { return c_; }

  template <class... TSlots>
  WithWaitingTimeout<WaitingMessages<TSlots...>>
  timedRecv(std::chrono::milliseconds time, Slot<TSlots>&... slots) {
    TimePoint t = now() + time;
    return WithWaitingTimeout<WaitingMessages<TSlots...>>(t, slots...);
  }

  template <class... TSlots>
  WaitingTimeout sleep(std::chrono::milliseconds time) {
    return WaitingTimeout(time);
  }

  template <class... TSlots>
  WaitingMessages<TSlots...> tryRecv(Slot<TSlots>&... slots) {
    return WaitingMessages<TSlots...>(slots...);
  }

  template <class T> WaitingMessage<T> recv(Slot<T>& slot) {
    return WaitingMessage<T>(this->tryRecv<T>(slot));
  }

  void queueKill(Pid p);

protected:
  void link(Pid p);

protected:
  Context* c_;
  Pid pid_;

private:
  std::vector<Pid> killOnDie_;
  std::vector<TSendAddress<Pid>> notifyOnDie_;
};
}
