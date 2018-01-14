#include "BaseTypes.h"

namespace s {

std::string Pid::toString() const { return concatString(idx_, version_); }

std::ostream& operator<<(std::ostream& s, Pid p) {
  s << p.toString();
  return s;
}
}