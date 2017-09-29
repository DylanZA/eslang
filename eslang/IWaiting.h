#pragma once
#include <chrono>
#include <experimental/coroutine>
#include <folly/futures/Future.h>
#include <optional>
#include <vector>

#include "BaseTypes.h"
#include "Except.h"

namespace s {

struct ProcessTaskPromiseType;
struct SubProcessTaskPromiseType;

struct IWaiting {
  virtual bool isReadyForResume() const { return false; }
  virtual std::vector<SendAddress> const* wakeOnSender() const {
    return nullptr;
  }

  virtual std::optional<TimePoint> wakeOnTime() const { return {}; }

  virtual folly::Future<folly::Unit>* wakeOnFuture() { return nullptr; }

  virtual bool isWaiting(SlotId s) const { return false; }

  void await_suspend(std::experimental::coroutine_handle<ProcessTaskPromiseType>
                         handle) noexcept;
  void
  await_suspend(std::experimental::coroutine_handle<SubProcessTaskPromiseType>
                    handle) noexcept;
};

} // namespace s
