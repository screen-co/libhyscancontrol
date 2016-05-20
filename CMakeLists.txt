cmake_minimum_required (VERSION 2.6)

project (libhyscancontrol)

include (FindPkgConfig)

#
# Default build type.
if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE "Release")
endif ()

#
# Compilers options.
if (${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -Wall $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -Wall $ENV{CFLAGS}")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL "MSVC")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /wd4244 /wd4305 $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /wd4244 /wd4305 $ENV{CFLAGS}")
else ()
  message (FATAL_ERROR "Unsupported compiler ${CMAKE_C_COMPILER_ID}")
endif ()

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
pkg_check_modules (GLIB2 REQUIRED "glib-2.0 gobject-2.0 gthread-2.0 gio-2.0")
add_definitions (${GLIB2_CFLAGS})
link_directories (${GLIB2_LIBRARY_DIRS})

#
# HyScan libraries default paths.
if (NOT HYSCAN_CORE_SOURCE_DIR)
  set (HYSCAN_CORE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancore)
endif ()
if (NOT HYSCAN_DB_SOURCE_DIR)
  set (HYSCAN_DB_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandb)
endif ()
if (NOT HYSCAN_CACHE_SOURCE_DIR)
  set (HYSCAN_CACHE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancache)
endif ()
if (NOT HYSCAN_SONAR_SOURCE_DIR)
  set (HYSCAN_SONAR_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libhyscansonar)
endif ()
if (NOT HYSCAN_TYPES_SOURCE_DIR)
  set (HYSCAN_TYPES_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes)
endif ()

#
# HyScan dependencies.
pkg_check_modules (HYSCAN QUIET hyscancore hyscandb hyscancache hyscansonar hyscantypes)
if (${HYSCAN_FOUND})
  add_definitions (${HYSCAN_CFLAGS})
  link_directories (${HYSCAN_LIBRARY_DIRS})
else ()
  include_directories ("${HYSCAN_CORE_SOURCE_DIR}/hyscancore")
  include_directories ("${HYSCAN_DB_SOURCE_DIR}/hyscandb")
  include_directories ("${HYSCAN_CACHE_SOURCE_DIR}/hyscancache")
  include_directories ("${HYSCAN_SONAR_SOURCE_DIR}/hyscansonar")
  include_directories ("${HYSCAN_TYPES_SOURCE_DIR}/hyscantypes")
  link_directories ("${HYSCAN_CORE_SOURCE_DIR}/bin")
  link_directories ("${HYSCAN_DB_SOURCE_DIR}/bin")
  link_directories ("${HYSCAN_CACHE_SOURCE_DIR}/bin")
  link_directories ("${HYSCAN_SONAR_SOURCE_DIR}/bin")
  link_directories ("${HYSCAN_TYPES_SOURCE_DIR}/bin")
  set (HYSCAN_LIBRARIES hyscancore hyscandb hyscancache hyscansonar hyscantypes)
endif ()

#
# Include directories.
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/hyscancontrol")

#
# Target.
add_definitions (-DG_LOG_DOMAIN="HyScanControl")
add_subdirectory ("hyscancontrol")
add_subdirectory ("tests")
