/* hyscan-gtk-map-profile-switch.c
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * SECTION: hyscan-gtk-map-profile-switch
 * @Short_description: Виджет переключения профилей карты
 * @Title: HyScanGtkMap
 *
 * Виджет представляет из себя выпадающих список со списком профилей карты
 * и кнопку редактирования активного профиля.
 *
 * Для добавления профилей в список используйте функции:
 *
 * - hyscan_gtk_map_profile_switch_append() - добавляет профиль,
 * - hyscan_gtk_map_profile_switch_load_config() - добавляет профили из конфигурационных каталогов.
 *
 * Виджет позволяет включить режим работы профилей "оффлайн":
 *
 * - hyscan_gtk_map_profile_switch_set_offline() - установка режима,
 * - hyscan_gtk_map_profile_switch_get_offline() - получение текущего режима.
 *
 * Кнопка редактирования профиля открыает окно настройки параметров профиля.
 * После изменения профиль будет сохранён в директории с профилями пользователя.
 *
 */

#include "hyscan-gtk-map-profile-switch.h"
#include <string.h>
#include <hyscan-config.h>
#include <glib/gi18n-lib.h>
#include <hyscan-gtk-param-cc.h>

#define PROFILE_DIR          "map-profiles"
#define PROFILE_DEFAULT_ID   "default"
#define PROFILE_EXTENSION    ".ini"

enum
{
  PROP_O,
  PROP_MAP,
  PROP_BASE_ID,
  PROP_CACHE_DIR,
  PROP_OFFLINE,
};

struct _HyScanGtkMapProfileSwitchPrivate
{
  GHashTable    *profiles;        /* Хэш-таблица профилей. */
  HyScanGtkMap  *map;             /* Виджет карты. */
  gchar         *base_id;         /* Идентификатор слоя подложки.*/
  gchar         *cache_dir;       /* Путь к папке с кэшем карт. */
  gchar         *active_id;       /* Идентификатор активного профиля. */
  gboolean       offline;         /* Режим работы "оффлайн". */
  GtkWidget     *combo_box;       /* Выпдающий список профилей. */
  GtkWidget     *profile_param;   /* Виджет конфигурации профиля. */
};

static void    hyscan_gtk_map_profile_switch_set_property        (GObject                   *object,
                                                                  guint                      prop_id,
                                                                  const GValue              *value,
                                                                  GParamSpec                *pspec);
static void    hyscan_gtk_map_profile_switch_get_property        (GObject                   *object,
                                                                  guint                      prop_id,
                                                                  GValue                    *value,
                                                                  GParamSpec                *pspec);
static void    hyscan_gtk_map_profile_switch_object_constructed  (GObject                   *object);
static void    hyscan_gtk_map_profile_switch_object_finalize     (GObject                   *object);
static void    hyscan_gtk_map_profile_switch_changed             (HyScanGtkMapProfileSwitch *profile_switch);
static void    hyscan_gtk_map_profile_switch_clicked             (HyScanGtkMapProfileSwitch *profile_switch);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapProfileSwitch, hyscan_gtk_map_profile_switch, GTK_TYPE_BOX)

