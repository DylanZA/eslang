#pragma once
#include <experimental/coroutine>

#include "Except.h"

namespace s {

template <class Ret> class CoroutineHandle {
public:
  CoroutineHandle(std::experimental::coroutine_handle<Ret> c) : coroutine_(c) {}
  CoroutineHandle(CoroutineHandle const&) = delete;
  CoroutineHandle(CoroutineHandle&& rhs) : coroutine_(rhs.coroutine_) {
    rhs.coroutine_ = nullptr;
  }
  CoroutineHandle& operator=(CoroutineHandle const&) = delete;
  CoroutineHandle& operator=(CoroutineHandle&& rhs) {
    if (&rhs != this) {
      coroutine_ = rhs.coroutine_;
      rhs.coroutine_ = nullptr;
    }
    return *this;
  }

  void clear() { coroutine_ = nullptr; }

  ~CoroutineHandle() {
    if (coroutine_) {
      coroutine_.destroy();
    }
  }

  void resume() { coroutine_.resume(); }

  std::experimental::coroutine_handle<Ret>& get() { return coroutine_; }
  std::experimental::coroutine_handle<Ret> const& get() const {
    return coroutine_;
  }

private:
  std::experimental::coroutine_handle<Ret> coroutine_ = nullptr;
};
}
