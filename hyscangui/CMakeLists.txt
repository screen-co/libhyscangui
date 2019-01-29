find_program (MARSHAL_COMPILER glib-genmarshal)
if (${MARSHAL_COMPILER} STREQUAL MARSHAL_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib marshallers compiler not found.")
endif ()

file (MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/marshallers")
include_directories ("${CMAKE_BINARY_DIR}/marshallers")

add_custom_command (OUTPUT "${CMAKE_BINARY_DIR}/marshallers/hyscan-gui-marshallers.c"
                    COMMAND ${MARSHAL_COMPILER} --header
                            --prefix hyscan_gui_marshal
                            hyscan-gui-marshallers.list >
                            "${CMAKE_BINARY_DIR}/marshallers/hyscan-gui-marshallers.h"
                    COMMAND ${MARSHAL_COMPILER} --body
                            --prefix hyscan_gui_marshal
                            hyscan-gui-marshallers.list >
                            "${CMAKE_BINARY_DIR}/marshallers/hyscan-gui-marshallers.c"
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    DEPENDS hyscan-gui-marshallers.list
                    VERBATIM)

add_library (${HYSCAN_GUI_LIBRARY} SHARED
             hyscan-gtk-area.c
             hyscan-gtk-spin-button.c
             hyscan-gtk-datetime.c
             hyscan-gtk-param-key.c
             hyscan-gtk-param.c
             hyscan-gtk-param-list.c
             hyscan-gtk-param-cc.c
             hyscan-gtk-param-tree.c
             hyscan-task-queue.c
             hyscan-gtk-map.c
             hyscan-gtk-map-tile.c
             hyscan-gtk-map-tiles.c
             hyscan-gtk-map-float.c
             hyscan-gtk-map-tile-source.c
             hyscan-gtk-map-tiles-osm.c
             ${CMAKE_BINARY_DIR}/marshallers/hyscan-gui-marshallers.c)

target_link_libraries (${HYSCAN_GUI_LIBRARY} ${GTK3_LIBRARIES} ${HYSCAN_LIBRARIES} ${GTKCIFROAREA_LIBRARIES})

set_target_properties (${HYSCAN_GUI_LIBRARY} PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
set_target_properties (${HYSCAN_GUI_LIBRARY} PROPERTIES SOVERSION ${HYSCAN_GUI_VERSION})

configure_file (hyscangui.pc.in
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscangui-${HYSCAN_MAJOR_VERSION}.pc"
                @ONLY)

install (TARGETS ${HYSCAN_GUI_LIBRARY}
         COMPONENT runtime
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install (FILES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscangui-${HYSCAN_MAJOR_VERSION}.pc"
         COMPONENT development
         DESTINATION "lib/pkgconfig"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install (FILES hyscan-gtk-area.h
         COMPONENT development
         DESTINATION "include/hyscan-${HYSCAN_MAJOR_VERSION}/hyscangui"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
