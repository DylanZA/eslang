#include "Slot.h"
#include "Process.h"

namespace s {

SlotBase::SlotBase(Process* parent)
    : parent_(parent), id_(parent->getSlotId(this)) {}

SlotBase::~SlotBase() { parent_->slotDestroyed(this); }
}
