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
    std::experimental::coroutine_handle<MethodTaskPromiseType>::
        from_promise(
            *static_cast<MethodTaskPromiseType*>(subCoroutineChild))
            .destroy();
  }
}

void SuspendRunNext::await_suspend(
  std::experimental::coroutine_handle<MethodTaskPromiseType> h
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

MethodTaskPromiseType* MethodTaskPromiseType::methodTaskParentPromise() {
  ESLANGREQUIRE(this->parent, "No parent");
  ESLANGREQUIRE(this->parentIsMethod, "Parent is wrong type");
  return static_cast<MethodTaskPromiseType*>(parent);
}

std::experimental::coroutine_handle<> MethodTaskPromiseType::parentHandle() {
  ESLANGREQUIRE(parent, "No parent");
  if (parentIsMethod) {
    return std::experimental::coroutine_handle<MethodTaskPromiseType>::from_promise(methodTaskParentPromise());
  }
  return std::experimental::coroutine_handle<ProcessTaskPromiseType>::from_promise(*parent);
}

MethodTask MethodTaskPromiseType::get_return_object() {
  return MethodTask(std::experimental::coroutine_handle<
                        MethodTaskPromiseType>::from_promise(*this));
}

bool ProcessTask::resume() {
  ProcessTaskPromiseType* child = coroutine_.promise().subCoroutineChild;
  while (child && child->subCoroutineChild) {
    child = child->subCoroutineChild;
  }
  coroutine_.promise().waiting = nullptr;
  try {
    if (child) {
      std::experimental::coroutine_handle<MethodTaskPromiseType>::from_promise(
        static_cast<MethodTaskPromiseType&>(*child)
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
