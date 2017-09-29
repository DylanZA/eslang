#pragma once
#include <chrono>
#include <experimental/coroutine>
#include <folly/futures/Future.h>
#include <optional>
#include <vector>

#include "BaseTypes.h"
#include "Except.h"
#include <variant>

namespace s {

struct IWaiting;
class ProcessTask;

struct ProcessTaskPromiseType {
  IWaiting* waiting = nullptr;
  ProcessTaskPromiseType* subCoroutineChild = nullptr;
  auto initial_suspend() { return std::experimental::suspend_always{}; }
  auto final_suspend() { 
    return std::experimental::suspend_always{}; 
  }
  void return_void() {}
  ProcessTask get_return_object();
  ~ProcessTaskPromiseType();
};

class SubProcessTask;
struct SubProcessTaskPromiseType;

struct SuspendRunNext {
  std::experimental::coroutine_handle<> run;
  SuspendRunNext(std::experimental::coroutine_handle<> r = {}) : run(r) {}
  bool await_ready() {
    return false;
  }
  void await_resume() {}
  void await_suspend(
    std::experimental::coroutine_handle<SubProcessTaskPromiseType> h
  );
};

struct SubProcessTaskPromiseType : ProcessTaskPromiseType {
  // hack, msvc cannot have a coroutine_handle<T> inside of T :(
  // (or I cannot fix this)
  // if we are suspended, then one of these should be set so we can resume our parent
  ProcessTaskPromiseType* parent = nullptr;;
  bool parentIsSubProcess = false;
  SubProcessTaskPromiseType* subProcessParentPromise();
  std::experimental::coroutine_handle<> parentHandle();

  auto final_suspend() {
    // make sure our parent knows that it's child is dead now,
    // and then run the parent
    if (parent) {
      parent->subCoroutineChild = nullptr;
      return SuspendRunNext{ parentHandle() };
    }
    else {
      // else we were never actually suspended, so nothing to do
      return SuspendRunNext{ };
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
    std::experimental::coroutine_handle<promise_type> coroutine = {})
    : coroutine_(coroutine) {
  }

  // these are used when you await on a subprocess task
  bool await_ready() {
    if (!coroutine_) {
      return true;
    } else if (coroutine_.done()) {
      // can await_ready have side effects?
      // not sure
      coroutine_.destroy();
      coroutine_ = {};
      return true;
    } else if (!coroutine_.promise().waiting) {
      return true;
    }
    return false;
  }

  void await_resume() {}

  void await_suspend(std::experimental::coroutine_handle<ProcessTaskPromiseType>
                     parent) noexcept {
    coroutine_.promise().parent = &parent.promise();

    // propogate the IWaiting up the stack
    parent.promise().waiting = coroutine_.promise().waiting;
    parent.promise().subCoroutineChild = &coroutine_.promise();
  }

  void await_suspend(
    std::experimental::coroutine_handle<SubProcessTaskPromiseType> parent
  ) noexcept {
    auto& our_promise = coroutine_.promise();
    parent.promise().subCoroutineChild = &our_promise;
    auto* parent_promise = &parent.promise();

    our_promise.parent = parent_promise;
    our_promise.parentIsSubProcess = true;

    IWaiting* waiting = our_promise.waiting;

    // propogate the new IWaiting up the stack
    // this is in case A calls B calls C then IWaiting gets to the top
    // but when we resume C it waits again, now we have to re-propogate
    // but we dont get the benefit of await_suspend being called
    // could probably be done better, but for now api > performance
    SubProcessTaskPromiseType* at = &our_promise;
    while (at->parent) {
      at->parent->waiting = waiting;
      if (!at->parentIsSubProcess) {
        break;
      }
      at = at->subProcessParentPromise();
    }
  }

private:
  std::experimental::coroutine_handle<promise_type> coroutine_;
};
}
