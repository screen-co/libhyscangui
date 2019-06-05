/* hyscan-track-list-model.с
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
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

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-track-list-model
 * @Short_description: Модель активных элементов (не обязательно галсов)
 * @Title: HyScanTrackListModel
 *
 * Модель хранит список идентификаторов активных элементов и оповещает своих подписчиков
 * о любых изменения в этом списке.
 *
 * - hyscan_track_list_model_new() - создание модели
 * - hyscan_track_list_model_get() - получение таблицы активных элементов
 * - hyscan_track_list_model_set_active() - установка статуса элемента
 * - hyscan_track_list_model_get_active() - получение статуса элемента
 *
 * Для подписки на события изменения используется сигнал #HyScanTrackListModel::changed
 *
 */

#include "hyscan-track-list-model.h"

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST
};

struct _HyScanTrackListModelPrivate
{
  GHashTable *table;
};

static void    hyscan_track_list_model_object_constructed       (GObject               *object);
static void    hyscan_track_list_model_object_finalize          (GObject               *object);

static guint       hyscan_track_list_model_signals[SIGNAL_LAST] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (HyScanTrackListModel, hyscan_track_list_model, G_TYPE_OBJECT)

static void
hyscan_track_list_model_class_init (HyScanTrackListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_track_list_model_object_constructed;
  object_class->finalize = hyscan_track_list_model_object_finalize;

  /**
   * HyScanTrackListModel::changed:
   * @track_list_model: указатель на #HyScanTrackListModel
   *
   * Сигнал посылается при изменении списка активных элементов.
   */
  hyscan_track_list_model_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_TRACK_LIST_MODEL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_track_list_model_init (HyScanTrackListModel *track_list_model)
{
  track_list_model->priv = hyscan_track_list_model_get_instance_private (track_list_model);
}

static void
hyscan_track_list_model_object_constructed (GObject *object)
{
  HyScanTrackListModel *track_list_model = HYSCAN_TRACK_LIST_MODEL (object);
  HyScanTrackListModelPrivate *priv = track_list_model->priv;

  G_OBJECT_CLASS (hyscan_track_list_model_parent_class)->constructed (object);

  priv->table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
hyscan_track_list_model_object_finalize (GObject *object)
{
  HyScanTrackListModel *track_list_model = HYSCAN_TRACK_LIST_MODEL (object);
  HyScanTrackListModelPrivate *priv = track_list_model->priv;

  g_hash_table_unref (priv->table);

  G_OBJECT_CLASS (hyscan_track_list_model_parent_class)->finalize (object);
}

/**
 * hyscan_track_list_model_new:
 *
 * Returns: новый объект #HyScanTrackListModel
 */
HyScanTrackListModel *
hyscan_track_list_model_new (void)
{
  return g_object_new (HYSCAN_TYPE_TRACK_LIST_MODEL, NULL);
}

/**
 * hyscan_track_list_model_get:
 * @tlist_model: указатель на #HyScanTrackListModel
 *
 * Returns: (element-type boolean): хэш-таблица, где ключ - название элемента,
 *   значение - признак того, что элемент активен.
 */
GHashTable *
hyscan_track_list_model_get (HyScanTrackListModel *tlist_model)
{
  g_return_val_if_fail (HYSCAN_IS_TRACK_LIST_MODEL (tlist_model), NULL);

  return g_hash_table_ref (tlist_model->priv->table);
}

/**
 * hyscan_track_list_model_set_active:
 * @tlist_model: указатель на #HyScanTrackListModel
 * @track_name: имя элемента
 * @active: признак активности элемента
 *
 * Устанавливает элемент с именем @track_name активным или неактивным.
 */
void
hyscan_track_list_model_set_active (HyScanTrackListModel *tlist_model,
                                    const gchar          *track_name,
                                    gboolean              active)
{
  HyScanTrackListModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TRACK_LIST_MODEL (tlist_model));
  priv = tlist_model->priv;

  g_hash_table_insert (priv->table, g_strdup (track_name), GINT_TO_POINTER (active));

  g_signal_emit (tlist_model, hyscan_track_list_model_signals[SIGNAL_CHANGED], 0);
}

/**
 * hyscan_track_list_model_get_active:
 * @tlist_model: указатель на #HyScanTrackListModel
 * @track_name: имя элемента
 *
 * Returns: возвращает %TRUE, если @track_name активен; иначе %FALSE.
 */
gboolean
hyscan_track_list_model_get_active (HyScanTrackListModel *tlist_model,
                                    const gchar          *track_name)
{
  HyScanTrackListModelPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_TRACK_LIST_MODEL (tlist_model), FALSE);
  priv = tlist_model->priv;

  return GPOINTER_TO_INT (g_hash_table_lookup (priv->table, track_name));
}
