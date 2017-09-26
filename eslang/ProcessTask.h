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

template <class TPromise> class OwnedCoroutine {
public:
  OwnedCoroutine() = default;
  explicit OwnedCoroutine(std::experimental::coroutine_handle<TPromise> h)
      : h_(h) {}
  OwnedCoroutine(OwnedCoroutine const&) = delete;
  OwnedCoroutine& operator=(OwnedCoroutine const&) = delete;
  OwnedCoroutine(OwnedCoroutine&& rhs) : h_(rhs.h_) { rhs.h_ = {}; }
  OwnedCoroutine& operator=(OwnedCoroutine&& rhs) {
    std::swap(rhs.h_, h_);
    return *this;
  }
  std::experimental::coroutine_handle<TPromise> get() { return h_; }
  ~OwnedCoroutine() {
    if (h_) {
      h_.destroy();
    }
  }
  void resume() {
    try {
      h_.resume();
    } catch (std::exception const&) {
      // h_ is now dead
      h_ = {};
      throw;
    }
  }
  TPromise& promise() { return h_.promise(); }
  TPromise const& promise() const { return h_.promise(); }
  bool hasValue() const { return static_cast<bool>(h_); }
  bool atFinalSuspend() const { return h_.done(); }
  void destroy() {
    auto h = h_;
    h_ = {};
    h.destroy();
  }

private:
  std::experimental::coroutine_handle<TPromise> h_ = {};
};

struct PromiseTypeBase {
  IWaiting* waiting = nullptr;
  PromiseTypeBase* subCoroutineChild = nullptr;
  auto initial_suspend() { return std::experimental::suspend_always{}; }
  auto final_suspend() { return std::experimental::suspend_always{}; }
  virtual std::experimental::coroutine_handle<> getHandle() {
    return std::experimental::coroutine_handle<PromiseTypeBase>::from_promise(
        *this);
  }
  ~PromiseTypeBase();
};

struct PromiseTypeBaseWithReturn : PromiseTypeBase {
  void return_void() {}
  ProcessTask get_return_object();
  std::experimental::coroutine_handle<> getHandle() override {
    return std::experimental::coroutine_handle<
        PromiseTypeBaseWithReturn>::from_promise(*this);
  }
};

template <class T = void> class MethodTask;

template <class T = void> struct MethodTaskPromiseTypeWithReturn;

template <class T> struct GenTask;

struct SuspendRunNext {
  std::experimental::coroutine_handle<> run;
  SuspendRunNext(std::experimental::coroutine_handle<> r = {}) : run(r) {}
  bool await_ready() { return false; }
  void await_resume() {}
  void await_suspend(std::experimental::coroutine_handle<>) {
    if (run) {
      run.resume();
    }
  }
};

template <class T>
void updateSuspend(PromiseTypeBaseWithReturn& parent, T& our_promise) noexcept {
  our_promise.parent = &parent;

  // propogate the IWaiting up the stack
  parent.waiting = our_promise.waiting;
  parent.subCoroutineChild = &our_promise;
}

template <class T, class U>
void updateSuspend(U& parent_promise, T& our_promise) noexcept {
  parent_promise.subCoroutineChild = &our_promise;

  our_promise.parent = &parent_promise;

  IWaiting* waiting = our_promise.waiting;

  // propogate the new IWaiting up the stack
  // this is in case A calls B calls C then IWaiting gets to the top
  // but when we resume C it waits again, now we have to re-propogate
  // but we dont get the benefit of await_suspend being called
  // could probably be done better, but for now api > performance
  MethodTaskPromiseType* at = &our_promise;
  while (at && at->parent) {
    at->parent->waiting = waiting;
    at = at->methodTaskParentPromise();
  }
}

struct MethodTaskPromiseType : PromiseTypeBase {
  // hack, msvc cannot have a coroutine_handle<T> inside of T :(
  // (or I cannot fix this)
  // if we are suspended, then one of these should be set so we can resume our
  // parent
  PromiseTypeBase* parent = nullptr;
  MethodTaskPromiseType* methodTaskParentPromise();
  std::experimental::coroutine_handle<> parentHandle();

