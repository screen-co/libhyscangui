
cmake_minimum_required (VERSION 2.8)

project (libhyscangui)
enable_testing()

set (HYSCAN_MAJOR_VERSION 5)
set (HYSCAN_GUI_VERSION 0)
set (HYSCAN_GUI_LIBRARY "hyscangui-${HYSCAN_MAJOR_VERSION}")

include (FindPkgConfig)

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release)
endif ()

if (${CMAKE_C_COMPILER_ID} STREQUAL GNU OR ${CMAKE_C_COMPILER_ID} STREQUAL Clang)
  set (C_FLAGS "-Wall -Wextra -Wno-unused-parameter $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 ${C_FLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g ${C_FLAGS}")
  set (MATH_LIBRARIES m)
elseif (${CMAKE_C_COMPILER_ID} STREQUAL MSVC)
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

pkg_check_modules (GTK3 REQUIRED gtk+-3.0)
pkg_check_modules (LIBXML2 REQUIRED libxml-2.0)
pkg_check_modules (LIBSOUP REQUIRED libsoup-2.4)
pkg_check_modules (GDK_PIXBUF REQUIRED gdk-pixbuf-2.0)
pkg_check_modules (EPOXY REQUIRED epoxy)

add_definitions (${GTK3_CFLAGS} ${LIBXML2_CFLAGS} ${LIBSOUP_CFLAGS} ${GDK_PIXBUF_CFLAGS})
include_directories (${EPOXY_INCLUDE_DIRS})
link_directories (${GTK3_LIBRARY_DIRS} ${LIBXML2_LIBRARY_DIRS} ${LIBSOUP_LIBRARY_DIRS} ${GDK_PIXBUF_LIBRARY_DIRS} ${EPOXY_LIBRARY_DIRS})

if (HYSCAN_USE_SYS_LIBS)
  pkg_check_modules (HYSCAN REQUIRED "hyscantypes-${HYSCAN_MAJOR_VERSION}")

  link_directories (${HYSCAN_LIBRARY_DIRS})
  add_definitions (${HYSCAN_CFLAGS})
else ()
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes/hyscantypes")
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancore/hyscancore")
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscanmodel/hyscanmodel")
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancache/hyscancache")
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandriver/hyscandriver")
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandb/hyscandb")
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscangtk/hyscangtk")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes/bin")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancore/bin")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscanmodel/bin")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancache/bin")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandriver/bin")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandb/bin")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscangtk/bin")
  set (HYSCAN_LIBRARIES "hyscantypes-${HYSCAN_MAJOR_VERSION}"
                        "hyscandriver-${HYSCAN_MAJOR_VERSION}"
                        "hyscancore-${HYSCAN_MAJOR_VERSION}"
                        "hyscandb-${HYSCAN_MAJOR_VERSION}"
                        "hyscanmodel-${HYSCAN_MAJOR_VERSION}"
                        "hyscancache-${HYSCAN_MAJOR_VERSION}"
                        "hyscangtk-${HYSCAN_MAJOR_VERSION}")
endif ()

include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../gtkcifroarea/gtkcifroarea")
link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../gtkcifroarea/bin")
set (GTKCIFROAREA_LIBRARIES gtkcifroarea-3.1)

include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/hyscangui")

add_definitions (-DG_LOG_DOMAIN="HyScanGUI")
add_definitions (-DGETTEXT_PACKAGE="hyscangui")
add_subdirectory (hyscangui)
add_subdirectory (tests)
