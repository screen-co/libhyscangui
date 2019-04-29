
set (TEST_LIBRARIES ${GTK3_LIBRARIES}
                    ${LIBXML2_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    ${HYSCAN_GUI_LIBRARY})

add_executable (gtk-area-test gtk-area-test.c)
add_executable (gtk-param-test gtk-param-test.c)
add_executable (gtk-spin-button-test gtk-spin-button-test.c)
add_executable (gtk-datetime-test gtk-datetime-test.c)
add_executable (gtk-map-test gtk-map-test.c hyscan-gtk-map-kit.c)
add_executable (gtk-map-track-mod-test gtk-map-track-mod-test.c hyscan-gtk-map-kit.c)
add_executable (task-queue-test task-queue-test.c)
add_executable (mercator-test mercator-test.c)
add_executable (tile-source-test tile-source-test.c)
add_executable (nmea-file-device-test nmea-file-device-test.c)
add_executable (navigation-model-test navigation-model-test.c)
add_executable (nmea-file2udp nmea-file2udp.c)
add_executable (tile-test tile-test.c)
add_executable (tile-loader-test tile-loader-test.c)

target_link_libraries (gtk-area-test ${TEST_LIBRARIES})
target_link_libraries (gtk-param-test ${TEST_LIBRARIES})
target_link_libraries (gtk-spin-button-test ${TEST_LIBRARIES})
target_link_libraries (gtk-datetime-test ${TEST_LIBRARIES})
target_link_libraries (gtk-map-test ${TEST_LIBRARIES})
target_link_libraries (gtk-map-track-mod-test ${TEST_LIBRARIES})
target_link_libraries (task-queue-test ${TEST_LIBRARIES})
target_link_libraries (mercator-test ${TEST_LIBRARIES})
target_link_libraries (tile-source-test ${TEST_LIBRARIES})
target_link_libraries (nmea-file-device-test ${TEST_LIBRARIES})
target_link_libraries (navigation-model-test ${TEST_LIBRARIES})
target_link_libraries (nmea-file2udp ${TEST_LIBRARIES})
target_link_libraries (tile-test ${TEST_LIBRARIES})
target_link_libraries (tile-loader-test ${TEST_LIBRARIES})

install (TARGETS gtk-area-test
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
