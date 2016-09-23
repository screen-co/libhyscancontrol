
add_executable (sonar-control-test sonar-control-test.c)
add_executable (sonar-control-data-test sonar-control-data-test.c)

target_link_libraries (sonar-control-test ${GLIB2_LIBRARIES} ${LIBXML2_LIBRARIES} ${ADDON_LIBRARIES} hyscancontrol)
target_link_libraries (sonar-control-data-test ${GLIB2_LIBRARIES} ${LIBXML2_LIBRARIES} ${ADDON_LIBRARIES} hyscancontrol)
