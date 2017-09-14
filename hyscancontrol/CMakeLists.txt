
find_program (MARSHAL_COMPILER glib-genmarshal)
if (${MARSHAL_COMPILER} STREQUAL MARSHAL_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib marshallers compiler not found.")
endif ()

file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/marshallers)
include_directories (${CMAKE_BINARY_DIR}/marshallers)

add_custom_command (OUTPUT "${CMAKE_BINARY_DIR}/marshallers/hyscan-control-marshallers.c"
                    COMMAND ${MARSHAL_COMPILER} --header
                            --prefix hyscan_control_marshal
                            hyscan-control-marshallers.list >
                            "${CMAKE_BINARY_DIR}/marshallers/hyscan-control-marshallers.h"
                    COMMAND ${MARSHAL_COMPILER} --body
                            --prefix hyscan_control_marshal
                            hyscan-control-marshallers.list >
                            "${CMAKE_BINARY_DIR}/marshallers/hyscan-control-marshallers.c"
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    DEPENDS hyscan-control-marshallers.list
                    VERBATIM)

add_library (${HYSCAN_CONTROL_LIBRARY} SHARED
             hyscan-nmea-uart.c
             hyscan-nmea-udp.c
             hyscan-sonar-box.c
             hyscan-sonar-schema.c
             hyscan-sonar-server.c
             hyscan-sonar-client.c
             hyscan-sonar-rpc.c
             hyscan-sensor-control.c
             hyscan-generator-control.c
             hyscan-tvg-control.c
             hyscan-sonar-control.c
             hyscan-sensor-control-server.c
             hyscan-generator-control-server.c
             hyscan-tvg-control-server.c
             hyscan-sonar-control-server.c
             hyscan-control-common.c
             hyscan-sonar-discover.c
             hyscan-sonar-driver.c
             "${CMAKE_BINARY_DIR}/marshallers/hyscan-control-marshallers.c")

target_link_libraries (${HYSCAN_CONTROL_LIBRARY}
                       ${WIN32_LIBRARIES}
                       ${GLIB2_LIBRARIES}
                       ${GMODULE2_LIBRARIES}
                       ${ZLIB_LIBRARIES}
                       ${URPC_LIBRARIES}
                       ${HYSCAN_LIBRARIES})

set_target_properties (${HYSCAN_CONTROL_LIBRARY} PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
set_target_properties (${HYSCAN_CONTROL_LIBRARY} PROPERTIES SOVERSION ${HYSCAN_CONTROL_VERSION})

configure_file (hyscancontrol.pc.in
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscancontrol-${HYSCAN_MAJOR_VERSION}.pc"
                @ONLY)

install (TARGETS ${HYSCAN_CONTROL_LIBRARY}
         COMPONENT runtime
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install (FILES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscancontrol-${HYSCAN_MAJOR_VERSION}.pc"
         COMPONENT development
         DESTINATION "lib/pkgconfig"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install (FILES hyscan-control-common.h
               hyscan-sonar-control.h
               hyscan-generator-control.h
               hyscan-tvg-control.h
               hyscan-sensor-control.h
               hyscan-sonar-client.h
               hyscan-sonar-discover.h
               hyscan-sonar-driver.h
               hyscan-sonar-server.h
               hyscan-sonar-messages.h
               hyscan-generator-control-server.h
               hyscan-sonar-control-server.h
               hyscan-tvg-control-server.h
               hyscan-sensor-control-server.h
               hyscan-nmea-udp.h
               hyscan-nmea-uart.h
               hyscan-sonar-box.h
               hyscan-sonar-schema.h
         COMPONENT development
         DESTINATION "include/hyscan-${HYSCAN_MAJOR_VERSION}/hyscancontrol"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