static void
hyscan_gtk_map_profile_switch_class_init (HyScanGtkMapProfileSwitchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_profile_switch_set_property;
  object_class->get_property = hyscan_gtk_map_profile_switch_get_property;

  object_class->constructed = hyscan_gtk_map_profile_switch_object_constructed;
  object_class->finalize = hyscan_gtk_map_profile_switch_object_finalize;

  g_object_class_install_property (object_class, PROP_MAP,
    g_param_spec_object ("map", "HyScanGtkMap", "HyScanGtkMap widget", HYSCAN_TYPE_GTK_MAP,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_BASE_ID,
    g_param_spec_string ("base-id", "Base Layer Id", "Identifier of HyScanGtkMapBase", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CACHE_DIR,
    g_param_spec_string ("cache-dir", "Cache Directory", "Map tiles cache directory", NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class, PROP_OFFLINE,
    g_param_spec_boolean ("offline", "Offline", "Work in offline mode", FALSE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
hyscan_gtk_map_profile_switch_init (HyScanGtkMapProfileSwitch *profile_switch)
{
  HyScanGtkMapProfileSwitchPrivate *priv;
  GtkWidget *edit_profile_btn;

  profile_switch->priv = hyscan_gtk_map_profile_switch_get_instance_private (profile_switch);
  priv = profile_switch->priv;

  priv->profiles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  /* Выпадающий список с профилями. */
  priv->combo_box = gtk_combo_box_text_new ();
  g_signal_connect_swapped (priv->combo_box , "changed", G_CALLBACK (hyscan_gtk_map_profile_switch_changed), profile_switch);

  edit_profile_btn = gtk_button_new_from_icon_name ("document-edit-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_halign (edit_profile_btn, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (edit_profile_btn, GTK_ALIGN_CENTER);
  g_signal_connect_swapped (edit_profile_btn, "clicked", G_CALLBACK (hyscan_gtk_map_profile_switch_clicked), profile_switch);

  gtk_box_pack_start (GTK_BOX (profile_switch), priv->combo_box, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (profile_switch), edit_profile_btn, FALSE, FALSE, 0);
}

static void
hyscan_gtk_map_profile_switch_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  HyScanGtkMapProfileSwitch *gtk_map_profile_switch = HYSCAN_GTK_MAP_PROFILE_SWITCH (object);
  HyScanGtkMapProfileSwitchPrivate *priv = gtk_map_profile_switch->priv;

  switch (prop_id)
    {
    case PROP_MAP:
      priv->map = g_value_dup_object (value);
      break;

    case PROP_BASE_ID:
      priv->base_id = g_value_dup_string (value);
      break;

    case PROP_CACHE_DIR:
      hyscan_gtk_map_profile_switch_set_cache_dir (gtk_map_profile_switch, g_value_get_string (value));
      break;

    case PROP_OFFLINE:
      hyscan_gtk_map_profile_switch_set_offline (gtk_map_profile_switch, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_profile_switch_get_property (GObject      *object,
                                            guint         prop_id,
                                            GValue       *value,
                                            GParamSpec   *pspec)
{
  HyScanGtkMapProfileSwitch *profile_switch = HYSCAN_GTK_MAP_PROFILE_SWITCH (object);
  HyScanGtkMapProfileSwitchPrivate *priv = profile_switch->priv;

  switch (prop_id)
    {
    case PROP_CACHE_DIR:
      g_value_set_string (value, priv->cache_dir);
      break;

    case PROP_OFFLINE:
      g_value_set_boolean (value, hyscan_gtk_map_profile_switch_get_offline (profile_switch));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_profile_switch_object_constructed (GObject *object)
{
  HyScanGtkMapProfileSwitch *profile_switch = HYSCAN_GTK_MAP_PROFILE_SWITCH (object);

  G_OBJECT_CLASS (hyscan_gtk_map_profile_switch_parent_class)->constructed (object);
}

static void
hyscan_gtk_map_profile_switch_object_finalize (GObject *object)
{
  HyScanGtkMapProfileSwitch *gtk_map_profile_switch = HYSCAN_GTK_MAP_PROFILE_SWITCH (object);
  HyScanGtkMapProfileSwitchPrivate *priv = gtk_map_profile_switch->priv;

  g_hash_table_destroy (priv->profiles);
  g_object_unref (priv->map);
  g_free (priv->base_id);
  g_free (priv->active_id);

  G_OBJECT_CLASS (hyscan_gtk_map_profile_switch_parent_class)->finalize (object);
}

/* Переключает активный профиль карты. */
static void
hyscan_gtk_map_profile_switch_changed (HyScanGtkMapProfileSwitch *profile_switch)
{
  HyScanGtkMapProfileSwitchPrivate *priv = profile_switch->priv;
  GtkComboBox *combo = GTK_COMBO_BOX (priv->combo_box);
  const gchar *active_id;

  active_id = gtk_combo_box_get_active_id (combo);

  if (active_id != NULL)
    hyscan_gtk_map_profile_switch_set_id (profile_switch, active_id);
}

static void
hyscan_gtk_map_profile_switch_param_clear (HyScanGtkMapProfileSwitch *profile_switch)
{
  HyScanGtkMapProfileSwitchPrivate *priv = profile_switch->priv;
  priv->profile_param = NULL;
  gtk_widget_set_sensitive (GTK_WIDGET (profile_switch), TRUE);
}

static void
hyscan_gtk_map_profile_switch_param_apply (HyScanGtkMapProfileSwitch *profile_switch)
{
  HyScanGtkMapProfileSwitchPrivate *priv = profile_switch->priv;
  HyScanParam *param;
  HyScanProfileMap *profile, *new_profile;
  gchar *file_name, *base_name;
  const gchar *user_dir;

  profile = g_hash_table_lookup (priv->profiles, priv->active_id);
  if (profile == NULL)
    return;

  hyscan_gtk_param_apply (HYSCAN_GTK_PARAM (priv->profile_param));

  user_dir = hyscan_config_get_user_files_dir ();
  if (user_dir == NULL)
    return;

  /* Создаём копию профиля в пользовательской директории. */
  base_name = g_strdup_printf ("%s%s", priv->active_id, PROFILE_EXTENSION);
  file_name = g_build_path (G_DIR_SEPARATOR_S, user_dir, PROFILE_DIR, base_name, NULL);
  new_profile = hyscan_profile_map_copy (profile, file_name);

  /* Записываем копию профиля на диск и считываем её обратно. */
  param = hyscan_gtk_layer_container_get_param (HYSCAN_GTK_LAYER_CONTAINER (priv->map));
  hyscan_profile_map_set_param (new_profile, param);
  hyscan_profile_write (HYSCAN_PROFILE (new_profile));
  if (hyscan_profile_read (HYSCAN_PROFILE (new_profile)))
    hyscan_gtk_map_profile_switch_append (profile_switch, priv->active_id, new_profile);

  g_object_unref (param);
  g_free (file_name);
  g_free (base_name);
}

static void
hyscan_gtk_map_profile_switch_clicked (HyScanGtkMapProfileSwitch *profile_switch)
{
  HyScanGtkMapProfileSwitchPrivate *priv = profile_switch->priv;
  GtkWidget *window, *box, *a_bar;
  GtkWidget *btn_apply, *btn_ok, *btn_cancel;
  HyScanProfileMap *profile;
  HyScanParam *map_param;
  gchar *title;

  if (priv->profile_param != NULL || priv->active_id == NULL)
    return;

  profile = g_hash_table_lookup (priv->profiles, priv->active_id);
  if (profile == NULL)
    return;

  map_param = hyscan_gtk_layer_container_get_param (HYSCAN_GTK_LAYER_CONTAINER (priv->map));

  title = g_strdup_printf (_("%s - Edit Profile"), hyscan_profile_get_name (HYSCAN_PROFILE (profile)));
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), title);
  gtk_window_set_transient_for (GTK_WINDOW (window),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (priv->map))));
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
  g_free (title);

  gtk_widget_set_sensitive (GTK_WIDGET (profile_switch), FALSE);

  g_signal_connect_swapped (window, "destroy", G_CALLBACK (hyscan_gtk_map_profile_switch_param_clear), profile_switch);

  priv->profile_param = hyscan_gtk_param_cc_new_full (map_param, "/", FALSE);
  btn_apply = gtk_button_new_with_mnemonic (_("_Apply"));
  btn_ok = gtk_button_new_with_mnemonic (_("_OK"));
  btn_cancel = gtk_button_new_with_mnemonic (_("_Cancel"));

  g_signal_connect_swapped (btn_apply,  "clicked", G_CALLBACK (hyscan_gtk_map_profile_switch_param_apply), profile_switch);
  g_signal_connect_swapped (btn_ok,     "clicked", G_CALLBACK (hyscan_gtk_map_profile_switch_param_apply), profile_switch);
  g_signal_connect_swapped (btn_ok,     "clicked", G_CALLBACK (gtk_widget_destroy), window);
  g_signal_connect_swapped (btn_cancel, "clicked", G_CALLBACK (gtk_widget_destroy), window);

  a_bar = gtk_action_bar_new ();
  gtk_action_bar_pack_end (GTK_ACTION_BAR (a_bar), btn_ok);
  gtk_action_bar_pack_end (GTK_ACTION_BAR (a_bar), btn_apply);
  gtk_action_bar_pack_end (GTK_ACTION_BAR (a_bar), btn_cancel);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (box), priv->profile_param, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), a_bar,               FALSE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show_all (window);

  g_object_unref (map_param);
}

static void
hyscan_gtk_map_profile_switch_update (HyScanGtkMapProfileSwitch *profile_switch)
{
  HyScanGtkMapProfileSwitchPrivate *priv = profile_switch->priv;
  GtkComboBoxText *combo = GTK_COMBO_BOX_TEXT (priv->combo_box);

  GHashTableIter iter;
  const gchar *key;
  HyScanProfileMap *profile;

  gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (combo));

  g_hash_table_iter_init (&iter, priv->profiles);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &profile))
    {
      const gchar *title;

      title = hyscan_profile_get_name (HYSCAN_PROFILE (profile));
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), key, title);
    }

  if (priv->active_id != NULL)
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), priv->active_id);
}

