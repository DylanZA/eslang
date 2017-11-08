#include "ProcessTask.h"
#include "IWaiting.h"

namespace s {

namespace {
void updateProcessPromise(PromiseBase* at,
                          ProcessPromise* process_promise) noexcept {
  // need to propogate processPromise downward
  // this is the first time we have suspended all the way to the context, so
  // make sure the topmost process knows what to resume next also
  do {
    at->processPromise = process_promise;
    // make sure we know what to resume next
    process_promise->nextChild = at;

    at = at->subCoroutineChild;
  } while (at);
}
} // namespace

void updateSuspend(ProcessPromise& parent,
                   MethodTaskPromise& our_promise) noexcept {
  our_promise.parent = &parent;
  parent.subCoroutineChild = &our_promise;
  if (!our_promise.processPromise) {
    updateProcessPromise(&our_promise, &parent);
  }
}

void updateSuspend(MethodTaskPromise& parent_promise,
                   MethodTaskPromise& our_promise) noexcept {
  our_promise.parent = &parent_promise;
  parent_promise.subCoroutineChild = &our_promise;
  if (!our_promise.processPromise && parent_promise.processPromise) {
    updateProcessPromise(&our_promise, parent_promise.processPromise);
  }
}

ProcessTask ProcessPromise::get_return_object() {
  return ProcessTask(
      std::experimental::coroutine_handle<ProcessPromise>::from_promise(*this));
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
  PromiseBase* child = coroutine_.promise().nextChild;
  coroutine_.promise().waiting = nullptr;
  coroutine_.promise().nextChild = nullptr;
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
  if (coroutine_.promise().nextChild) {
    return coroutine_.promise().nextChild->waiting;
  }
  return coroutine_.promise().waiting;
}
}
