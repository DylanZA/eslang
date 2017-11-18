#pragma once
#include <eslang/Except.h>
#include <folly/Conv.h>
#include <folly/Synchronized.h>
#include <gtest/gtest.h>
#include <sstream>

namespace {
struct ScopeLog {
  std::string const s;
  explicit ScopeLog(std::string s) : s(s) {}

  ~ScopeLog() { LOG(INFO) << "~ScopeLog: " << s; }
};

struct LifetimeChecker {
  struct State {
    int i = 0;
    std::unordered_map<int, std::string> data;
    int add(std::string s) {
      return data.emplace(++i, std::move(s)).first->first;
    }
  };
  folly::Synchronized<State> state;
  void check() {
    std::stringstream ss;
    auto l = state.rlock();
    for (auto const& kv : l->data) {
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
  LifetimeCheck(std::string s) : i(lifetimeChecker.state->add(std::move(s))) {}
  LifetimeCheck(LifetimeCheck const&) = delete;
  LifetimeCheck(LifetimeCheck&&) = delete;
  LifetimeCheck& operator=(LifetimeCheck const&) = delete;
  LifetimeCheck& operator=(LifetimeCheck&&) = delete;
  ~LifetimeCheck() { lifetimeChecker.state->data.erase(i); }
};
}

#define LIFETIMECHECK                                                          \
  LifetimeCheck lc{folly::to<std::string>(__FILE__, ":", __LINE__)};