# CMake generated Testfile for 
# Source directory: /Users/barney/CLionProjects/barney_github/wiredtiger/lang/python
# Build directory: /Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/lang/python
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_ex_access "/Users/barney/Applications/CLion.app/Contents/bin/cmake/mac/bin/cmake" "-E" "env" "PYTHONPATH=/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/lang/python" "/opt/homebrew/Frameworks/Python.framework/Versions/3.12/bin/python3.12" "-S" "/Users/barney/CLionProjects/barney_github/wiredtiger/examples/python/ex_access.py")
set_tests_properties(test_ex_access PROPERTIES  LABELS "check" _BACKTRACE_TRIPLES "/Users/barney/CLionProjects/barney_github/wiredtiger/lang/python/CMakeLists.txt;179;add_test;/Users/barney/CLionProjects/barney_github/wiredtiger/lang/python/CMakeLists.txt;0;")
