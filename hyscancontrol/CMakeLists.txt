
add_library (hyscancontrol SHARED
             hyscan-sonar-sensor.c
             hyscan-sonar-writer.c)

target_link_libraries (hyscancontrol ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES})