/* Получает NULL-терминированный массив файлов профилей. */
static gchar **
hyscan_gtk_map_profile_switch_list_files (const gchar *profiles_path)
{
  guint nprofiles = 0;
  gchar **profiles = NULL;
  GError *error = NULL;
  GDir *dir;

  if (!g_file_test (profiles_path, G_FILE_TEST_IS_DIR))
    goto exit;

  dir = g_dir_open (profiles_path, 0, &error);
  if (error == NULL)
    {
      const gchar *filename;

      while ((filename = g_dir_read_name (dir)) != NULL)
        {
          gchar *fullname;

          fullname = g_build_filename (profiles_path, filename, NULL);
          if (g_file_test (fullname, G_FILE_TEST_IS_REGULAR))
            {
              profiles = g_realloc (profiles, ++nprofiles * sizeof (gchar **));
              profiles[nprofiles - 1] = g_strdup (fullname);
            }
          g_free (fullname);
        }

      g_dir_close (dir);
    }
  else
    {
      g_warning ("HyScanGtkMapProfileSwitch: %s", error->message);
      g_error_free (error);
    }

exit:
  profiles = g_realloc (profiles, ++nprofiles * sizeof (gchar **));
  profiles[nprofiles - 1] = NULL;

  return profiles;
}

/* Добавляет дополнительные профили карты, загружая из папки profile_dir. */
static void
hyscan_gtk_map_profile_load_dir  (HyScanGtkMapProfileSwitch *profile_switch,
                                  const gchar               *profile_dir)
{
  HyScanGtkMapProfileSwitchPrivate *priv = profile_switch->priv;
  gchar **config_files;
  guint conf_i;

  config_files = hyscan_gtk_map_profile_switch_list_files (profile_dir);
  for (conf_i = 0; config_files[conf_i] != NULL; ++conf_i)
    {
      HyScanProfileMap *profile;
      gchar *profile_id;

      if (!g_str_has_suffix (config_files[conf_i], PROFILE_EXTENSION))
        continue;

      profile = hyscan_profile_map_new (priv->cache_dir, config_files[conf_i]);
      /* Убираем расширение из имени файла - это будет идентификатор профиля. */
      profile_id = g_path_get_basename (config_files[conf_i]);
      profile_id[strlen (profile_id) - strlen (PROFILE_EXTENSION)] = '\0';

      /* Читаем профиль и добавляем его в карту. */
      if (hyscan_profile_read (HYSCAN_PROFILE (profile)))
        hyscan_gtk_map_profile_switch_append (profile_switch, profile_id, profile);

      g_free (profile_id);
      g_object_unref (profile);
    }

  g_strfreev (config_files);
}

