
add_library( hyscangui SHARED
             hyscan-gtk-area.c )

target_link_libraries( hyscangui ${GTK3_LIBRARIES} )
