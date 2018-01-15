#include <gtest/gtest.h>

#include "TestCommon.h"
#include <eslang/Context.h>
#include <eslang/Logging.h>

namespace s {

class MaxInFlight : public Process {
public:
  using Process::Process;
  LIFETIMECHECK;
  struct Item {
    std::shared_ptr<uint64_t> val;
    uint64_t get() { return *val; }
    Item(std::shared_ptr<uint64_t> v) : val(v) { ++*val; }
    ~Item() { --*val; }
    Item(Item const& item) : val(item.val) { ++*val; }
    Item& operator=(Item const& item) {
      val = item.val;
      ++*val;
      return *this;
    }
  };

  struct Receiver : Process {
    Slot<Item> rec{this};
    using Process::Process;

    ProcessTask run() {
      for (size_t count = 0; count < 10000; ++count) {
        auto i(co_await recv(rec));
        for (size_t i = 0; i < 10; ++i) {
          co_await WaitingYield{};
        }
        ESLOG(LL::TRACE, "i is ", i.get());
        ASSERT_LT(i.get(), 1000) << "i got too big";
      }
    }
  };

  ProcessTask run() {
    // spawn a submethod, and make sure that if we kill it it cleans up the
    // coroutine
    auto val = std::make_shared<uint64_t>(1);
    auto sub = spawnLink<Receiver>();
    while (true) {
      co_await sendThrottled(c_->makeSendAddress(sub, &Receiver::rec), val);
    }
  }
};
}

TEST(BackPressure, MaxInFlight) {
  s::Context c;
  c.spawn<s::MaxInFlight>();
  c.run();
  lifetimeChecker.check();
}