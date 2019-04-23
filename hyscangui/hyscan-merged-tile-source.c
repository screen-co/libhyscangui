/* hyscan-gtk-map-tile-source.c
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

/**
 * SECTION: hyscan-merged-map-tile-source
 * @Short_description: Композитный источник тайлов
 * @Title: HyScanMergedMapTileSource
 *
 * Класс реализует интерфейс #HyScanGtkMapTileSource.
 *
 * Позволяет загружать тайлы из нескольних источником одновременно. Класс
 * накладывает изображения тайла из разных источников друг на друга, формируя
 * таким образом общую картинку. Для добавления источников используйте функцию
 * hyscan_merged_tile_source_append().
 *
 * Верхние источники тайлов должны формировать изображение с прозрачностью, чтобы
 * было видно изображение нижних слоёв.
 *
 * Примеры применения композитного источника:
 * - отображения отметок гаваней, маяков и прочих навигационных ориентиров
 *   поверх выбранной карты;
 * - размещение более подробной карты отдельной местности поверх стандартной карты;
 * - использование разных источников тайлов на разных масштабах.
 *
 */

#include "hyscan-merged-tile-source.h"

struct _HyScanMergedTileSourcePrivate
{
  GList                             *sources;      /* Список источников тайлов. */

  HyScanGeoProjection               *projection;   /* Используемая картографическая проекция. */
  HyScanGtkMapTileGrid              *grid;         /* Параметры тайловой сетки. */
  guint                              min_zoom;     /* Минимальный допустимый масштаб. */
  guint                              max_zoom;     /* Максимальный допустимый масштаб. */
};

static void hyscan_merged_tile_source_interface_init (HyScanGtkMapTileSourceInterface *iface);

static void hyscan_merged_tile_source_object_finalize (GObject *object);

G_DEFINE_TYPE_WITH_CODE (HyScanMergedTileSource, hyscan_merged_tile_source, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanMergedTileSource)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_MAP_TILE_SOURCE,
                                                hyscan_merged_tile_source_interface_init))

static void
hyscan_merged_tile_source_class_init (HyScanMergedTileSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hyscan_merged_tile_source_object_finalize;
}

static void
hyscan_merged_tile_source_init (HyScanMergedTileSource *merged_tile_source)
{
  merged_tile_source->priv = hyscan_merged_tile_source_get_instance_private (merged_tile_source);
}

static void
hyscan_merged_tile_source_object_finalize (GObject *object)
{
  HyScanMergedTileSource *merged_tile_source = HYSCAN_MERGED_TILE_SOURCE (object);
  HyScanMergedTileSourcePrivate *priv = merged_tile_source->priv;

  g_list_free_full (priv->sources, g_object_unref);
  g_clear_object (&priv->projection);
  g_clear_object (&priv->grid);

  G_OBJECT_CLASS (hyscan_merged_tile_source_parent_class)->finalize (object);
}

static gboolean
hyscan_merged_tile_source_fill_tile (HyScanGtkMapTileSource *source,
                                     HyScanGtkMapTile       *tile,
                                     GCancellable           *cancellable)
{
  HyScanMergedTileSource *merged = HYSCAN_MERGED_TILE_SOURCE (source);
  HyScanMergedTileSourcePrivate *priv = merged->priv;

  GList *source_l;
  cairo_surface_t *surface;
  cairo_t *cairo = NULL;

  g_return_val_if_fail (priv->sources != NULL, FALSE);

  /* Получаем поверхности из каждого источника и рисуем их поверх друг друга. */
  for (source_l = priv->sources; source_l != NULL; source_l = source_l->next)
    {
      gboolean fill_status;

      fill_status = hyscan_gtk_map_tile_source_fill (source_l->data, tile, cancellable);
      if (fill_status == FALSE)
        continue;

      surface = hyscan_gtk_map_tile_get_surface (tile);
      if (cairo == NULL)
        {
          cairo = cairo_create (surface);
        }
      else
        {
          cairo_set_source_surface (cairo, surface, 0, 0);
          cairo_paint (cairo);
        }

      g_clear_pointer (&surface, cairo_surface_destroy);
    }

  /* Ни один из источников ничерго не заполнил - неудача. */
  if (cairo == NULL)
    return FALSE;

  /* Устанавливаем полученное изображение тайла. */
  hyscan_gtk_map_tile_set_surface (tile, cairo_get_target (cairo));

  /* Освобождаем память. */
  cairo_destroy (cairo);

  return TRUE;
}

