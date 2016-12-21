
add_executable (nmea-uart-test nmea-uart-test.c)
add_executable (nmea-udp-test nmea-udp-test.c)
add_executable (nmea-uart2udp nmea-uart2udp.c)
add_executable (sonar-control-test sonar-control-test.c)
add_executable (sonar-control-data-test sonar-control-data-test.c)

target_link_libraries (nmea-uart-test ${GLIB2_LIBRARIES} hyscancontrol)
target_link_libraries (nmea-udp-test ${GLIB2_LIBRARIES} hyscancontrol)
target_link_libraries (nmea-uart2udp ${GLIB2_LIBRARIES} hyscancontrol)
target_link_libraries (sonar-control-test ${GLIB2_LIBRARIES} ${LIBXML2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancontrol)
target_link_libraries (sonar-control-data-test ${GLIB2_LIBRARIES} ${LIBXML2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancontrol)
