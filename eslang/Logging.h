#pragma once

#include "ConcatString.h"
#include <boost/log/trivial.hpp>

namespace s {

enum class LL { INFO, WARNING, ERR, V, DEBUG, TRACE, FATAL };
}

#define ESLOG(level, ...)                                                      \
  switch (level) {                                                             \
    \
case::s::LL::INFO : BOOST_LOG_TRIVIAL(info)                                    \
                    << ::s::concatString(__VA_ARGS__);                         \
    break;                                                                     \
    \
case::s::LL::WARNING : BOOST_LOG_TRIVIAL(warning)                              \
                       << ::s::concatString(__VA_ARGS__);                      \
    break;                                                                     \
    \
case::s::LL::V : BOOST_LOG_TRIVIAL(debug)                                      \
                 << ::s::concatString(__VA_ARGS__);                            \
    break;                                                                     \
    \
case::s::LL::DEBUG : BOOST_LOG_TRIVIAL(debug)                                  \
                     << ::s::concatString(__VA_ARGS__);                        \
    break;                                                                     \
    \
case::s::LL::TRACE : BOOST_LOG_TRIVIAL(trace)                                  \
                     << ::s::concatString(__VA_ARGS__);                        \
    break;                                                                     \
    \
case::s::LL::ERR : BOOST_LOG_TRIVIAL(error)                                    \
                   << ::s::concatString(__VA_ARGS__);                          \
    break;                                                                     \
    \
case::s::LL::FATAL : BOOST_LOG_TRIVIAL(info)                                   \
                     << ::s::concatString(__VA_ARGS__);                        \
    std::terminate();                                                          \
    break;                                                                     \
  \
};
