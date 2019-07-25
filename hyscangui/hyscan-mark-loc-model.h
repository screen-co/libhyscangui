/* hyscan-mark-loc-model.h
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#ifndef __HYSCAN_MARK_LOC_MODEL_H__
#define __HYSCAN_MARK_LOC_MODEL_H__

#include <glib-object.h>
#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-mark-location.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MARK_LOC_MODEL             (hyscan_mark_loc_model_get_type ())
#define HYSCAN_MARK_LOC_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MARK_LOC_MODEL, HyScanMarkLocModel))
#define HYSCAN_IS_MARK_LOC_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MARK_LOC_MODEL))
#define HYSCAN_MARK_LOC_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MARK_LOC_MODEL, HyScanMarkLocModelClass))
#define HYSCAN_IS_MARK_LOC_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MARK_LOC_MODEL))
#define HYSCAN_MARK_LOC_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MARK_LOC_MODEL, HyScanMarkLocModelClass))

typedef struct _HyScanMarkLocModel HyScanMarkLocModel;
typedef struct _HyScanMarkLocModelPrivate HyScanMarkLocModelPrivate;
typedef struct _HyScanMarkLocModelClass HyScanMarkLocModelClass;

struct _HyScanMarkLocModel
{
  GObject parent_instance;

  HyScanMarkLocModelPrivate *priv;
};

struct _HyScanMarkLocModelClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_mark_loc_model_get_type         (void);

HYSCAN_API
HyScanMarkLocModel *   hyscan_mark_loc_model_new              (HyScanDB           *db,
                                                               HyScanCache        *cache);

HYSCAN_API
void                   hyscan_mark_loc_model_set_project      (HyScanMarkLocModel *ml_model,
                                                               const gchar        *project);

HYSCAN_API
GHashTable *           hyscan_mark_loc_model_get              (HyScanMarkLocModel *ml_model);

G_END_DECLS

#endif /* __HYSCAN_MARK_LOC_MODEL_H__ */
