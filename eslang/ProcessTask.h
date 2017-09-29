#pragma once
#include <chrono>
#include <experimental/coroutine>
#include <folly/futures/Future.h>
#include <optional>
#include <vector>

#include "BaseTypes.h"
#include "Except.h"

namespace s {

struct IWaiting;
class ProcessTask;

struct ProcessTaskPromiseType {
  IWaiting* waiting = nullptr;
  ProcessTaskPromiseType* subCoroutineChild = nullptr;
  auto initial_suspend() { return std::experimental::suspend_always{}; }
  auto final_suspend() { return std::experimental::suspend_always{}; }
  void return_void() {}
  ProcessTask get_return_object();
  ~ProcessTaskPromiseType();
};

class SubProcessTask;
struct SubProcessTaskPromiseType : ProcessTaskPromiseType {
  // hack, msvc cannot have a coroutine_handle<T> inside of T :(
  // (or I cannot fix this)
  SubProcessTaskPromiseType* subTaskParentPromise = nullptr;
  std::experimental::coroutine_handle<ProcessTaskPromiseType> fullParent = {};
  std::experimental::coroutine_handle<SubProcessTaskPromiseType>
  subTaskParent();

  struct SuspendRunNext : std::experimental::suspend_always {
    std::experimental::coroutine_handle<> run;

    SuspendRunNext(std::experimental::coroutine_handle<> r) : run(r) {}

    void await_resume() {}

    void await_suspend(
        std::experimental::coroutine_handle<SubProcessTaskPromiseType> h) {
      // got to copy this or else we may be destroyed
      auto r = run;
      h.destroy();
      r.resume();
    }
  };

  auto final_suspend() {
    // make sure our parent knows that it's child is dead now,
    // and then run the parent
    if (fullParent) {
      fullParent.promise().subCoroutineChild = nullptr;
      return SuspendRunNext{fullParent};
    } else {
      subTaskParentPromise->subCoroutineChild = nullptr;
      return SuspendRunNext{subTaskParent()};
    }
  }
  auto initial_suspend() { return std::experimental::suspend_never{}; }
  void return_void() {}

  SubProcessTask get_return_object();
};

class ProcessTask {
public:
  using promise_type = ProcessTaskPromiseType;
  explicit ProcessTask(
      std::experimental::coroutine_handle<promise_type> coroutine)
      : coroutine_(coroutine) {}
  ProcessTask(ProcessTask const&) = delete;
  ProcessTask& operator=(ProcessTask const&) = delete;
  ProcessTask(ProcessTask&& rhs) { std::swap(rhs.coroutine_, coroutine_); }
  ProcessTask& operator=(ProcessTask&& rhs) {
    std::swap(rhs.coroutine_, coroutine_);
  }
  ~ProcessTask() {
    if (!done()) {
      coroutine_.destroy();
    }
  }
  bool done() const { return !static_cast<bool>(coroutine_); }

  IWaiting* waiting() { return coroutine_.promise().waiting; }

  IWaiting const* waiting() const { return coroutine_.promise().waiting; }

  bool resume();

private:
  std::experimental::coroutine_handle<promise_type> coroutine_ = {};
};

class SubProcessTask {
public:
  using promise_type = SubProcessTaskPromiseType;
  explicit SubProcessTask(
      std::experimental::coroutine_handle<promise_type> coroutine)
      : coroutine_(coroutine) {}
  // these are used when you await on a process task
  bool await_ready() noexcept { return false; }

  void await_resume() {}

  void await_suspend(std::experimental::coroutine_handle<ProcessTaskPromiseType>
                         upward_handle) noexcept {
    // propogate the IWaiting up the stack
    coroutine_.promise().fullParent = upward_handle;
    upward_handle.promise().waiting = coroutine_.promise().waiting;
    upward_handle.promise().subCoroutineChild = &coroutine_.promise();
  }

  void
  await_suspend(std::experimental::coroutine_handle<SubProcessTaskPromiseType>
                    upward_handle) noexcept {
    auto& our_promise = coroutine_.promise();
    upward_handle.promise().subCoroutineChild = &our_promise;
    auto* up_promise = &upward_handle.promise();
    our_promise.subTaskParentPromise = &upward_handle.promise();
    IWaiting* waiting = our_promise.waiting;

    // propogate the new IWaiting up the stack
    // this is in case A calls B calls C then IWaiting gets to the top
    // but when we resume C it waits again, now we have to re-propogate
    // but we dont get the benefit of await_suspend being called
    // could probably be done better, but for now api > performance
    do {
      up_promise->waiting = waiting;
      if (up_promise->fullParent) {
        up_promise->fullParent.promise().waiting = waiting;
        break;
      }
      up_promise = up_promise->subTaskParentPromise;
    } while (up_promise);
  }

private:
  std::experimental::coroutine_handle<promise_type> coroutine_;
};
}