/**
 * hyscan_gtk_map_profile_switch_new:
 * @map: виджет карты #HyScanGtkMap
 * @base_id: идентификатор слоя подложки
 * @cache_dir: путь к каталогу с кэшем карт
 *
 * Функция создаёт виджет переключения профилей карты.
 *
 * Returns: указатель на #HyScanGtkMapProfileSwitch
 */
GtkWidget *
hyscan_gtk_map_profile_switch_new (HyScanGtkMap *map,
                                   const gchar  *base_id,
                                   const gchar  *cache_dir)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_PROFILE_SWITCH,
                       "map", map,
                       "base-id", base_id,
                       "cache-dir", cache_dir,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       "spacing", 6,
                       NULL);
}

/**
 * hyscan_gtk_map_profile_switch_load_config:
 * @profile_switch: указатель на #HyScanGtkMapProfileSwitch
 *
 * Функция загружает список профилей карты из каталогов для поиска профилей,
 * указанных в hyscan_config_get_profile_dirs().
 */
void
hyscan_gtk_map_profile_switch_load_config (HyScanGtkMapProfileSwitch *profile_switch)
{
  HyScanGtkMapProfileSwitchPrivate *priv;
  HyScanProfileMap *default_profile;
  const gchar **dirs;
  gint i;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_PROFILE_SWITCH (profile_switch));
  priv = profile_switch->priv;

  /* Добавляем профиль по умолчанию. */
  default_profile = hyscan_profile_map_new_default (priv->cache_dir);
  hyscan_gtk_map_profile_switch_append (profile_switch, PROFILE_DEFAULT_ID, default_profile);
  g_object_unref (default_profile);

  /* Загружаем профили из папок для поиска профилей. */
  dirs = hyscan_config_get_profile_dirs ();
  for (i = 0; dirs[i] != NULL; i++)
    {
      gchar *profile_dir;

      profile_dir = g_build_filename (dirs[i], PROFILE_DIR, NULL);
      hyscan_gtk_map_profile_load_dir (profile_switch, profile_dir);

      g_free (profile_dir);
    }

  hyscan_gtk_map_profile_switch_set_id (profile_switch, PROFILE_DEFAULT_ID);
}

