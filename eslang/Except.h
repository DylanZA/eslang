#pragma once
#include "ConcatString.h"
#include <stdexcept>
namespace s {

class EslangException : public std::runtime_error {
public:
  EslangException(std::string message = {})
      : std::runtime_error(std::move(message)) {}
};
}

// todo: add line numbers etc...
#define ESLANGEXCEPT(...) throw ::s::EslangException(concatString(__VA_ARGS__));

#define ESLANGREQUIRE(b, ...)                                                  \
  do {                                                                         \
    if (!b)                                                                    \
      throw ::s::EslangException(concatString(__VA_ARGS__));                   \
  } while (0);
