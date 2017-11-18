#include <glog/logging.h>
#include <gtest/gtest.h>

#include <folly/init/Init.h>

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_stderrthreshold = 0;
  FLAGS_v = 2;
  folly::init(&argc, &argv);

  return RUN_ALL_TESTS();
}
