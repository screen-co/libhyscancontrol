
add_executable (dummy-ssse-server dummy-ssse-server.c hyscan-sonar-dummy-ssse.c)
add_executable (dummy-ssse-client dummy-ssse-client.c hyscan-sonar-dummy-ssse.c)

target_link_libraries (dummy-ssse-server ${GLIB2_LIBRARIES} hyscantypes hyscandb hyscansonar hyscancore hyscancontrol)
target_link_libraries (dummy-ssse-client ${GLIB2_LIBRARIES} hyscantypes hyscandb hyscansonar hyscancore hyscancontrol)
