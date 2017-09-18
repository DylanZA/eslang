#pragma once
#include <chrono>
#include <experimental/coroutine>
#include <folly/futures/Future.h>
#include <optional>
#include <vector>

#include "BaseTypes.h"
#include "CoroutineHandle.h"
#include "Except.h"

namespace s {

struct IWaiting;
class ProcessTask;
struct ProcessTaskPromiseType {
  IWaiting* waiting = nullptr;
  auto initial_suspend() { return std::experimental::suspend_always{}; }
  auto final_suspend() { return std::experimental::suspend_always{}; }
  void return_void() {}
  ProcessTask get_return_object();
};

class ProcessTask {
public:
  using promise_type = ProcessTaskPromiseType;
  explicit ProcessTask(
      std::experimental::coroutine_handle<promise_type> coroutine)
      : coroutine_(coroutine) {}

  bool done() const { return !static_cast<bool>(coroutine_.get()); }

  IWaiting* waiting() { return coroutine_.get().promise().waiting; }

  IWaiting const* waiting() const { return coroutine_.get().promise().waiting; }

  bool resume();

private:
  CoroutineHandle<promise_type> coroutine_;
};
}
