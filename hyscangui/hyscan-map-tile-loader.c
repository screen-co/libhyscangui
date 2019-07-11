/* hyscan-map-tile-loader.c
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
 * SECTION: hyscan-map-tile-loader
 * @Short_description: Класс для загрузки тайлов
 * @Title: HyScanMapTileLoader
 *
 * Класс выполняет загрузку всех тайлов определнной области карты из указанного
 * источника по всем доступным масштабам.
 *
 * Хотя класс может выполнять загрузку тайлов из любого источника #HyScanMapTileSource,
 * его основное практическое применение - это сохранение тайлов из сети на жесткий
 * диск, чтобы пользователь мог работать с картами офф-лайн. В этом случае в
 * качестве источника тайлов следует использовать #HyScanMapTileSourceFile.
 *
 * Методы класса:
 * - hyscan_map_tile_loader_new() - создает новый объект класса,
 * - hyscan_map_tile_loader_start() - запускает загрузку тайлов,
 * - hyscan_map_tile_loader_stop() - останавливает загрузку тайлов.
 *
 * Для отслеживания прогресса загрузки следует использовать сигналы
 * #HyScanMapTileLoader::progress и #HyScanMapTileLoader::done.
 *
 */

#include "hyscan-map-tile-loader.h"

enum
{
  SIGNAL_PROGRESS,
  SIGNAL_DONE,
  SIGNAL_LAST,
};

struct _HyScanMapTileLoaderPrivate
{
  gboolean                     busy;       /* Признак того, что идёт загрузка. */
  gboolean                     stop;       /* Флаг, что надо остановить загрузку. */

  HyScanMapTileSource         *source;     /* Указатель на источник тайлов. */
  gdouble                      from_x;     /* Координата x начала области загрузки. */
  gdouble                      to_x;       /* Координата x конца области загрузки. */
  gdouble                      from_y;     /* Координата y начала области загрузки. */
  gdouble                      to_y;       /* Координата y конца области загрузки. */
};

static guint hyscan_map_tile_loader_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMapTileLoader, hyscan_map_tile_loader, G_TYPE_OBJECT)

