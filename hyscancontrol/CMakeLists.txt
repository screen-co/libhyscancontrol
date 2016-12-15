
find_program (MARSHAL_COMPILER glib-genmarshal)
if (${MARSHAL_COMPILER} STREQUAL MARSHAL_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib marshallers compiler not found.")
endif ()

file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/marshallers)
include_directories (${CMAKE_BINARY_DIR}/marshallers)

add_custom_command (OUTPUT ${CMAKE_BINARY_DIR}/marshallers/hyscan-control-marshallers.c
                    COMMAND ${MARSHAL_COMPILER} --header
                            hyscan-control-marshallers.list >
                            ${CMAKE_BINARY_DIR}/marshallers/hyscan-control-marshallers.h
                    COMMAND ${MARSHAL_COMPILER} --body
                            hyscan-control-marshallers.list >
                            ${CMAKE_BINARY_DIR}/marshallers/hyscan-control-marshallers.c
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    DEPENDS hyscan-control-marshallers.list
                    VERBATIM)

add_library (hyscancontrol SHARED
             hyscan-sonar-box.c
             hyscan-sonar-schema.c
             hyscan-sensor-control.c
             hyscan-generator-control.c
             hyscan-tvg-control.c
             hyscan-sonar-control.c
             hyscan-sensor-control-server.c
             hyscan-generator-control-server.c
             hyscan-tvg-control-server.c
             hyscan-sonar-control-server.c
             hyscan-sensor-proxy.c
             hyscan-generator-proxy.c
             hyscan-tvg-proxy.c
             hyscan-sonar-proxy.c
             hyscan-control-common.c
             hyscan-proxy-common.c
             hyscan-sonar-discover.c
             hyscan-sonar-driver.c
             ${CMAKE_BINARY_DIR}/marshallers/hyscan-control-marshallers.c)

target_link_libraries (hyscancontrol ${GLIB2_LIBRARIES} ${GMODULE2_LIBRARIES} ${HYSCAN_LIBRARIES})

set_target_properties (hyscancontrol PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
