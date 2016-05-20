
#
# Searching for Glib resource compiler.
find_program (RESOURCE_COMPILER glib-compile-resources)
if (${RESOURCE_COMPILER} STREQUAL RESOURCE_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib resource compiler not found.")
endif ()

#
# Generate resource C file.
file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/resources)
execute_process (COMMAND ${RESOURCE_COMPILER}
                         --generate-source
                         --target ${CMAKE_BINARY_DIR}/resources/hyscan-sonar-dummy-ssse-resources.c
                         hyscan-sonar-dummy-ssse-resources.xml
                         WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable (dummy-ssse-server
                dummy-ssse-server.c
                hyscan-sonar-dummy-ssse.c
                ${CMAKE_BINARY_DIR}/resources/hyscan-sonar-dummy-ssse-resources.c)
add_executable (dummy-ssse-client
                dummy-ssse-client.c
                hyscan-sonar-dummy-ssse.c
                ${CMAKE_BINARY_DIR}/resources/hyscan-sonar-dummy-ssse-resources.c)

target_link_libraries (dummy-ssse-server ${GLIB2_LIBRARIES} hyscantypes hyscandb hyscansonar hyscancore hyscancontrol)
target_link_libraries (dummy-ssse-client ${GLIB2_LIBRARIES} hyscantypes hyscandb hyscansonar hyscancore hyscancontrol)
