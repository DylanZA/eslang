cmake_minimum_required (VERSION 3.8)
project (Eslang)
SET(CMAKE_EXPORT_COMPILE_COMMANDS 1)

add_definitions(/await)
add_definitions(/std:c++latest)

message(STATUS CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS CMAKE_CURRENT_LIST_DIR ${CMAKE_CURRENT_LIST_DIR})
message(STATUS CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
message(STATUS CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})

FIND_PATH(LIBGFLAGS_INCLUDE_DIR gflags/gflags.h)
FIND_LIBRARY(LIBGFLAGS_LIBRARY NAMES gflags gflags_static)
include_directories(${LIBGFLAGS_INCLUDE_DIR})

FIND_PATH(LIBGLOG_INCLUDE_DIR glog/logging.h)
FIND_LIBRARY(LIBGLOG_LIBRARY glog)
include_directories(${LIBGLOG_INCLUDE_DIR})

FIND_PATH(FOLLY_INCLUDE_DIR folly/Function.h)
FIND_LIBRARY(LIBEVENT_LIBRARY event)
FIND_LIBRARY(DOUBLE_CONVERT_LIBRARY double-conversion)

FIND_PATH(FOLLY_INCLUDE_DIR folly/Function.h)
FIND_LIBRARY(FOLLY_LIBRARY folly)

FIND_PATH(LIBGLOG_INCLUDE_DIR glog/logging.h)
FIND_LIBRARY(LIBGLOG_LIBRARY glog)

find_package(OpenSSL MODULE REQUIRED)

find_package(Boost COMPONENTS context system filesystem chrono thread iostreams date_time REQUIRED)


set(ESLANG_BASE_LIBS
	${LIBGLOG_LIBRARY}
	${LIBGFLAGS_LIBRARY}
	${Boost_LIBRARIES}
	${LIBEVENT_LIBRARY}
	${DOUBLE_CONVERT_LIBRARY}
	${FOLLY_LIBRARY}
	${OPENSSL_LIBRARIES}
	shlwapi.lib
	Iphlpapi.lib
	Ws2_32.lib
    )

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${FOLLY_INCLUDE_DIR})
include_directories(${LIBGLOG_INCLUDE_DIR})

add_subdirectory(eslang)
add_subdirectory(eslang_io)
set(ESLANG_LIBS eslang eslang_io)
set (EXAMPLES basic tcp count send_back_pressure)
foreach(EXAMPLE ${EXAMPLES})
  add_executable (example_${EXAMPLE} examples/${EXAMPLE}.cpp)
  target_link_libraries(example_${EXAMPLE} ${ESLANG_LIBS})
endforeach(EXAMPLE)

find_package(gtest REQUIRED)
include_directories(${GTEST_INCLUDE_DIR})
file(GLOB test_files "tests/*.cpp"  "tests/*.h")
add_executable(eslang_test ${test_files})
target_link_libraries(eslang_test ${GTEST_LIBRARY} ${ESLANG_LIBS})
