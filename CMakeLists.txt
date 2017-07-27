
cmake_minimum_required (VERSION 2.6)

project (libhyscangui)

include (FindPkgConfig)

if (${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
  set (C_FLAGS "-Wall -Wextra -Wno-unused-parameter $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 ${C_FLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O1 ${C_FLAGS}")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL "MSVC")
  set (C_FLAGS "/wd4244 $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${C_FLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${C_FLAGS}")
else ()
  message (FATAL_ERROR "Unsupported compiler ${CMAKE_C_COMPILER_ID}")
endif ()

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

pkg_check_modules (GTK3 REQUIRED "gtk+-3.0")
pkg_check_modules (LIBXML2 REQUIRED "libxml-2.0")
add_definitions (${GTK3_CFLAGS} ${LIBXML2_CFLAGS})
link_directories (${GTK3_LIBRARY_DIRS} ${LIBXML2_LIBRARY_DIRS})

include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/hyscangui")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes/hyscantypes")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandb/hyscandb")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancache/hyscancache")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancore/hyscancore")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancore/hyscancontrol")

link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes/bin")
link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandb/bin")
link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancache/bin")
link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancore/bin")
link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancontrol/bin")
set (HYSCAN_LIBRARIES hyscantypes hyscandb hyscancache hyscancore hyscancontrol)

add_definitions (-DG_LOG_DOMAIN="HyScanGUI")
add_subdirectory ("hyscangui")
add_subdirectory ("tests")
