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
  ~ProcessTaskPromiseType();
};

struct ProcessTaskPromiseTypeWithReturn : ProcessTaskPromiseType {
  void return_void() {}
  ProcessTask get_return_object();
};


template<class T = void>
class MethodTask;

template<class T = void>
struct MethodTaskPromiseTypeWithReturn;

struct SuspendRunNext {
  std::experimental::coroutine_handle<> run;
  SuspendRunNext(std::experimental::coroutine_handle<> r = {}) : run(r) {}
  bool await_ready() {
    return false;
  }
  void await_resume() {}
  template<class T>
  void await_suspend(
    std::experimental::coroutine_handle<MethodTaskPromiseTypeWithReturn<T>> h
  ) {
    if (run) {
      // got to copy this or else we may be destroyed
      auto r = run;
      h.destroy();
      r.resume();
    }
  }
};

struct MethodTaskPromiseType : ProcessTaskPromiseType {
  // hack, msvc cannot have a coroutine_handle<T> inside of T :(
  // (or I cannot fix this)
  // if we are suspended, then one of these should be set so we can resume our parent
  ProcessTaskPromiseType* parent = nullptr;;
  bool parentIsMethod = false;
  MethodTaskPromiseType* methodTaskParentPromise();
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
};

template<>
struct MethodTaskPromiseTypeWithReturn<void> : MethodTaskPromiseType
{
  MethodTask<void> get_return_object();
  void return_void() {}
};

template<class T>
struct MethodTaskPromiseTypeWithReturn : MethodTaskPromiseType
{
  T t_;
  void return_value(T t) {
    t_ = std::move(t);
  }
  MethodTask<T> get_return_object();
};

class ProcessTask {
public:
  using promise_type = ProcessTaskPromiseTypeWithReturn;
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

template<class T>
class MethodTaskBase {
public:
  using promise_type = MethodTaskPromiseTypeWithReturn<T>;
  explicit MethodTaskBase(
    std::experimental::coroutine_handle<promise_type> coroutine = {})
    : coroutine_(coroutine) {
  }

  bool await_ready() {
    if (!coroutine_) {
      return true;
    }
    else if (coroutine_.done()) {
      // can await_ready have side effects?
      // not sure
      coroutine_.destroy();
      coroutine_ = {};
      return true;
    }
    else if (!coroutine_.promise().waiting) {
      return true;
    }
    return false;
  }

  void await_suspend(std::experimental::coroutine_handle<ProcessTaskPromiseTypeWithReturn>
                     parent) noexcept {
    coroutine_.promise().parent = &parent.promise();

    // propogate the IWaiting up the stack
    parent.promise().waiting = coroutine_.promise().waiting;
    parent.promise().subCoroutineChild = &coroutine_.promise();
  }

  void await_suspend(
    std::experimental::coroutine_handle<MethodTaskPromiseTypeWithReturn<T>> parent
  ) noexcept {
    auto& our_promise = coroutine_.promise();
    parent.promise().subCoroutineChild = &our_promise;
    auto* parent_promise = &parent.promise();

    our_promise.parent = parent_promise;
    our_promise.parentIsMethod = true;

    IWaiting* waiting = our_promise.waiting;

    // propogate the new IWaiting up the stack
    // this is in case A calls B calls C then IWaiting gets to the top
    // but when we resume C it waits again, now we have to re-propogate
    // but we dont get the benefit of await_suspend being called
    // could probably be done better, but for now api > performance
    MethodTaskPromiseType* at = &our_promise;
    while (at->parent) {
      at->parent->waiting = waiting;
      if (!at->parentIsMethod) {
        break;
      }
      at = at->methodTaskParentPromise();
    }
  }

protected:
  std::experimental::coroutine_handle<promise_type> coroutine_;
};

template<>
class MethodTask<void> : public MethodTaskBase<void> {
public:
  using MethodTaskBase::MethodTaskBase;
  void await_resume() {
  }
};

template<class T>
class MethodTask : public MethodTaskBase<T> {
public:
  using MethodTaskBase::MethodTaskBase;

  MethodTask(T t) : t_(std::move(t)) {}
  T t_;

  T await_resume() {
    return std::move(t_);
  }

  bool await_ready() {
    if (!coroutine_) {
      return true;
    }
    else if (coroutine_.done()) {
      // can await_ready have side effects?
      // not sure
      t_ = std::move(coroutine_.promise().t_);
      coroutine_.destroy();
      coroutine_ = {};
      return true;
    }
    else if (!coroutine_.promise().waiting) {
      return true;
    }
    return false;
  }
};

template<class T>
MethodTask<T> MethodTaskPromiseTypeWithReturn<T>::get_return_object() {
  return MethodTask<T>(
    std::experimental::coroutine_handle<MethodTaskPromiseTypeWithReturn<T>>::from_promise(
      *this));
}

}
