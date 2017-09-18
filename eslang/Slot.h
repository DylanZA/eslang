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
template <class T> using MessageQueue = std::deque<Message<T>>;

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
    messages_.push_back(std::move(*m));
  }

private:
  MessageQueue<T> messages_;
};

} // namespace s
