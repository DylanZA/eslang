#include "IWaiting.h"
#include "ProcessTask.h"

namespace s {

void IWaiting::await_suspend(
    std::experimental::coroutine_handle<ProcessTaskPromiseType>
        handle) noexcept {
  handle.promise().waiting = this;
}
}