/**
 * hyscan_gtk_map_profile_switch_append:
 * @profile_switch: указатель на #HyScanGtkMapProfileSwitch
 * @profile_id: идентификатор профиля
 * @profile: профиль карты #HyScanProfileMap
 *
 * Функция добавляет новый профиль в список профилей виджета. Если профиль
 * с указанным идентификатором уже существует, он будет заменён на новый.
 */
void
hyscan_gtk_map_profile_switch_append (HyScanGtkMapProfileSwitch *profile_switch,
                                      const gchar               *profile_id,
                                      HyScanProfileMap          *profile)
{
  HyScanGtkMapProfileSwitchPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_PROFILE_SWITCH (profile_switch));
  priv = profile_switch->priv;

  g_hash_table_insert (priv->profiles, g_strdup (profile_id), g_object_ref (profile));
  hyscan_gtk_map_profile_switch_update (profile_switch);
}

/**
 * hyscan_gtk_map_profile_switch_get_id:
 * @profile_switch: указатель на #HyScanGtkMapProfileSwitch
 *
 * Функция возвращает идентификатор активного профиля карты.
 *
 * Returns: (transfer full): идентификатор активного профиля карты, для удаления g_object_unref().
 */
gchar *
hyscan_gtk_map_profile_switch_get_id (HyScanGtkMapProfileSwitch *profile_switch)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_PROFILE_SWITCH (profile_switch), NULL);

  return g_strdup (profile_switch->priv->active_id);
}

