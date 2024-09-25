# CMake generated Testfile for 
# Source directory: /Users/barney/CLionProjects/barney_github/wiredtiger/bench/wtperf
# Build directory: /Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/bench/wtperf
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_wtperf_small_lsm "/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/bench/wtperf/wtperf" "-O" "/Users/barney/CLionProjects/barney_github/wiredtiger/bench/wtperf/runners/small-lsm.wtperf" "-o" "run_time=20")
set_tests_properties(test_wtperf_small_lsm PROPERTIES  LABELS "check;wtperf" WORKING_DIRECTORY "/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/bench/wtperf/test_wtperf_small_lsm_test_dir" _BACKTRACE_TRIPLES "/Users/barney/CLionProjects/barney_github/wiredtiger/test/ctest_helpers.cmake;191;add_test;/Users/barney/CLionProjects/barney_github/wiredtiger/bench/wtperf/CMakeLists.txt;38;define_test_variants;/Users/barney/CLionProjects/barney_github/wiredtiger/bench/wtperf/CMakeLists.txt;0;")
