#pragma once

#include "Except.h"
#include "IWaiting.h"
#include "ProcessTask.h"
#include "Slot.h"

namespace s {
struct WaitingReady : IWaiting {
  bool await_ready() noexcept { return true; }
  void await_resume() {}
};

struct WaitingAlways : IWaiting {
  bool await_ready() noexcept { return false; }
  void await_resume() {}
};

// returns control to context for one cycle
struct WaitingYield : IWaiting {
  bool await_ready() noexcept { return false; }
  void await_resume() {}
  bool isReadyForResume() const { return true; }
};

struct WaitingMaybe : IWaiting {
  bool wait;
  explicit WaitingMaybe(bool wait) : wait(wait) {}
  bool await_ready() noexcept { return !wait; }
  void await_resume() {}
  bool isReadyForResume() const { return true; }
};

struct WaitOnFuture : IWaiting {
  folly::Future<folly::Unit> f;
  explicit WaitOnFuture(folly::Future<folly::Unit> f) : f(std::move(f)) {}

  bool await_ready() noexcept { return f.isReady(); }

  folly::Future<folly::Unit>* wakeOnFuture() override { return &f; }

  void await_resume() {}
};

template <class TUnderlying> struct WithWaitingTimeout : TUnderlying {
  TimePoint t_;

  template <class... Args>
  WithWaitingTimeout(TimePoint t, Args&&... args)
      : TUnderlying(std::forward<Args>(args)...), t_(t) {}

  std::optional<TimePoint> wakeOnTime() const override { return t_; }
};

using WaitingTimeout = WithWaitingTimeout<WaitingAlways>;

template <class TUnderlying> struct WithWaitingFuture : TUnderlying {
  folly::Future<folly::Unit> f;

  template <class... Args>
  WithWaitingFuture(folly::Future<folly::Unit> f, Args&&... args)
      : TUnderlying(std::forward<Args>(args)...), f(std::move(f)) {}

  folly::Future<folly::Unit>* wakeOnFuture() override { return &f; }
};

template <class T>
WithWaitingFuture<T> makeWithWaitingFuture(folly::Future<folly::Unit> f, T t) {
  return WithWaitingFuture<T>(std::move(f), std::move(t));
}

template <class... TTypes> struct WaitingMessages : IWaiting {
  std::tuple<MessageQueue<TTypes>*...> messages;
  std::array<SendAddress, sizeof...(TTypes)> addresses;

  WaitingMessages(TSlotBase<TTypes>&... args);

  template <std::size_t...> bool any_ready(std::index_sequence<>) noexcept {
    // no messages at all
    return false;
  }

  template <std::size_t... I>
  bool any_ready(std::index_sequence<I...>) noexcept {
    auto ret = {(!std::get<I>(messages)->empty())...};
    for (auto a : ret) {
      if (a) {
        return true;
      }
    }
    return false;
  }

  bool await_ready() noexcept {
    return any_ready(std::index_sequence_for<TTypes...>{});
  }

  void
  await_suspend(std::experimental::coroutine_handle<ProcessTask::promise_type>
                    handle) noexcept {
    handle.promise().waiting = this;
  }

  template <size_t I>
  auto get_optional() -> std::optional<
      typename std::tuple_element<I, std::tuple<TTypes...>>::type> {
    using TM = std::tuple_element<I, std::tuple<TTypes...>>::type;
    std::optional<TM> ret;
    if (!std::get<I>(messages)->empty()) {
      ret.emplace(static_cast<TM&&>(std::get<I>(messages)->pop().val()));
    }
    return ret;
  }

  template <std::size_t... I>
  std::tuple<std::optional<TTypes>...> get_vals(std::index_sequence<I...>) {
    return std::tuple<std::optional<TTypes>...>(get_optional<I>()...);
  }

  std::tuple<std::optional<TTypes>...> await_resume() {
    return get_vals(std::index_sequence_for<TTypes...>{});
  }

  bool isWaiting(SlotId s) const override {
    return std::any_of(addresses.begin(), addresses.end(),
                       [s](auto const& add) { return add.slot() == s; });
  }
};

template <class T> struct WaitingMessage {
  WaitingMessages<T> underlying;

  WaitingMessage(WaitingMessages<T> underlying)
      : underlying(std::move(underlying)) {}

  bool await_ready() noexcept { return underlying.await_ready(); }

  void
  await_suspend(std::experimental::coroutine_handle<ProcessTask::promise_type>
                    handle) noexcept {
    return underlying.await_suspend(std::move(handle));
  }

  T await_resume() {
    auto ret = underlying.await_resume();
    if (!std::get<0>(ret)) {
      ESLANGEXCEPT("Message was not ready");
    }
    return std::move(*std::get<0>(ret));
  }
};

template <class... TTypes>
WaitingMessages<TTypes...>::WaitingMessages(TSlotBase<TTypes>&... args)
    : messages(args.queue()...), addresses({args.address()...}) {}
}
