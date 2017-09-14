
set (TEST_LIBRARIES ${GLIB2_LIBRARIES}
                    ${LIBXML2_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    ${HYSCAN_CONTROL_LIBRARY})

add_executable (nmea-uart-test nmea-uart-test.c)
add_executable (nmea-udp-test nmea-udp-test.c)
add_executable (nmea-uart2udp nmea-uart2udp.c)
add_executable (dummy-sonar-server dummy-sonar-server.c hyscan-sonar-dummy.c)
add_executable (dummy-sonar-client dummy-sonar-client.c hyscan-sonar-dummy.c)
add_executable (sonar-control-test sonar-control-test.c)
add_executable (sonar-control-data-test sonar-control-data-test.c)

target_link_libraries (nmea-uart-test ${TEST_LIBRARIES})
target_link_libraries (nmea-udp-test ${TEST_LIBRARIES})
target_link_libraries (nmea-uart2udp ${TEST_LIBRARIES})
target_link_libraries (dummy-sonar-server ${TEST_LIBRARIES})
target_link_libraries (dummy-sonar-client ${TEST_LIBRARIES})
target_link_libraries (sonar-control-test ${TEST_LIBRARIES})
target_link_libraries (sonar-control-data-test ${TEST_LIBRARIES})

install (TARGETS nmea-uart-test
                 nmea-udp-test
                 nmea-uart2udp
                 dummy-sonar-server
                 dummy-sonar-client
                 sonar-control-test
                 sonar-control-data-test
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
