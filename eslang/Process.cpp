#include "Process.h"
#include "Context.h"

namespace s {

TimePoint Process::now() const { return c_->now(); }

void Process::link(Pid p) { c_->link(this, p); }

}
