#pragma once
#include <chrono>
#include <experimental/coroutine>
#include <optional>
#include <vector>

#include "BaseTypes.h"
#include "Except.h"
#include "Future.h"

namespace s {

struct ProcessPromise;

template <class T> struct MethodTaskPromiseWithReturn;

struct IWaiting {
  virtual bool isReadyForResume() const { return false; }
  virtual std::optional<std::chrono::milliseconds> sleepFor() const {
    return {};
  }

  virtual EslangPromise* wakeOnFuture() { return nullptr; }

  virtual bool isWaiting(SlotId s) const { return false; }

  template <class TPromise>
  void
  await_suspend(std::experimental::coroutine_handle<TPromise> handle) noexcept {
    handle.promise().waiting = this;
    if (handle.promise().processPromise) {
      handle.promise().processPromise->nextChild = &handle.promise();
    }
  }
};

} // namespace s
