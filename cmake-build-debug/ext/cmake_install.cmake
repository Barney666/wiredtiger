# Install script for directory: /Users/barney/CLionProjects/barney_github/wiredtiger/ext

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/Library/Developer/CommandLineTools/usr/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/ext/compressors/lz4/cmake_install.cmake")
  include("/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/ext/compressors/snappy/cmake_install.cmake")
  include("/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/ext/compressors/zlib/cmake_install.cmake")
  include("/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/ext/compressors/zstd/cmake_install.cmake")
  include("/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/ext/compressors/nop/cmake_install.cmake")
  include("/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/ext/collators/reverse/cmake_install.cmake")
  include("/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/ext/collators/revint/cmake_install.cmake")
  include("/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/ext/encryptors/nop/cmake_install.cmake")
  include("/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/ext/extractors/csv/cmake_install.cmake")
  include("/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/ext/encryptors/rotn/cmake_install.cmake")
  include("/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/ext/encryptors/sodium/cmake_install.cmake")
  include("/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/ext/storage_sources/dir_store/cmake_install.cmake")
  include("/Users/barney/CLionProjects/barney_github/wiredtiger/cmake-build-debug/ext/test/fail_fs/cmake_install.cmake")

endif()

