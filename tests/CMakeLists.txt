
set (TEST_LIBRARIES ${GTK3_LIBRARIES}
                    ${LIBXML2_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    ${HYSCAN_GUI_LIBRARY})

add_executable (gtk-area-test gtk-area-test.c)

target_link_libraries (gtk-area-test ${TEST_LIBRARIES})

install (TARGETS gtk-area-test
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
