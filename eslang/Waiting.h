#pragma once

#include <array>

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
  EslangPromise* f;
  explicit WaitOnFuture(EslangPromise* f) : f(f) {}

  bool await_ready() noexcept { return f->isReady(); }

  EslangPromise* wakeOnFuture() override { return f; }

  void await_resume() {}
};

template <class TUnderlying> struct WithWaitingTimeout : TUnderlying {
  std::chrono::milliseconds t_;

  template <class... Args>
  WithWaitingTimeout(std::chrono::milliseconds t, Args&&... args)
      : TUnderlying(std::forward<Args>(args)...), t_(t) {}

  std::optional<std::chrono::milliseconds> sleepFor() const override {
    return t_;
  }
};

using WaitingTimeout = WithWaitingTimeout<WaitingAlways>;

template <class TUnderlying> struct WithWaitingFuture : TUnderlying {
  EslangPromise* p;

  template <class... Args>
  WithWaitingFuture(EslangPromise* p, Args&&... args)
      : TUnderlying(std::forward<Args>(args)...), p(p) {}

  EslangPromise* wakeOnFuture() override { return p; }
};

template <class T>
WithWaitingFuture<T> makeWithWaitingFuture(EslangPromise* p, T t) {
  return WithWaitingFuture<T>(p, std::move(t));
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

  template <size_t I>
  auto get_optional() -> std::optional<
      typename std::tuple_element<I, std::tuple<TTypes...>>::type> {
    using TM = typename std::tuple_element<I, std::tuple<TTypes...>>::type;
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

  template <class U> void await_suspend(U handle) noexcept {
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
