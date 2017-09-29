#pragma once
#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <tuple>

#include "BaseTypes.h"
#include "Except.h"
#include "IWaiting.h"

namespace s {

class Process;

// todo: use a small_vector or something
template <class T> class MessageQueue {
public:
  MessageQueue() = default;
  MessageQueue(MessageQueue const&) = delete;
  MessageQueue(MessageQueue&&) = delete;
  MessageQueue& operator=(MessageQueue const&) = delete;
  MessageQueue& operator=(MessageQueue&&) = delete;
  size_t size() const {
    if (stackStorage) {
      return 1 + others.size();
    }
    return 0;
  }
  bool empty() const { return size() == 0; }
  void push(Message<T> t) {
    if (stackStorage) {
      others.push_back(std::move(t));
    } else {
      stackStorage = std::move(t);
    }
  }

  Message<T> pop() {
    Message<T> ret = std::move(*stackStorage);
    if (others.empty()) {
      stackStorage.reset();
    } else {
      stackStorage = std::move(others.front());
      others.pop_front();
    }
    return ret;
  }

  // a guess that optimising for single queue length is a good idea.
  // maybe terrible in practive. who can say!
  std::optional<Message<T>> stackStorage;
  std::deque<Message<T>> others;
};

class SlotBase {
public:
  SlotBase(Process* parent);
  SlotBase(SlotBase const&) = delete;
  SlotBase(SlotBase&&) = delete;
  SlotBase& operator=(SlotBase const&) = delete;
  SlotBase& operator=(SlotBase&&) = delete;
  SlotId id() const { return id_; }
  virtual ~SlotBase();
  virtual void push(MessageBase message) = 0;

protected:
  Process* const parent_;
  SlotId const id_;
};

template <class T> class TSlotBase : public SlotBase {
public:
  using SlotBase::SlotBase;
  virtual ~TSlotBase() = default;
  virtual MessageQueue<T>* queue() = 0;
  virtual TSendAddress<T> address() const = 0;
};

template <class T> class Slot : public TSlotBase<T> {
public:
  Slot(Process* p) : TSlotBase(p) {}

  MessageQueue<T>* queue() override { return &messages_; }

  TSendAddress<T> address() const override {
    return TSendAddress<T>(parent_->pid(), id_);
  }

  SlotId id() const { return id_; }

  void push(MessageBase message) override {
    Message<T>* m = static_cast<Message<T>*>(&message);
    messages_.push(std::move(*m));
  }

private:
  MessageQueue<T> messages_;
};

} // namespace s
