#pragma once
#include <eslang/Except.h>
#include <eslang/Logging.h>
#include <gtest/gtest.h>
#include <mutex>
#include <sstream>

namespace {

struct ScopeLog {
  std::string const s;
  explicit ScopeLog(std::string s) : s(s) {}

  ~ScopeLog() { ESLOG(s::LL::INFO, "~ScopeLog: ", s); }
};

struct ScopeRun {
  std::function<void()> fn;
  explicit ScopeRun(std::function<void()> fn) : fn(fn) {}

  ~ScopeRun() { fn(); }
};

struct LifetimeChecker {
  struct State {
    int i = 0;
    std::unordered_map<int, std::string> data;
    int add(std::string s) {
      return data.emplace(++i, std::move(s)).first->first;
    }
  };
  State state;
  std::mutex mutex;
  auto lock() { return std::unique_lock<std::mutex>(mutex); }
  void check() {
    std::stringstream ss;
    auto l = lock();
    for (auto const& kv : state.data) {
      ss << kv.second << ",";
    }
    auto bad = ss.str();
    ASSERT_TRUE(bad.empty()) << ss.str();
  }
  ~LifetimeChecker() { check(); }
};

LifetimeChecker lifetimeChecker;

struct LifetimeCheck {
  int i;
  LifetimeCheck(std::string s) {
    auto l = lifetimeChecker.lock();
    i = lifetimeChecker.state.add(std::move(s));
  }
  LifetimeCheck(LifetimeCheck const&) = delete;
  LifetimeCheck(LifetimeCheck&&) = delete;
  LifetimeCheck& operator=(LifetimeCheck const&) = delete;
  LifetimeCheck& operator=(LifetimeCheck&&) = delete;
  ~LifetimeCheck() {
    auto l = lifetimeChecker.lock();
    lifetimeChecker.state.data.erase(i);
  }
};
}

#define LIFETIMECHECK LifetimeCheck lc{concatString(__FILE__, ":", __LINE__)};
