#pragma once
#include <boost/asio/io_service.hpp>
#include <folly/ExceptionWrapper.h>
#include <folly/Function.h>

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

  void setContinuation(boost::asio::io_service& io_service,
                       folly::Function<void()> func) {
    exec_ = std::make_pair(&io_service, std::move(func));
  }

private:
  void set() {
    set_ = true;
    if (exec_) {
      // exec_->first.post(std::move(exec_->second));
      // the worst part of asio is handlers must be copyable
      exec_->first->post(std::move(exec_->second).asStdFunction());
      exec_.reset();
    }
  }

  std::optional<std::pair<boost::asio::io_service*, folly::Function<void()>>>
      exec_;
  bool set_ = false;
  std::optional<folly::exception_wrapper> exception_;
};

} // namespace s
