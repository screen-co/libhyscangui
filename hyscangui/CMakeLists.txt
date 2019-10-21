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
             hyscan-gtk-waterfall-state.c
             hyscan-gtk-waterfall.c
             hyscan-gtk-waterfall-control.c
             hyscan-gtk-waterfall-grid.c
             hyscan-gtk-waterfall-tools.c
             hyscan-gtk-waterfall-mark.c
             hyscan-gtk-waterfall-meter.c
             hyscan-gtk-waterfall-player.c
             hyscan-gtk-waterfall-magnifier.c
             # hyscan-gtk-waterfall-depth.c
             hyscan-gtk-spin-button.c
             hyscan-gtk-datetime.c
             hyscan-gtk-param-key.c
             hyscan-gtk-param.c
             hyscan-gtk-param-list.c
             hyscan-gtk-param-cc.c
             hyscan-gtk-param-tree.c
             hyscan-gtk-layer-list.c
             hyscan-gtk-planner-editor.c
             hyscan-gtk-planner-zeditor.c
             hyscan-gtk-planner-origin.c
             hyscan-gtk-planner-status.c
             hyscan-cairo.c

             #[[ Виджет карты ]]
             hyscan-gtk-map.c
             hyscan-profile-map.c

             #[[ Слои ]]
             hyscan-gtk-layer-container.c
             hyscan-gtk-layer.c
             hyscan-gtk-map-base.c
             hyscan-gtk-map-tiled.c
             hyscan-gtk-map-control.c
             hyscan-gtk-map-ruler.c
             hyscan-gtk-map-grid.c
             hyscan-gtk-map-scale.c
             hyscan-gtk-map-pin.c
             hyscan-gtk-map-nav.c
             hyscan-gtk-map-track-item.c
             hyscan-gtk-map-track.c
             hyscan-gtk-map-wfmark.c
             hyscan-gtk-map-geomark.c
             hyscan-gtk-map-planner.c
             hyscan-gtk-map-steer.c

             #[[ Загрузка тайлов ]]
             hyscan-map-tile.c
             hyscan-map-tile-source.c
             hyscan-map-tile-source-web.c
             hyscan-map-tile-source-file.c
             hyscan-map-tile-source-blend.c
             hyscan-map-tile-loader.c

             # [[ Модели ]]
             hyscan-mark-location.c
             hyscan-mark-loc-model.c
             hyscan-planner-selection.c
             ${CMAKE_BINARY_DIR}/marshallers/hyscan-gui-marshallers.c)

target_link_libraries (${HYSCAN_GUI_LIBRARY}
                       ${GTK3_LIBRARIES}
                       ${HYSCAN_LIBRARIES}
                       ${GTKCIFROAREA_LIBRARIES}
                       ${LIBSOUP_LIBRARIES}
                       ${GDK_PIXBUF_LIBRARIES})

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
               hyscan-gtk-waterfall-state.h
               hyscan-gtk-waterfall.h
               hyscan-gtk-waterfall-layer.h
               hyscan-gtk-waterfall-mayer.h
               hyscan-gtk-waterfall-control.h
               hyscan-gtk-waterfall-grid.h
               hyscan-gtk-waterfall-tools.h
               hyscan-gtk-waterfall-mark.h
               hyscan-gtk-waterfall-meter.h
               hyscan-gtk-waterfall-player.h
               hyscan-gtk-waterfall-magnifier.h
               hyscan-gtk-layer-list.h
               hyscan-gtk-planner-editor.h
               hyscan-gtk-planner-zeditor.h
               hyscan-gtk-planner-origin.h
               hyscan-gtk-planner-status.h
               hyscan-gtk-layer-common.h
               hyscan-gtk-layer-container.h
               hyscan-gtk-layer.h
               hyscan-gtk-map.h
               hyscan-profile-map.h
               hyscan-gtk-map-base.h
               hyscan-gtk-map-tiled.h
               hyscan-gtk-map-control.h
               hyscan-gtk-map-ruler.h
               hyscan-gtk-map-grid.h
               hyscan-gtk-map-scale.h
               hyscan-gtk-map-pin.h
               hyscan-gtk-map-planner.h
               hyscan-gtk-map-steer.h
               hyscan-gtk-map-nav.h
               hyscan-gtk-map-track-item.h
               hyscan-gtk-map-track.h
               hyscan-gtk-map-wfmark.h
               hyscan-gtk-map-geomark.h
               hyscan-map-tile.h
               hyscan-map-tile-source.h
               hyscan-map-tile-source-web.h
               hyscan-map-tile-source-file.h
               hyscan-map-tile-source-blend.h
               hyscan-map-tile-loader.h
               hyscan-planner-selection.h
               hyscan-cairo.h
         COMPONENT development
         DESTINATION "include/hyscan-${HYSCAN_MAJOR_VERSION}/hyscangui"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
