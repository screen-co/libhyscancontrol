
cmake_minimum_required (VERSION 2.6)

project (libhyscancontrol)

include (FindPkgConfig)

#
# Default build type.
if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE "Release")
endif ()

if (${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -Wall $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -Wall $ENV{CFLAGS}")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL "MSVC")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /wd4244 /wd4305 $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /wd4244 /wd4305 $ENV{CFLAGS}")
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

pkg_check_modules (GLIB2 REQUIRED "glib-2.0 gobject-2.0 gthread-2.0 gio-2.0")
pkg_check_modules (GMODULE2 REQUIRED "gmodule-2.0")
pkg_check_modules (LIBXML2 REQUIRED "libxml-2.0")
add_definitions (${GLIB2_CFLAGS} ${GMODULE2_CFLAGS} ${LIBXML2_CFLAGS})
link_directories (${GLIB2_LIBRARY_DIRS} ${GMODULE2_LIBRARY_DIRS} ${LIBXML2_LIBRARY_DIRS})

include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/hyscancontrol")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes/hyscantypes")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandb/hyscandb")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancache/hyscancache")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscanmath/hyscanmath")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancore/hyscancore")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscansonar/hyscansonar")

link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes/bin")
link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandb/bin")
link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancache/bin")
link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscanmath/bin")
link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancore/bin")
link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscansonar/bin")
set (ADDON_LIBRARIES hyscantypes hyscandb hyscancache hyscanmath hyscancore hyscansonar)

add_definitions (-DG_LOG_DOMAIN="HyScanControl")
add_subdirectory ("hyscancontrol")
add_subdirectory ("tests")
