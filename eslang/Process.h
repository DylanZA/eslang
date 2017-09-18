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
};

class Process {
public:
  Process(ProcessArgs args) : c_(args.c), pid_(args.pid) {}

  virtual ~Process() = default;
  void* getSlotId(SlotBase* slot) { return slot; }

  void slotDestroyed(SlotBase* slot) { slots_.erase(slot); }

  void push(SlotId slot, MessageBase m) {
    static_cast<SlotBase*>(slot)->push(std::move(m));
  }

  Pid pid() { return pid_; }

  TimePoint now() const;

  void addLink(Pid b) { links_.insert(b); }

  std::unordered_set<Pid> const& links() const { return links_; }

  template <class T, class... Args> Pid spawn(Args... args) {
    return c_->spawn<T>(std::forward<Args>(args)...);
  }

  template <class T, class... Args> Pid spawnLink(Args... args) {
    auto pid = spawn<T>(std::forward<Args>(args)...);
    c_->link(this, pid);
    return pid;
  }

  template <class T, class... Args>
  WaitingMaybe send(TSendAddress<T> p, Args&&... params) {
    c_->send(p, Message<T>(std::forward<Args>(params)...));
    return WaitingMaybe(c_->waitOnQueue());
  }

  Context* c() { return c_; }

protected:
  template <class... TSlots>
  WithWaitingTimeout<WaitingMessages<TSlots...>>
  timedRecv(std::chrono::milliseconds time, Slot<TSlots>&... slots) {
    TimePoint t = now() + time;
    return WithWaitingTimeout<WaitingMessages<TSlots...>>(t, slots...);
  }

  template <class... TSlots>
  WaitingTimeout sleep(std::chrono::milliseconds time) {
    TimePoint t = now() + time;
    return WaitingTimeout(t);
  }

  template <class... TSlots>
  WaitingMessages<TSlots...> tryRecv(Slot<TSlots>&... slots) {
    return WaitingMessages<TSlots...>(std::forward<Slot<TSlots>>(slots)...);
  }

  template <class T> WaitingMessage<T> recv(Slot<T>& slot) {
    return WaitingMessage<T>(this->tryRecv<T>(slot));
  }

  void link(Pid p);

protected:
  Context* c_;
  Pid pid_;

private:
  std::unordered_set<Pid> links_;
  std::unordered_set<SlotId> slots_;
};
}
