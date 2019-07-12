/* hyscan-gtk-waterfall-state.h
 *
 * Copyright 2017-2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#ifndef __HYSCAN_GTK_WATERFALL_STATE_H__
#define __HYSCAN_GTK_WATERFALL_STATE_H__

#include <hyscan-db.h>
#include <hyscan-tile-common.h>
#include <hyscan-amplitude-factory.h>
#include <hyscan-depth-factory.h>
#include "hyscan-gtk-layer-container.h"

G_BEGIN_DECLS

/**
 * HyScanWaterfallDisplayType:
 * @HYSCAN_WATERFALL_DISPLAY_SIDESCAN: ГБО
 * @HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER: Эхолот
 *
 * Типы отображения.
 */
typedef enum
{
  HYSCAN_WATERFALL_DISPLAY_SIDESCAN,
  HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER,
} HyScanWaterfallDisplayType;

#define HYSCAN_TYPE_GTK_WATERFALL_STATE             (hyscan_gtk_waterfall_state_get_type ())
#define HYSCAN_GTK_WATERFALL_STATE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_STATE, HyScanGtkWaterfallState))
#define HYSCAN_IS_GTK_WATERFALL_STATE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_STATE))
#define HYSCAN_GTK_WATERFALL_STATE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_STATE, HyScanGtkWaterfallStateClass))
#define HYSCAN_IS_GTK_WATERFALL_STATE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_STATE))
#define HYSCAN_GTK_WATERFALL_STATE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_STATE, HyScanGtkWaterfallStateClass))

typedef struct _HyScanGtkWaterfallState HyScanGtkWaterfallState;
typedef struct _HyScanGtkWaterfallStatePrivate HyScanGtkWaterfallStatePrivate;
typedef struct _HyScanGtkWaterfallStateClass HyScanGtkWaterfallStateClass;

struct _HyScanGtkWaterfallState
{
  HyScanGtkLayerContainer parent_instance;

  HyScanGtkWaterfallStatePrivate *priv;
};

struct _HyScanGtkWaterfallStateClass
{
  HyScanGtkLayerContainerClass parent_class;
};

HYSCAN_API
GType                   hyscan_gtk_waterfall_state_get_type                    (void);

HYSCAN_API
void                    hyscan_gtk_waterfall_state_echosounder                 (HyScanGtkWaterfallState *state,
                                                                                HyScanSourceType         source);

HYSCAN_API
void                    hyscan_gtk_waterfall_state_sidescan                    (HyScanGtkWaterfallState *state,
                                                                                HyScanSourceType         lsource,
                                                                                HyScanSourceType         rsource);

HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_tile_flags              (HyScanGtkWaterfallState *state,
                                                                                HyScanTileFlags          flags);

HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_track                   (HyScanGtkWaterfallState *state,
                                                                                HyScanDB                *db,
                                                                                const gchar             *project,
                                                                                const gchar             *track);

HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_ship_speed              (HyScanGtkWaterfallState *state,
                                                                                gfloat                   speed);

HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_sound_velocity          (HyScanGtkWaterfallState *state,
                                                                                GArray                  *velocity);

HYSCAN_API
HyScanWaterfallDisplayType  hyscan_gtk_waterfall_state_get_sources             (HyScanGtkWaterfallState *state,
                                                                                HyScanSourceType        *lsource,
                                                                                HyScanSourceType        *rsource);

HYSCAN_API
HyScanTileFlags         hyscan_gtk_waterfall_state_get_tile_flags              (HyScanGtkWaterfallState *state);

HYSCAN_API
HyScanAmplitudeFactory * hyscan_gtk_waterfall_state_get_amp_factory            (HyScanGtkWaterfallState *state);

HYSCAN_API
HyScanDepthFactory *    hyscan_gtk_waterfall_state_get_dpt_factory            (HyScanGtkWaterfallState *state);

HYSCAN_API
HyScanCache *           hyscan_gtk_waterfall_state_get_cache                   (HyScanGtkWaterfallState *state);

HYSCAN_API
void                    hyscan_gtk_waterfall_state_get_track                   (HyScanGtkWaterfallState *state,
                                                                                HyScanDB               **db,
                                                                                gchar                  **project,
                                                                                gchar                  **track);

HYSCAN_API
gfloat                  hyscan_gtk_waterfall_state_get_ship_speed              (HyScanGtkWaterfallState *state);

HYSCAN_API
GArray *                hyscan_gtk_waterfall_state_get_sound_velocity          (HyScanGtkWaterfallState *state);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_STATE_H__ */
