/* hyscan-gtk-waterfall-player.h
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

#ifndef __HYSCAN_GTK_WATERFALL_PLAYER_H__
#define __HYSCAN_GTK_WATERFALL_PLAYER_H__

#include "hyscan-gtk-layer.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_PLAYER             (hyscan_gtk_waterfall_player_get_type ())
#define HYSCAN_GTK_WATERFALL_PLAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_PLAYER, HyScanGtkWaterfallPlayer))
#define HYSCAN_IS_GTK_WATERFALL_PLAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_PLAYER))
#define HYSCAN_GTK_WATERFALL_PLAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_PLAYER, HyScanGtkWaterfallPlayerClass))
#define HYSCAN_IS_GTK_WATERFALL_PLAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_PLAYER))
#define HYSCAN_GTK_WATERFALL_PLAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_PLAYER, HyScanGtkWaterfallPlayerClass))

typedef struct _HyScanGtkWaterfallPlayer HyScanGtkWaterfallPlayer;
typedef struct _HyScanGtkWaterfallPlayerPrivate HyScanGtkWaterfallPlayerPrivate;
typedef struct _HyScanGtkWaterfallPlayerClass HyScanGtkWaterfallPlayerClass;

struct _HyScanGtkWaterfallPlayer
{
  GInitiallyUnowned parent_instance;

  HyScanGtkWaterfallPlayerPrivate *priv;
};

struct _HyScanGtkWaterfallPlayerClass
{
  GInitiallyUnownedClass parent_class;
};

HYSCAN_API
GType           hyscan_gtk_waterfall_player_get_type    (void);

HYSCAN_API
HyScanGtkWaterfallPlayer* hyscan_gtk_waterfall_player_new (void);

HYSCAN_API
void            hyscan_gtk_waterfall_player_set_fps     (HyScanGtkWaterfallPlayer *player,
                                                         guint                     fps);

HYSCAN_API
void            hyscan_gtk_waterfall_player_set_speed   (HyScanGtkWaterfallPlayer *player,
                                                        gdouble                   speed);

HYSCAN_API
gdouble         hyscan_gtk_waterfall_player_get_speed   (HyScanGtkWaterfallPlayer *player);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_PLAYER_H__ */
