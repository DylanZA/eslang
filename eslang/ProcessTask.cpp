#include "ProcessTask.h"
#include "IWaiting.h"

namespace s {

ProcessTask ProcessTaskPromiseTypeWithReturn::get_return_object() {
  return ProcessTask(
      std::experimental::coroutine_handle<ProcessTaskPromiseTypeWithReturn>::from_promise(
          *this));
}

ProcessTaskPromiseType::~ProcessTaskPromiseType() {
  if (subCoroutineChild) {
    std::experimental::coroutine_handle<MethodTaskPromiseTypeWithReturn<>>::
        from_promise(
            *static_cast<MethodTaskPromiseTypeWithReturn<>*>(subCoroutineChild))
            .destroy();
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
    return std::experimental::coroutine_handle<MethodTaskPromiseType>::from_promise(
      *methodTaskParentPromise()
    );
  }
  return std::experimental::coroutine_handle<ProcessTaskPromiseType>::from_promise(*parent);
}

MethodTask<void> MethodTaskPromiseTypeWithReturn<>::get_return_object() {
  return MethodTask<void>(
    std::experimental::coroutine_handle<MethodTaskPromiseTypeWithReturn<void>>::from_promise(
      *this));
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
