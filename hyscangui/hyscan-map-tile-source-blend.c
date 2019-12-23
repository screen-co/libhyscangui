/* hyscan-map-tile-source-blend.c
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
 * SECTION: hyscan-map-tile-source-blend
 * @Short_description: Композитный источник тайлов
 * @Title: HyScanMapTileSourceBlend
 *
 * Класс реализует интерфейс #HyScanMapTileSource.
 *
 * Позволяет загружать тайлы из нескольних источником одновременно. Класс
 * накладывает изображения тайла из разных источников друг на друга, формируя
 * таким образом общую картинку. Для добавления источников используйте функцию
 * hyscan_map_tile_source_blend_append().
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

#include "hyscan-map-tile-source-blend.h"

struct _HyScanMapTileSourceBlendPrivate
{
  GList                             *sources;      /* Список источников тайлов. */

  HyScanGeoProjection               *projection;   /* Используемая картографическая проекция. */
  HyScanMapTileGrid                 *grid;         /* Параметры тайловой сетки. */
  guint                              min_zoom;     /* Минимальный допустимый масштаб. */
  guint                              max_zoom;     /* Максимальный допустимый масштаб. */
  guint                              hash;         /* Хэш источника тайлов. */
};

static void hyscan_map_tile_source_blend_interface_init (HyScanMapTileSourceInterface *iface);

static void hyscan_map_tile_source_blend_object_finalize (GObject *object);

G_DEFINE_TYPE_WITH_CODE (HyScanMapTileSourceBlend, hyscan_map_tile_source_blend, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanMapTileSourceBlend)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_MAP_TILE_SOURCE,
                                                hyscan_map_tile_source_blend_interface_init))

static void
hyscan_map_tile_source_blend_class_init (HyScanMapTileSourceBlendClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hyscan_map_tile_source_blend_object_finalize;
}

static void
hyscan_map_tile_source_blend_init (HyScanMapTileSourceBlend *map_tile_source_blend)
{
  map_tile_source_blend->priv = hyscan_map_tile_source_blend_get_instance_private (map_tile_source_blend);
}

static void
hyscan_map_tile_source_blend_object_finalize (GObject *object)
{
  HyScanMapTileSourceBlend *map_tile_source_blend = HYSCAN_MAP_TILE_SOURCE_BLEND (object);
  HyScanMapTileSourceBlendPrivate *priv = map_tile_source_blend->priv;

  g_list_free_full (priv->sources, g_object_unref);
  g_clear_object (&priv->projection);
  g_clear_object (&priv->grid);

  G_OBJECT_CLASS (hyscan_map_tile_source_blend_parent_class)->finalize (object);
}

