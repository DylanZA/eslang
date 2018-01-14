#pragma once
#include <sstream>
#include <string>
namespace s {

namespace {

void doAppendStrings(std::ostream& ss) {
}

template<class T>
void doAppendStrings(std::ostream& ss, T const& t) {
  ss << t;
}

template<class T, class... Args>
void doAppendStrings(std::ostream& ss, T const& t, Args... args) {
  doAppendStrings(ss, t);
  doAppendStrings(ss, std::forward<Args>(args)...);
}

// concatenate the args as strings
// could be done with something like folly::to or absl::StrCat, but that would be an additional dependency
template<class... Args> 
std::string concatString(Args... args) {
  std::stringstream ss;
  doAppendStrings(ss, std::forward<Args>(args)...);
  return ss.str();
}

}

}
