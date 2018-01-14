cmake_minimum_required (VERSION 3.8)
project (Eslang)
SET(CMAKE_EXPORT_COMPILE_COMMANDS 1)

add_definitions(/await)
add_definitions(/std:c++latest)
add_definitions(/D_WIN32_WINNT=0x0601)

message(STATUS CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS CMAKE_CURRENT_LIST_DIR ${CMAKE_CURRENT_LIST_DIR})
message(STATUS CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
message(STATUS CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})

find_package(OpenSSL MODULE REQUIRED)

find_package(Boost COMPONENTS context system filesystem chrono thread iostreams program_options log log_setup date_time REQUIRED)

set(ESLANG_BASE_LIBS
	${Boost_LIBRARIES}
	${OPENSSL_LIBRARIES}
	shlwapi.lib
	Iphlpapi.lib
	Ws2_32.lib
    )

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIRS})

add_subdirectory(eslang)
add_subdirectory(eslang_io)
add_subdirectory(eslang_www)
set(ESLANG_LIBS eslang eslang_io eslang_www)
set (EXAMPLES tcp count send_back_pressure www)
foreach(EXAMPLE ${EXAMPLES})
  add_executable (example_${EXAMPLE} examples/${EXAMPLE}.cpp)
  target_link_libraries(example_${EXAMPLE} ${ESLANG_LIBS})
endforeach(EXAMPLE)

find_package(gtest REQUIRED)
include_directories(${GTEST_INCLUDE_DIR})

# test is only for the core libs (for now)
file(GLOB test_files "tests/*.cpp"  "tests/*.h")
add_executable(eslang_test ${test_files})
target_link_libraries(eslang_test ${GTEST_LIBRARY} eslang)
