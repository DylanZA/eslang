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

void SuspendRunNext::await_suspend(
  std::experimental::coroutine_handle<SubProcessTaskPromiseType> h
) {
  // got to copy this or else we may be destroyed
  if (run) {
    auto r = run;
    h.destroy();
    r.resume();
  }
  else {
    //h.destroy();
  }
}

SubProcessTaskPromiseType* SubProcessTaskPromiseType::subProcessParentPromise() {
  ESLANGREQUIRE(this->parent, "No parent");
  ESLANGREQUIRE(this->parentIsSubProcess, "Parent is wrong type");
  return static_cast<SubProcessTaskPromiseType*>(parent);
}

std::experimental::coroutine_handle<> SubProcessTaskPromiseType::parentHandle() {
  ESLANGREQUIRE(parent, "No parent");
  if (parentIsSubProcess) {
    return std::experimental::coroutine_handle<SubProcessTaskPromiseType>::from_promise(subProcessParentPromise());
  }
  return std::experimental::coroutine_handle<ProcessTaskPromiseType>::from_promise(*parent);
}

SubProcessTask SubProcessTaskPromiseType::get_return_object() {
  return SubProcessTask(std::experimental::coroutine_handle<
                        SubProcessTaskPromiseType>::from_promise(*this));
}

bool ProcessTask::resume() {
  ProcessTaskPromiseType* child = coroutine_.promise().subCoroutineChild;
  while (child && child->subCoroutineChild) {
    child = child->subCoroutineChild;
  }
  coroutine_.promise().waiting = nullptr;
  try {
    if (child) {
      std::experimental::coroutine_handle<SubProcessTaskPromiseType>::from_promise(
        static_cast<SubProcessTaskPromiseType&>(*child)
      ).resume();
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
