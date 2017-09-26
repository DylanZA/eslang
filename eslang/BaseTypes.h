#pragma once
#include <chrono>
#include <cstdint>
#include <experimental/coroutine>
#include <optional>
#include <ostream>

#include "Except.h"

namespace s {

class NonMovable {
public:
  NonMovable() = default;
  NonMovable(NonMovable const&) = delete;
  NonMovable(NonMovable&&) = delete;
  NonMovable& operator=(NonMovable&&) = delete;
  NonMovable& operator=(NonMovable const&) = delete;
};
class MessageBase {
public:
  class StorageBase {
  public:
    virtual ~StorageBase() = default;
  };

  template <class T> class Storage : public StorageBase {
  public:
    template <class... Args>
    Storage(Args&&... args) : val(std::forward<Args>(args)...) {}
    T val;
  };

  // allow movable, not copyable
  // so one day we can optimize moving of big messages
  MessageBase(MessageBase&&) = default;
  MessageBase& operator=(MessageBase&&) = default;
  MessageBase(MessageBase const&) = delete;
  MessageBase& operator=(MessageBase const&) = delete;

  MessageBase(std::unique_ptr<StorageBase> data) : storage_(std::move(data)) {}

protected:
  std::unique_ptr<StorageBase> storage_;
};

template <class T> class Message : public MessageBase {
public:
  template <class... Args>
  Message(Args&&... args)
      : MessageBase(std::make_unique<Storage<T>>(std::forward<Args>(args)...)) {
  }
  T const& val() const { return static_cast<Storage<T>*>(storage_.get())->val; }
  T& val() { return static_cast<Storage<T>*>(storage_.get())->val; }
};

class Pid {
public:
  Pid(uint64_t idx, uint64_t version) : idx_(idx), version_(version) {}
  bool operator==(Pid rhs) const {
    return idx_ == rhs.idx_ && version_ == rhs.version_;
  }
  uint64_t idx() const { return idx_; }
  uint64_t version() const { return version_; }
  std::string toString() const;

private:
  friend std::ostream& operator<<(std::ostream& s, Pid p);
  uint64_t idx_;
  uint64_t version_;
};

using TimePoint = std::chrono::steady_clock::time_point;

using SlotId = void*;

class SendAddress {
public:
  Pid pid() const { return pid_; }
  SlotId slot() const { return slot_; }

protected:
  SendAddress(Pid pid, SlotId slot) : pid_(pid), slot_(slot) {}

private:
  Pid pid_;
  SlotId slot_;
};

class Context;

template <class T> class Slot;

template <class TType> class TSendAddress : public SendAddress {
private:
  friend class Context;

  template <class T> friend class Slot;

  using SendAddress::SendAddress;
};
}

namespace std {
template <> struct hash<s::Pid> {
  std::hash<uint64_t> h;
  size_t operator()(s::Pid p) const { return h(p.idx()); }
};
}