static void
hyscan_map_tile_loader_class_init (HyScanMapTileLoaderClass *klass)
{

  /**
   * HyScanMapTileLoader::progress:
   * @loader: объект #HyScanMapTileLoader, получивший сигнал
   * @fraction: текущий прогресс от 0.0 до 1.0
   *
   * Сигнал отправляется после загрузки каждого тайла и сообщает об общем
   * прогрессе загрузки.
   */
  hyscan_map_tile_loader_signals[SIGNAL_PROGRESS] =
    g_signal_new ("progress", HYSCAN_TYPE_MAP_TILE_LOADER,
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__DOUBLE,
                  G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  /**
   * HyScanMapTileLoader::done:
   * @loader: объект #HyScanMapTileLoader, получивший сигнал
   * @failed: количество тайлов, которые не удалось загрузить
   *
   * Сигнал отправляется при завершении загрузки тайлов. Переменная @failed
   * содержит в себе количество тайлов, который источник не смог заполнить.
   */
  hyscan_map_tile_loader_signals[SIGNAL_DONE] =
    g_signal_new ("done", HYSCAN_TYPE_MAP_TILE_LOADER,
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void
hyscan_map_tile_loader_init (HyScanMapTileLoader *map_tile_loader)
{
  map_tile_loader->priv = hyscan_map_tile_loader_get_instance_private (map_tile_loader);
}

static gpointer
hyscan_map_tile_loader_func (gpointer data)
{
  HyScanMapTileLoader *loader =  HYSCAN_MAP_TILE_LOADER (data);
  HyScanMapTileLoaderPrivate *priv = loader->priv;

  HyScanMapTileGrid *grid;
  guint min_zoom, max_zoom, zoom;

  guint total = 0, processed = 0, failed = 0;

  grid = hyscan_map_tile_source_get_grid (priv->source);

  hyscan_map_tile_grid_get_zoom_range (grid, &min_zoom, &max_zoom);

  /* Определяем объем работы. */
  for (zoom = min_zoom; zoom <= max_zoom; zoom++)
    {
      gint x0, y0, xn, yn;

      hyscan_map_tile_grid_get_view (grid, zoom, priv->from_x, priv->to_x, priv->from_y, priv->to_y,
                                         &x0, &xn, &y0, &yn);

      total += (xn - x0 + 1) * (yn - y0 + 1);
    }

  /* Поочерёдно загружаем тайлы. */
  for (zoom = min_zoom; zoom <= max_zoom; zoom++)
    {
      gint x0, y0, xn, yn;
      gint x, y;

      hyscan_map_tile_grid_get_view (grid, zoom, priv->from_x, priv->to_x, priv->from_y, priv->to_y,
                                         &x0, &xn, &y0, &yn);

      for (x = x0; x <= xn; ++x)
        for (y = y0; y <= yn; ++y)
          {
            HyScanMapTile *tile;

            if (g_atomic_int_get (&priv->stop))
              break;

            tile = hyscan_map_tile_new (grid, x, y, zoom);
            if (!hyscan_map_tile_source_fill (priv->source, tile, NULL))
              {
                ++failed;
                g_warning ("HyScanMapTileLoader: failed to load tile %d/%d/%d", zoom, x, y);
              }

            ++processed;
            g_signal_emit (loader, hyscan_map_tile_loader_signals[SIGNAL_PROGRESS], 0,
                           (gdouble) processed / total);

            g_object_unref (tile);
          }
    }

  /* Всё необработанное считаем неудавшимся. */
  failed += total - processed;

  g_object_unref (grid);
  g_clear_object (&priv->source);

  g_atomic_int_set (&priv->busy, FALSE);

  g_signal_emit (loader, hyscan_map_tile_loader_signals[SIGNAL_DONE], 0, failed);

  return GINT_TO_POINTER (TRUE);
}

/**
 * hyscan_map_tile_loader_new:
 *
 * Создаёт новый объект #HyScanMapTileLoader.
 *
 * Returns: указатель на новый объект #HyScanMapTileLoader, для удаления g_object_unref().
 */
HyScanMapTileLoader *
hyscan_map_tile_loader_new (void)
{
  return g_object_new (HYSCAN_TYPE_MAP_TILE_LOADER, NULL);
}

/**
 * hyscan_map_tile_loader_start:
 * @loader: указатель на #HyScanMapTileLoader
 * @source: источник тайлов
 * @from_x: минимальная граница области для кэширования по оси X
 * @to_x: максимальная граница области для кэширования по оси X
 * @from_y: минимальная граница области для кэширования по оси Y
 * @to_y: максимальная граница области для кэширования по оси Y
 *
 * Запускает загрузку тайлов из источника @source, которые полностью покрывают
 * указанную область.
 *
 * Returns: указатель на поток, в котором будет происходить загрузка тайлов, или
 *          %NULL в случае ошибки. Для удаления g_thread_unref() или g_thread_join().
 */
GThread *
hyscan_map_tile_loader_start (HyScanMapTileLoader *loader,
                              HyScanMapTileSource *source,
                              gdouble              from_x,
                              gdouble              to_x,
                              gdouble              from_y,
                              gdouble              to_y)
{
  HyScanMapTileLoaderPrivate *priv;
  GThread *thread;

  g_return_val_if_fail (HYSCAN_IS_MAP_TILE_LOADER (loader), NULL);

  priv = loader->priv;

  if (!g_atomic_int_compare_and_exchange (&priv->busy, FALSE, TRUE))
    return NULL;

  g_atomic_int_set (&priv->stop, FALSE);

  priv->source = g_object_ref (source);
  priv->from_x = from_x;
  priv->to_x = to_x;
  priv->from_y = from_y;
  priv->to_y = to_y;

  thread = g_thread_new ("tile-loader", hyscan_map_tile_loader_func, g_object_ref (loader));

  return thread;
}

/**
 * hyscan_map_tile_loader_stop:
 * @loader: указатель на #HyScanMapTileLoader
 *
 * Останавливает загрузку тайлов.
 */
void
hyscan_map_tile_loader_stop (HyScanMapTileLoader *loader)
{
  HyScanMapTileLoaderPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MAP_TILE_LOADER (loader));
  priv = loader->priv;

  g_atomic_int_set (&priv->stop, TRUE);
}
