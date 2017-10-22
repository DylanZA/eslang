#include "ProcessTask.h"
#include "IWaiting.h"

namespace s {

ProcessTask PromiseTypeBaseWithReturn::get_return_object() {
  return ProcessTask(std::experimental::coroutine_handle<
                     PromiseTypeBaseWithReturn>::from_promise(*this));
}

PromiseTypeBase::~PromiseTypeBase() {
  if (subCoroutineChild) {
    // subCoroutineChild->getHandle().destroy();
  }
}

MethodTaskPromiseType* MethodTaskPromiseType::methodTaskParentPromise() {
  ESLANGREQUIRE(this->parent, "No parent");
  // todo: not use dynamic cast
  return dynamic_cast<MethodTaskPromiseType*>(parent);
}

std::experimental::coroutine_handle<> MethodTaskPromiseType::parentHandle() {
  ESLANGREQUIRE(parent, "No parent");
  return parent->getHandle();
}

MethodTask<void> MethodTaskPromiseTypeWithReturn<>::get_return_object() {
  return MethodTask<void>(
      std::experimental::coroutine_handle<
          MethodTaskPromiseTypeWithReturn<void>>::from_promise(*this));
}

bool ProcessTask::resume() {
  PromiseTypeBase* child = coroutine_.promise().subCoroutineChild;
  while (child && child->subCoroutineChild) {
    child = child->subCoroutineChild;
  }
  coroutine_.promise().waiting = nullptr;
  if (child) {
    try {
      child->getHandle().resume();
    } catch (std::exception const&) {
      LOG(FATAL) << "child has thrown and is now dead, so we need to clean up "
                    "the thing that thinks it owns child";
    }
  } else {
    coroutine_.resume();
  }
  if (coroutine_.atFinalSuspend()) {
    coroutine_.destroy();
    return true;
  }
  return false;
}
}
