
add_executable (sonar-control-test sonar-control-test.c)

target_link_libraries (sonar-control-test ${GLIB2_LIBRARIES} ${ADDON_LIBRARIES} hyscancontrol)
