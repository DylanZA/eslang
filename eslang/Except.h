#pragma once
#include <folly/Conv.h>
#include <stdexcept>
namespace s {

class EslangException : public std::runtime_error {
public:
  EslangException(std::string message = {})
      : std::runtime_error(std::move(message)) {}
};

// todo: add line numbers etc...
#define ESLANGEXCEPT(...)                                                      \
  throw EslangException(folly::to<std::string>(__VA_ARGS__));
}