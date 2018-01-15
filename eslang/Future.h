#pragma once
#include <memory>

#include <boost/asio/io_service.hpp>

namespace s {

class ExceptionWrapper {
public:
  static ExceptionWrapper makeFromCurrentException() {
    return ExceptionWrapper(
        std::make_unique<PtrImpl>(std::current_exception()));
  }

  template <class T> static ExceptionWrapper make(T ex) {
    struct I : Impl {
      T e_;
      explicit I(T e) : e_(std::move(e)) {}
      void maybeThrow() override { throw e_; }
    };
    return ExceptionWrapper(std::make_unique<I>(std::move(ex)));
  }

  void maybeThrowException() { impl_->maybeThrow(); }

private:
  struct Impl {
    virtual ~Impl() = default;
    virtual void maybeThrow() = 0;
  };
  struct PtrImpl : Impl {
    std::exception_ptr ptr;

    PtrImpl(std::exception_ptr ptr) : ptr(std::move(ptr)) {}

    void maybeThrow() override {
      if (ptr) {
        std::rethrow_exception(ptr);
      }
    }
  };

  ExceptionWrapper(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

  std::unique_ptr<Impl> impl_;
};

class EslangPromise {
public:
  EslangPromise() = default;
  EslangPromise(EslangPromise const&) = delete;
  EslangPromise& operator=(EslangPromise const&) = delete;

  EslangPromise(EslangPromise&& rhs)
      : exec_(std::move(rhs.exec_)), set_(rhs.set_),
        exception_(std::move(rhs.exception_)) {}

  EslangPromise& operator=(EslangPromise&& rhs) {
    using std::swap;
    exec_ = std::move(rhs.exec_);
    set_ = std::move(rhs.set_);
    exception_ = std::move(rhs.exception_);
    return *this;
  }

  using Function = std::function<void()>;
  void setIfUnset() {
    if (!set_) {
      set();
    }
  }

  void process() {
    if (exception_) {
      exception_->maybeThrowException();
    }
    if (set_) {
      exec_.reset();
    }
  }

  template <class T> void setException(T const& t) {
    if (!exception_) {
      exception_ = ExceptionWrapper::make(t);
    }
    setIfUnset();
  }

  bool isReady() { return set_; }

  void setContinuation(boost::asio::io_service& io_service, Function func) {
    exec_ = std::make_pair(&io_service, std::move(func));
  }

private:
  void set() {
    set_ = true;
    if (exec_) {
      // exec_->first.post(std::move(exec_->second));
      // the worst part of asio is handlers must be copyable
      exec_->first->post(std::move(exec_->second));
      exec_.reset();
    }
  }

  std::optional<std::pair<boost::asio::io_service*, Function>> exec_;
  bool set_ = false;
  std::optional<ExceptionWrapper> exception_;
};

} // namespace s