  std::experimental::coroutine_handle<> getHandle() override {
    return std::experimental::coroutine_handle<
        MethodTaskPromiseType>::from_promise(*this);
  }

  auto final_suspend() {
    // make sure our parent knows that it's child is dead now,
    // and then run the parent
    if (parent) {
      parent->subCoroutineChild = nullptr;
      return SuspendRunNext{parentHandle()};
    } else {
      // else we were never actually suspended, so nothing to do
      return SuspendRunNext{};
    }
  }
  auto initial_suspend() { return std::experimental::suspend_never{}; }
};

template <>
struct MethodTaskPromiseTypeWithReturn<void> : MethodTaskPromiseType {
  MethodTask<void> get_return_object();
  void return_void() {}
  std::experimental::coroutine_handle<> getHandle() override {
    return std::experimental::coroutine_handle<
        MethodTaskPromiseTypeWithReturn<void>>::from_promise(*this);
  }
};

template <class T>
struct MethodTaskPromiseTypeWithReturn : MethodTaskPromiseType {
  T t_;
  void return_value(T t) { t_ = std::move(t); }
  MethodTask<T> get_return_object();
  std::experimental::coroutine_handle<> getHandle() override {
    return std::experimental::coroutine_handle<
        MethodTaskPromiseTypeWithReturn<T>>::from_promise(*this);
  }
};

template <class T> struct YieldPromiseType : MethodTaskPromiseType {
  // todo: make this not an optional (just needed it as a quick hack for the
  // movable stuff)
  folly::Optional<T> t_;

  struct YieldSuspender {
    bool await_ready() { return false; }
    void await_resume() {}

    template <class U>
    void await_suspend(std::experimental::coroutine_handle<U> h) {
      // indicate we are not waiting, ie we have a value
      h.promise().waiting = nullptr;
      // We cannot actually bubble up to the process here, as the process would
      // never know that we are ready, so just run the parent
      // This isn't really a "just", what is in effect happening is inversion of
      // the stack, and now when we call co_yield, it is as if we are pushing
      // values into our parent.
      // So if a calls b, now b is calling a.
      // This is even more heinous as this only happens once we know who our
      // parent is. In the inital yield we don't know, but that is ok as our
      // parent has not suspended, hence the check in NextAwaitable::await_ready
      if (auto parent = h.promise().parent) {
        // we have to clear the parent here, in case we get a recursive call to
        // this coroutine.
        // after we have resumed we cannot know that anything is still alive, so
        // cannot reset it. this is ok as if it suspends parent will be reset.
        h.promise().parent = nullptr;
        parent->subCoroutineChild = nullptr;
        parent->getHandle().resume();
      }
    }
  };

  auto yield_value(T t) {
    t_ = std::move(t);
    return YieldSuspender{};
  }
  GenTask<T> get_return_object();
  auto initial_suspend() { return std::experimental::suspend_always{}; }
  auto final_suspend() { return std::experimental::suspend_always{}; }
  std::experimental::coroutine_handle<> getHandle() override {
    return std::experimental::coroutine_handle<
        YieldPromiseType<T>>::from_promise(*this);
  }
};

class ProcessTask {
public:
  using promise_type = PromiseTypeBaseWithReturn;
  explicit ProcessTask(
      std::experimental::coroutine_handle<promise_type> coroutine)
      : coroutine_(coroutine) {}
  ProcessTask(ProcessTask&&) = default;

  bool done() const { return !coroutine_.hasValue(); }

  IWaiting* waiting() { return coroutine_.promise().waiting; }

  IWaiting const* waiting() const { return coroutine_.promise().waiting; }

  bool resume();

private:
  OwnedCoroutine<promise_type> coroutine_;
};

