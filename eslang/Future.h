#pragma once
#include <folly/futures/Future.h>
#include <folly/futures/SharedPromise.h>

namespace s {

class EslangPromise {
public:
  EslangPromise() = default;
  EslangPromise(EslangPromise&&) = delete;
  EslangPromise(EslangPromise const&) = delete;
  EslangPromise& operator=(EslangPromise const&) = delete;

  void setIfUnset() {
    if (!set_) {
      set();
    }
  }

  void process() {
    if (exception_) {
      exception_->throw_exception();
    }
    if (set_) {
      exec_.reset();
    }
  }

  template <class T> void setException(T const& t) {
    if (!exception_) {
      exception_ = folly::make_exception_wrapper<T>(t);
    }
    setIfUnset();
  }

  bool isReady() { return set_; }

  void setContinuation(folly::Executor* exec, folly::Func func) {
    exec_ = std::make_pair(exec, std::move(func));
  }

private:
  void set() {
    set_ = true;
    if (exec_) {
      exec_->first->add(std::move(exec_->second));
      exec_.reset();
    }
  }

  std::optional<std::pair<folly::Executor*, folly::Function<void()>>> exec_;
  bool set_ = false;
  std::optional<folly::exception_wrapper> exception_;
};

} // namespace s