static HyScanGtkMapTileGrid *
hyscan_merged_tile_source_get_grid (HyScanGtkMapTileSource *source)
{
  HyScanMergedTileSource *merged = HYSCAN_MERGED_TILE_SOURCE (source);
  HyScanMergedTileSourcePrivate *priv = merged->priv;

  return g_object_ref (priv->grid);
}

static HyScanGeoProjection *
hyscan_merged_tile_source_get_projection (HyScanGtkMapTileSource *source)
{
  HyScanMergedTileSource *merged = HYSCAN_MERGED_TILE_SOURCE (source);
  HyScanMergedTileSourcePrivate *priv = merged->priv;

  return g_object_ref (priv->projection);
}

static void
hyscan_merged_tile_source_get_zoom_limits (HyScanGtkMapTileSource *source,
                                           guint                  *min_zoom,
                                           guint                  *max_zoom)
{
  HyScanMergedTileSource *merged = HYSCAN_MERGED_TILE_SOURCE (source);
  HyScanMergedTileSourcePrivate *priv = merged->priv;

  (min_zoom != NULL) ? *min_zoom = priv->min_zoom : 0;
  (max_zoom != NULL) ? *max_zoom = priv->max_zoom : 0;
}

static void
hyscan_merged_tile_source_interface_init (HyScanGtkMapTileSourceInterface *iface)
{
  iface->get_projection = hyscan_merged_tile_source_get_projection;
  iface->get_grid = hyscan_merged_tile_source_get_grid;
  iface->get_zoom_limits = hyscan_merged_tile_source_get_zoom_limits;
  iface->fill_tile = hyscan_merged_tile_source_fill_tile;
}

HyScanMergedTileSource *
hyscan_merged_tile_source_new (void)
{
  return g_object_new (HYSCAN_TYPE_MERGED_TILE_SOURCE, NULL);
}

/**
 * hyscan_merged_tile_source_append:
 * @merged: указатель на #HyScanMergedTileSource
 * @source: дополнительный источник #HyScanGtkMapTileSource
 *
 * Добавляет дополнительный источник тайлов, изображение которого будет размещено
 * поверх ранее добавленных источников.
 *
 * Первый добавленный источник определяет используемую картографическую проекцию
 * и параметры тайловой сетки. Все последующие источники должны иметь те же самые
 * параметры сетки и проекции.
 *
 * Область доступных масштабов является объединением допустимых масштабов всех
 * добавленных источников, т.е. расширяется по мере добавления новых источниклв.
 *
 * Returns: %TRUE, если источник тайлов успешно добавлен
 */
gboolean
hyscan_merged_tile_source_append (HyScanMergedTileSource *merged,
                                  HyScanGtkMapTileSource *source)
{
  HyScanMergedTileSourcePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_MERGED_TILE_SOURCE (merged), FALSE);
  priv = merged->priv;

  if (priv->sources == NULL)
    {
      priv->grid = hyscan_gtk_map_tile_source_get_grid (source);
      priv->projection = hyscan_gtk_map_tile_source_get_projection (source);
      hyscan_gtk_map_tile_source_get_zoom_limits (source, &priv->min_zoom, &priv->max_zoom);
    }
  else
    {
      guint min_zoom, max_zoom;
      guint merged_hash, source_hash;
      HyScanGeoProjection *source_projection;

      /* Проверяем, что добавляемый источник совместим с уже добавленными. */
      source_projection = hyscan_gtk_map_tile_source_get_projection (source);
      source_hash = hyscan_geo_projection_hash (priv->projection);
      merged_hash = hyscan_geo_projection_hash (priv->projection);
      g_object_unref (source_projection);
      if (merged_hash != source_hash)
        {
          g_warning ("HyScanMergedTileSource: source projection is not compatible with current one");
          return FALSE;
        }

      // todo: test tile grid compatibility?

      /* Расширяем диапазон доступных масштабов с учётом нового источника. */
      hyscan_gtk_map_tile_source_get_zoom_limits (source, &min_zoom, &max_zoom);
      priv->min_zoom = MIN (min_zoom, priv->min_zoom);
      priv->max_zoom = MIN (max_zoom, priv->max_zoom);
    }

  priv->sources = g_list_append (priv->sources, g_object_ref (source));

  return TRUE;
}