/* Реализация #HyScanMapTileSourceInterface.fill_tile. */
static gboolean
hyscan_map_tile_source_blend_fill_tile (HyScanMapTileSource *source,
                                        HyScanMapTile       *tile,
                                        GCancellable        *cancellable)
{
  HyScanMapTileSourceBlend *blend = HYSCAN_MAP_TILE_SOURCE_BLEND (source);
  HyScanMapTileSourceBlendPrivate *priv = blend->priv;

  GList *source_l;
  cairo_surface_t *surface;
  cairo_t *cairo = NULL;

  g_return_val_if_fail (priv->sources != NULL, FALSE);

  /* Получаем поверхности из каждого источника и рисуем их поверх друг друга. */
  for (source_l = priv->sources; source_l != NULL; source_l = source_l->next)
    {
      gboolean fill_status;

      fill_status = hyscan_map_tile_source_fill (source_l->data, tile, cancellable);
      if (fill_status == FALSE)
        continue;

      surface = hyscan_map_tile_get_surface (tile);
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
  hyscan_map_tile_set_surface (tile, cairo_get_target (cairo));

  /* Освобождаем память. */
  cairo_destroy (cairo);

  return TRUE;
}

/* Реализация #HyScanMapTileSourceInterface.get_grid. */
static HyScanMapTileGrid *
hyscan_map_tile_source_blend_get_grid (HyScanMapTileSource *source)
{
  HyScanMapTileSourceBlend *blend = HYSCAN_MAP_TILE_SOURCE_BLEND (source);
  HyScanMapTileSourceBlendPrivate *priv = blend->priv;

  return g_object_ref (priv->grid);
}

/* Реализация #HyScanMapTileSourceInterface.get_projection. */
static HyScanGeoProjection *
hyscan_map_tile_source_blend_get_projection (HyScanMapTileSource *source)
{
  HyScanMapTileSourceBlend *blend = HYSCAN_MAP_TILE_SOURCE_BLEND (source);
  HyScanMapTileSourceBlendPrivate *priv = blend->priv;

  return g_object_ref (priv->projection);
}

/* Реализация #HyScanMapTileSourceInterface.get_hash. */
static guint
hyscan_map_tile_source_blend_hash (HyScanMapTileSource *source)
{
  HyScanMapTileSourceBlend *blend = HYSCAN_MAP_TILE_SOURCE_BLEND (source);
  HyScanMapTileSourceBlendPrivate *priv = blend->priv;

  return priv->hash;
}

static void
hyscan_map_tile_source_blend_interface_init (HyScanMapTileSourceInterface *iface)
{
  iface->get_projection = hyscan_map_tile_source_blend_get_projection;
  iface->get_grid = hyscan_map_tile_source_blend_get_grid;
  iface->fill_tile = hyscan_map_tile_source_blend_fill_tile;
  iface->hash = hyscan_map_tile_source_blend_hash;
}

/**
 * hyscan_map_tile_source_blend_new:
 *
 * Returns: новый композитный источник тайлов. Для удаления g_object_unref().
 */
HyScanMapTileSourceBlend *
hyscan_map_tile_source_blend_new (void)
{
  return g_object_new (HYSCAN_TYPE_MAP_TILE_SOURCE_BLEND, NULL);
}

/**
 * hyscan_map_tile_source_blend_append:
 * @blend: указатель на #HyScanMapTileSourceBlend
 * @source: дополнительный источник #HyScanMapTileSource
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
hyscan_map_tile_source_blend_append (HyScanMapTileSourceBlend *blend,
                                     HyScanMapTileSource      *source)
{
  HyScanMapTileSourceBlendPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_MAP_TILE_SOURCE_BLEND (blend), FALSE);
  priv = blend->priv;

  if (priv->sources == NULL)
    {
      priv->grid = hyscan_map_tile_source_get_grid (source);
      priv->projection = hyscan_map_tile_source_get_projection (source);
      priv->hash = hyscan_map_tile_source_hash (source);
    }
  else
    {
      guint min_zoom, max_zoom;
      guint blend_hash, source_hash;
      HyScanGeoProjection *source_projection;
      HyScanMapTileGrid *grid;
      gchar *hash_str;

      /* Проверяем, что добавляемый источник совместим с уже добавленными. */
      source_projection = hyscan_map_tile_source_get_projection (source);
      source_hash = hyscan_geo_projection_hash (source_projection);
      blend_hash = hyscan_geo_projection_hash (priv->projection);
      g_object_unref (source_projection);
      if (blend_hash != source_hash)
        {
          g_warning ("HyScanMapTileSourceBlend: source projection is not compatible with current one");
          return FALSE;
        }

      hash_str = g_strdup_printf ("%u:%u", priv->hash, hyscan_map_tile_source_hash (source));
      priv->hash = g_str_hash (hash_str);
      g_free (hash_str);

      grid = hyscan_map_tile_source_get_grid (source);

      /* Расширяем диапазон доступных масштабов с учётом нового источника. */
      hyscan_map_tile_grid_get_zoom_range (grid, &min_zoom, &max_zoom);
      priv->min_zoom = MIN (min_zoom, priv->min_zoom);
      priv->max_zoom = MIN (max_zoom, priv->max_zoom);
      g_object_unref (grid);
    }

  priv->sources = g_list_append (priv->sources, g_object_ref (source));

  return TRUE;
}
