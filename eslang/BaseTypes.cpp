#include "BaseTypes.h"
#include <folly/Conv.h>

namespace s {

std::string Pid::toString() const {
  return folly::to<std::string>(idx_, version_);
}

std::ostream& operator<<(std::ostream& s, Pid p) {
  s << p.toString();
  return s;
}
}