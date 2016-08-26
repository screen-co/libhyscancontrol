#
# Marshaller generation utility.
find_program (MARSHAL_COMPILER glib-genmarshal)
if (${MARSHAL_COMPILER} STREQUAL MARSHAL_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib marshallers compiler not found.")
endif ()

#
# Marshaller files.
file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/marshallers)
include_directories (${CMAKE_BINARY_DIR}/marshallers)

execute_process (COMMAND ${MARSHAL_COMPILER}
                         --header
                         hyscan-marshallers.list
                         OUTPUT_FILE ${CMAKE_BINARY_DIR}/marshallers/hyscan-marshallers.h
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

execute_process (COMMAND ${MARSHAL_COMPILER}
                         --body
                         hyscan-marshallers.list
                         OUTPUT_FILE ${CMAKE_BINARY_DIR}/marshallers/hyscan-marshallers.c
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_library (hyscancontrol SHARED
             hyscan-control.c
             hyscan-control-common.c
             hyscan-sensor-control.c
             hyscan-generator-control.c
             hyscan-tvg-control.c
             hyscan-sonar-control.c
             hyscan-ssse-control.c
             hyscan-sonar-box.c
             hyscan-sensor-control-server.c
             hyscan-generator-control-server.c
             hyscan-tvg-control-server.c
             hyscan-sonar-control-server.c
             hyscan-ssse-control-server.c
             hyscan-sonar-schema.c
             ${CMAKE_BINARY_DIR}/marshallers/hyscan-marshallers.c)

target_link_libraries (hyscancontrol ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES})
