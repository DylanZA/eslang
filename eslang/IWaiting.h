#pragma once
#include <chrono>
#include <experimental/coroutine>
#include <folly/futures/Future.h>
#include <optional>
#include <vector>

#include "BaseTypes.h"
#include "Except.h"
#include "Future.h"

namespace s {

struct PromiseTypeBaseWithReturn;

template <class T> struct MethodTaskPromiseTypeWithReturn;

struct IWaiting {
  virtual bool isReadyForResume() const { return false; }
  virtual std::vector<SendAddress> const* wakeOnSender() const {
    return nullptr;
  }

  virtual std::optional<TimePoint> wakeOnTime() const { return {}; }

  virtual EslangPromise* wakeOnFuture() { return nullptr; }

  virtual bool isWaiting(SlotId s) const { return false; }

  template <class TPromise>
  void
  await_suspend(std::experimental::coroutine_handle<TPromise> handle) noexcept {
    handle.promise().waiting = this;
  }
};

} // namespace s
