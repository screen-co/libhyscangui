set (TEST_LIBRARIES ${GTK3_LIBRARIES}
                    ${LIBXML2_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    ${HYSCAN_GUI_LIBRARY})

add_executable (gtk-area-test gtk-area-test.c)
add_executable (waterfall-plus waterfall-plus.c)

target_link_libraries (gtk-area-test ${TEST_LIBRARIES})
target_link_libraries (waterfall-plus ${TEST_LIBRARIES})

# ***
#add_executable (gtk-param-test gtk-param-test.c)
#add_executable (gtk-spin-button-test gtk-spin-button-test.c)
# ***

add_executable (gtk-datetime-test gtk-datetime-test.c)
add_executable (gtk-map-test gtk-map-test.c hyscan-gtk-map-kit.c)
add_executable (gtk-map-track-mod-test gtk-map-track-mod-test.c hyscan-gtk-map-kit.c)
add_executable (tile-source-test tile-source-test.c)
add_executable (tile-test tile-test.c)
add_executable (tile-loader-test tile-loader-test.c)
add_executable (tile-loader tile-loader.c)
add_executable (gtk-export-test gtk-export-test.c)
add_executable (gtk-map-param-test gtk-map-param-test.c)
add_executable (gtk-layer-list-test gtk-layer-list-test.c)

target_link_libraries (gtk-area-test ${TEST_LIBRARIES})
#target_link_libraries (gtk-param-test ${TEST_LIBRARIES})
#target_link_libraries (gtk-spin-button-test ${TEST_LIBRARIES})
target_link_libraries (gtk-datetime-test ${TEST_LIBRARIES})
target_link_libraries (gtk-map-test ${TEST_LIBRARIES})
target_link_libraries (gtk-map-track-mod-test ${TEST_LIBRARIES})
target_link_libraries (tile-source-test ${TEST_LIBRARIES})
target_link_libraries (tile-test ${TEST_LIBRARIES})
target_link_libraries (tile-loader-test ${TEST_LIBRARIES})
target_link_libraries (tile-loader ${TEST_LIBRARIES})
target_link_libraries (gtk-export-test ${TEST_LIBRARIES})
target_link_libraries (gtk-map-param-test ${TEST_LIBRARIES})
target_link_libraries (gtk-layer-list-test ${TEST_LIBRARIES})

add_test (NAME TileTest COMMAND tile-test
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME TileSourceTest COMMAND tile-source-test
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME TileLoaderTest COMMAND tile-loader-test
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

install (TARGETS gtk-area-test
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
