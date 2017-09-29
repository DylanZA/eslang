#include "ProcessTask.h"
#include "IWaiting.h"

namespace s {

ProcessTask ProcessTaskPromiseType::get_return_object() {
  return ProcessTask(
      std::experimental::coroutine_handle<ProcessTaskPromiseType>::from_promise(
          *this));
}

ProcessTaskPromiseType::~ProcessTaskPromiseType() {
  if (subCoroutineChild) {
    std::experimental::coroutine_handle<SubProcessTaskPromiseType>::
        from_promise(
            *static_cast<SubProcessTaskPromiseType*>(subCoroutineChild))
            .destroy();
  }
}

std::experimental::coroutine_handle<SubProcessTaskPromiseType>
SubProcessTaskPromiseType::subTaskParent() {
  return std::experimental::coroutine_handle<
      SubProcessTaskPromiseType>::from_promise(*this->subTaskParentPromise);
}

SubProcessTask SubProcessTaskPromiseType::get_return_object() {
  return SubProcessTask(std::experimental::coroutine_handle<
                        SubProcessTaskPromiseType>::from_promise(*this));
}

bool ProcessTask::resume() {
  std::experimental::coroutine_handle<> subhandle = {};
  if (this->waiting()) {
    subhandle = this->waiting()->subProcessResume;
  }
  coroutine_.promise().waiting = nullptr;
  try {
    if (subhandle) {
      subhandle.resume();
    } else {
      coroutine_.resume();
    }
    if (coroutine_.done()) {
      coroutine_ = {};
      return true;
    }
    return false;
  } catch (std::exception const&) {
    coroutine_ = {};
    throw;
  }
}
}
