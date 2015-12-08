cmake_minimum_required (VERSION 2.6)

project (libhyscangui)

include (FindPkgConfig)

#
# Default build type.
if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE "Release")
endif ()

#
# Default CPU architecture.
if (NOT CPU_ARCH)
  set (CPU_ARCH "x86")
endif ()

#
# CPU architecture.
if (CPU_ARCH STREQUAL "x86")
  add_definitions (-DCPU_ARCH_X86)
else ()
  message (FATAL_ERROR "${CPU_ARCH} is unsupported CPU architecture")
endif ()

#
# CPU bitness.
if (CMAKE_SIZEOF_VOID_P EQUAL 4)
  set (CPU32 1)
  add_definitions (-DCPU32)
elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set (CPU64 1)
  add_definitions (-DCPU64)
else ()
  MATH (EXPR CPU_BITNESS 8*${CMAKE_SIZEOF_VOID_P})
  message (FATAL_ERROR "${CPU_BITNESS}bit is unsupported CPU bitness")
endif ()

#
# Compilers options.
if (${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
  if (CPU_ARCH STREQUAL "x86" AND CPU32)
    set (COMPILER_FLAGS "-march=pentium4 -mtune=generic -mfpmath=sse -Wall")
  else ()
    set (COMPILER_FLAGS "-Wall")
  endif ()
  set (COMPILER_FLAGS_RELEASE "${COMPILER_FLAGS} -O2")
  set (COMPILER_FLAGS_DEBUG "${COMPILER_FLAGS} -Og")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL "MSVC")
  set (COMPILER_FLAGS_RELEASE "")
  set (COMPILER_FLAGS_DEBUG "")
else ()
  message (FATAL_ERROR "Unsupported compiler ${CMAKE_C_COMPILER_ID}")
endif ()

set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${COMPILER_FLAGS_RELEASE}")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${COMPILER_FLAGS_DEBUG}")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${COMPILER_FLAGS_RELEASE}")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${COMPILER_FLAGS_DEBUG}")

#
# Output directories.
if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

if (NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

if (NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

#
# System dependencies.
pkg_check_modules (GTK3 REQUIRED "gtk+-3.0")
add_definitions (${GTK3_CFLAGS})
link_directories (${GTK3_LIBRARY_DIRS})

#
# Include directories.
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/hyscangui")

#
# Target.
add_definitions (-DG_LOG_DOMAIN="HyScanGUI")
add_subdirectory ("hyscangui")
add_subdirectory ("tests")
