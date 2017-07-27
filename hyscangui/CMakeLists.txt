
add_library (hyscangui SHARED
             hyscan-gtk-area.c)

target_link_libraries (hyscangui ${GTK3_LIBRARIES} ${HYSCAN_LIBRARIES})

set_target_properties (hyscangui PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
