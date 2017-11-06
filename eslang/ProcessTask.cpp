#include "ProcessTask.h"
#include "IWaiting.h"

namespace s {

ProcessTask ProcessPromise::get_return_object() {
  return ProcessTask(std::experimental::coroutine_handle<
                     ProcessPromise>::from_promise(*this));
}

MethodTaskPromise* MethodTaskPromise::methodTaskParentPromise() {
  ESLANGREQUIRE(this->parent, "No parent");
  // todo: not use dynamic cast
  return dynamic_cast<MethodTaskPromise*>(parent);
}

std::experimental::coroutine_handle<> MethodTaskPromise::parentHandle() {
  ESLANGREQUIRE(parent, "No parent");
  return parent->getHandle();
}

MethodTask<void> MethodTaskPromiseWithReturn<>::get_return_object() {
  return MethodTask<void>(
      std::experimental::coroutine_handle<
          MethodTaskPromiseWithReturn<void>>::from_promise(*this));
}

IWaiting* ProcessTask::resume() {
  PromiseBase* child = coroutine_.promise().subCoroutineChild;
  while (child && child->subCoroutineChild) {
    child = child->subCoroutineChild;
  }
  coroutine_.promise().waiting = nullptr;
  // if these throw, the context will notice and clean up properly
  if (child) {
    child->getHandle().resume();
  } else {
    coroutine_.resume();
  }
  if (coroutine_.atFinalSuspend()) {
    coroutine_.destroy();
    return nullptr;
  }
  return coroutine_.promise().waiting;
}
}
