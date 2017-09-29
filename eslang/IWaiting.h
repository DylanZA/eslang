#pragma once
#include <chrono>
#include <experimental/coroutine>
#include <folly/futures/Future.h>
#include <optional>
#include <vector>

#include "BaseTypes.h"
#include "Except.h"

namespace s {

struct ProcessTaskPromiseTypeWithReturn;

template<class T>
struct MethodTaskPromiseTypeWithReturn;

struct IWaiting {
  virtual bool isReadyForResume() const { return false; }
  virtual std::vector<SendAddress> const* wakeOnSender() const {
    return nullptr;
  }

  virtual std::optional<TimePoint> wakeOnTime() const { return {}; }

  virtual folly::Future<folly::Unit>* wakeOnFuture() { return nullptr; }

  virtual bool isWaiting(SlotId s) const { return false; }

  void await_suspend(
    std::experimental::coroutine_handle<ProcessTaskPromiseTypeWithReturn> handle
  ) noexcept;

  template<class T>
  void await_suspend(
    std::experimental::coroutine_handle<MethodTaskPromiseTypeWithReturn<T>> handle
  ) noexcept {
    handle.promise().waiting = this;
  }
};

} // namespace s
