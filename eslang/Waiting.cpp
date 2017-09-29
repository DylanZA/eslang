#include "IWaiting.h"
#include "ProcessTask.h"

namespace s {
void IWaiting::await_suspend(
    std::experimental::coroutine_handle<ProcessTaskPromiseTypeWithReturn>
        handle) noexcept {
  handle.promise().waiting = this;
}
}
