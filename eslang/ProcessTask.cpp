#include "ProcessTask.h"

namespace s {

ProcessTask ProcessTaskPromiseType::get_return_object() {
  return ProcessTask(
      std::experimental::coroutine_handle<ProcessTaskPromiseType>::from_promise(
          *this));
}

bool ProcessTask::resume() {
  coroutine_.get().promise().waiting = nullptr;
  try {
    coroutine_.get().resume();
    if (coroutine_.get().done()) {
      coroutine_.clear();
      return true;
    }
    return false;
  } catch (std::exception const&) {
    coroutine_.clear();
    throw;
  }
}
}
