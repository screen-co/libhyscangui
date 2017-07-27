
set (TEST_LIBRARIES ${GTK3_LIBRARIES}
                    ${LIBXML2_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    hyscangui)

add_executable (gtk-area-test gtk-area-test.c)

target_link_libraries (gtk-area-test ${TEST_LIBRARIES})
