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
             hyscan-sonar-schema.c
             hyscan-control-common.c
             hyscan-write-control.c
             hyscan-sensor-control.c
             hyscan-generator-control.c
             ${CMAKE_BINARY_DIR}/marshallers/hyscan-marshallers.c)

target_link_libraries (hyscancontrol ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES})
