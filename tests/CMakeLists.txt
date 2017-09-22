
set (TEST_LIBRARIES ${GTK3_LIBRARIES}
                    ${LIBXML2_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    ${HYSCAN_GUI_LIBRARY})

add_executable (gtk-area-test gtk-area-test.c)
add_executable (gtk-waterfall-test gtk-waterfall-test.c)
add_executable (waterfall-plus waterfall-plus.c)

target_link_libraries (gtk-area-test ${TEST_LIBRARIES})
target_link_libraries (gtk-waterfall-test ${TEST_LIBRARIES})
target_link_libraries (waterfall-plus ${TEST_LIBRARIES})

install (TARGETS gtk-area-test
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
