
add_executable (sonar-control-test sonar-control-test.c hyscan-sonar-control-dummy.c)

target_link_libraries (sonar-control-test ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancontrol)