/**
 * hyscan_gtk_map_profile_switch_set_id:
 * @profile_switch: указатель на #HyScanGtkMapProfileSwitch
 * @profile_id: (nullable): идентификатор профиля карты или %NULL для применения текущего профиля
 *
 * Функция применяет профиль карты @profile_id.
 *
 */
void
hyscan_gtk_map_profile_switch_set_id (HyScanGtkMapProfileSwitch *profile_switch,
                                      const gchar               *profile_id)
{
  HyScanGtkMapProfileSwitchPrivate *priv;
  HyScanProfileMap *profile;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_PROFILE_SWITCH (profile_switch));

  priv = profile_switch->priv;

  if (profile_id == NULL)
    profile_id = priv->active_id;

  profile = g_hash_table_lookup (priv->profiles, profile_id);
  if (profile == NULL)
    return;

  if (g_strcmp0 (profile_id, priv->active_id) != 0)
    {
      g_free (priv->active_id);
      priv->active_id = g_strdup (profile_id);
    }
  hyscan_profile_map_set_offline (profile, priv->offline);
  hyscan_profile_map_apply (profile, priv->map, priv->base_id);

  g_signal_handlers_block_by_func (priv->combo_box, hyscan_gtk_map_profile_switch_changed, profile_switch);
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (priv->combo_box), priv->active_id);
  g_signal_handlers_unblock_by_func (priv->combo_box, hyscan_gtk_map_profile_switch_changed, profile_switch);
}

/**
 * hyscan_gtk_map_profile_switch_set_offline:
 * @profile_switch: указатель на #HyScanGtkMapProfileSwitch
 *
 * Функция возвращает режим оффлайн-работы.
 *
 * Returns: %TRUE, если включен оффлайн-режим.
 */
gboolean
hyscan_gtk_map_profile_switch_get_offline (HyScanGtkMapProfileSwitch *profile_switch)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_PROFILE_SWITCH (profile_switch), FALSE);

  return profile_switch->priv->offline;
}

/**
 * hyscan_gtk_map_profile_switch_set_offline:
 * @profile_switch: указатель на #HyScanGtkMapProfileSwitch
 * @offline: признак работы в оффлайн-режиме
 *
 * Функция устанавливает режим оффлайн-работы.
 */
void
hyscan_gtk_map_profile_switch_set_offline (HyScanGtkMapProfileSwitch *profile_switch,
                                           gboolean                   offline)
{
  HyScanGtkMapProfileSwitchPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_PROFILE_SWITCH (profile_switch));
  priv = profile_switch->priv;

  profile_switch->priv->offline = offline;
  if (priv->active_id != NULL)
    hyscan_gtk_map_profile_switch_set_id (profile_switch, priv->active_id);

  g_object_notify (G_OBJECT (profile_switch), "offline");
}

/**
 * hyscan_gtk_map_profile_switch_set_cache_dir:
 * @profile_switch: указатель на #HyScanGtkMapProfileSwitch
 * @cache_dir: путь к папке кэша карт
 *
 * Функция устанавливает путь к папке кэша карт.
 *
 */
void
hyscan_gtk_map_profile_switch_set_cache_dir (HyScanGtkMapProfileSwitch *profile_switch,
                                             const gchar               *cache_dir)
{
  HyScanGtkMapProfileSwitchPrivate *priv;
  HyScanProfileMap *profile;
  GHashTableIter iter;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_PROFILE_SWITCH (profile_switch));
  priv = profile_switch->priv;
  
  g_free (priv->cache_dir);
  priv->cache_dir = g_strdup (cache_dir);

  g_hash_table_iter_init (&iter, priv->profiles);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &profile))
    hyscan_profile_map_set_cache_dir (profile, priv->cache_dir);

  /* Инициируем повторное применение профиля, чтобы обновить настройки кэша. */
  if (priv->active_id != NULL)
    hyscan_gtk_map_profile_switch_set_id (profile_switch, priv->active_id);

  g_object_notify (G_OBJECT (profile_switch), "cache-dir");
}