template <class T, class TPromise> class MethodTaskBase {
public:
  using promise_type = TPromise;
  explicit MethodTaskBase(
      std::experimental::coroutine_handle<promise_type> coroutine = {})
      : coroutine_(coroutine) {}

  bool await_ready() {
    if (!coroutine_.hasValue() || coroutine_.atFinalSuspend() ||
        !coroutine_.promise().waiting) {
      return true;
    }
    return false;
  }

  template <class T>
  void await_suspend(std::experimental::coroutine_handle<T> parent) noexcept {
    updateSuspend(parent.promise(), coroutine_.promise());
  }

protected:
  OwnedCoroutine<promise_type> coroutine_;
};

template <>
class MethodTask<void>
    : public MethodTaskBase<void, MethodTaskPromiseTypeWithReturn<void>> {
public:
  using MethodTaskBase::MethodTaskBase;
  void await_resume() {}
};

template <class T>
class MethodTask
    : public MethodTaskBase<T, MethodTaskPromiseTypeWithReturn<T>> {
public:
  using MethodTaskBase::MethodTaskBase;
  T& await_resume() { return coroutine_.promise().t_; }
};

template <class T> struct GenTask {
public:
  using promise_type = YieldPromiseType<T>;
  explicit GenTask(std::experimental::coroutine_handle<promise_type> h)
      : coroutine_(h) {}

  struct NextAwaitable {
    GenTask<T>* gen;
    NextAwaitable(GenTask<T>* gen) : gen(gen) {}

    bool await_ready() { return gen->coroutine_.promise().waiting == nullptr; }

    bool await_resume() {
      // if we are done, then there are no more values left to yield
      return !gen->coroutine_.atFinalSuspend();
    }

    template <class TSuspenderPromise>
    void await_suspend(
        std::experimental::coroutine_handle<TSuspenderPromise> parent_handle) {
      updateSuspend(parent_handle.promise(), gen->coroutine_.promise());
    }
  };
  T& take() { return coroutine_.promise().t_.value(); }

  NextAwaitable next() {
    coroutine_.resume();
    return NextAwaitable(this);
  }

  struct Iterator {
    NextAwaitable next;
    Iterator(NextAwaitable next) : next(std::move(next)) {}
    bool operator==(Iterator const& rhs) const {
      return rhs.next.gen == next.gen;
    }
    bool operator!=(Iterator const& rhs) const { return !operator==(rhs); }
    bool await_ready() { return next.await_ready(); }
    void await_resume() {
      if (!next.await_resume()) {
        next = NextAwaitable(nullptr);
      }
    }
    template <class T> void await_suspend(T t) {
      next.await_suspend(std::move(t));
    }
    T& operator*() { return next.gen->take(); }
    Iterator& operator++() {
      next = next.gen->next();
      return *this;
    }
  };
  struct IteratorAwaitable {
    NextAwaitable next;
    IteratorAwaitable(GenTask<T>* gen) : next(gen->next()) {}
    bool await_ready() { return next.await_ready(); }
    Iterator await_resume() {
      bool valid = next.await_resume();
      return valid ? Iterator(std::move(next))
                   : Iterator(NextAwaitable(nullptr));
    }
    template <class T> void await_suspend(T t) {
      next.await_suspend(std::move(t));
    }
  };

  IteratorAwaitable begin() { return IteratorAwaitable(this); }
  Iterator end() { return Iterator(NextAwaitable(nullptr)); }

private:
  OwnedCoroutine<promise_type> coroutine_;
};

template <class T>
MethodTask<T> MethodTaskPromiseTypeWithReturn<T>::get_return_object() {
  return MethodTask<T>(
      std::experimental::coroutine_handle<
          MethodTaskPromiseTypeWithReturn<T>>::from_promise(*this));
}

template <class T> GenTask<T> YieldPromiseType<T>::get_return_object() {
  return GenTask<T>(
      std::experimental::coroutine_handle<YieldPromiseType<T>>::from_promise(
          *this));
}
}
