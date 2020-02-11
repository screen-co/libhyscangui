/* hyscan-gtk-param.c
 *
 * Copyright 2018 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanGui.
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
 * SECTION: hyscan-gtk-param
 * @Short_description: базовый класс для отображения параметров.
 * @Title: HyScanGtkParam
 *
 * HyScanParam можно представить различными способами. При этом не изменится
 * способ отображения каждого из ключей и необходимость опрашивать интерфейс
 * на предмет изменений. Эта логика реализована в данном классе.
 *
 * Он выполняет следующие функции:
 * - Получение схемы данных (#hyscan_gtk_param_get_schema,
     #hyscan_gtk_param_get_nodes, см. ниже)
 * - Установка активного списка параметров для постоянного отслеживания
 *   (#hyscan_gtk_param_set_watch_list)
 * - Сохранение или отмена изменений (#hyscan_gtk_param_apply и
 *   #hyscan_gtk_param_discard соответственно)
 * - Получение всех виджетов ключей для последующего размещения на виджете
 *   (#hyscan_gtk_param_get_widgets)
 * - Автоматическое обновление значений в виджетах ключей.
 *
 * Отличие функций #hyscan_gtk_param_get_schema и #hyscan_gtk_param_get_nodes
 * состоит в том, что #hyscan_gtk_param_get_nodes возвращает уже отфильтрованный
 * список, то есть все узлы и ключи, которые ниже корневого.
 *
 * Данный класс является базовым для классов, реально отображающих ключи.
 * Сам по себе он ничего не умеет отображать, потоэму специального метода для
 * его создания нет. Для удобства класс унаследован от #GtkGrid.
 */
#include "hyscan-gtk-param.h"
#include <hyscan-gtk-param-key.h>

enum
{
  PROP_0,
  PROP_PARAM,
  PROP_ROOT,
  PROP_HIDDEN,
};

struct _HyScanGtkParamPrivate
{
  HyScanParam      *param;   /* Отображаемый HyScanParam. */
  gchar            *root;    /* Корень схемы для частичного отображения. */
  gboolean          show_hidden;  /* Отображать ли скрытые ключи. */

  HyScanDataSchema *schema;  /* Схема данных. */
  const HyScanDataSchemaNode *root_node; /* Узлы фильтрованные. */

  HyScanParamList  *previous;/* Предыдущие знач-ия отслеживаемых параметров. */
  HyScanParamList  *watch;   /* Список отслеживаемых параметров. */
  HyScanParamList  *apply;   /* Список измененных параметров. */
  HyScanParamList  *discard; /* Список предыдущих значений измененных пар-ов. */

  GHashTable       *widgets; /* Виджеты ключей. */

  guint             updater; /* Функция периодического опроса. */
  guint             period;  /* Период опроса. */
};

static void     hyscan_gtk_param_set_property            (GObject                     *object,
                                                          guint                        prop_id,
                                                          const GValue                *value,
                                                          GParamSpec                  *pspec);
static void     hyscan_gtk_param_get_property            (GObject                     *object,
                                                          guint                        prop_id,
                                                          GValue                      *value,
                                                          GParamSpec                  *pspec);
static void     hyscan_gtk_param_object_constructed      (GObject                     *object);
static void     hyscan_gtk_param_object_finalize         (GObject                     *object);
static gchar *  hyscan_gtk_param_validate_root           (const gchar                *root);

static gboolean hyscan_gtk_param_updater                 (gpointer                     data);
static void     hyscan_gtk_param_set_key                 (GVariant                    *value,
                                                          const gchar                 *path,
                                                          GHashTable                  *widgets);
static void     hyscan_gtk_param_set_all_keys            (HyScanParamList             *plist,
                                                          GHashTable                  *ht);
static void     hyscan_gtk_param_key_changed             (HyScanGtkParamKey           *pkey,
                                                          const gchar                 *path,
                                                          GVariant                    *val,
                                                          gpointer                    udata);
static void     hyscan_gtk_param_make_keys               (HyScanGtkParam              *self,
                                                          const HyScanDataSchemaNode  *node,
                                                          GHashTable                  *ht);
static void     hyscan_gtk_param_reinitialize            (HyScanGtkParam              *self);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (HyScanGtkParam, hyscan_gtk_param, GTK_TYPE_GRID);

static void
hyscan_gtk_param_class_init (HyScanGtkParamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_param_set_property;
  object_class->get_property = hyscan_gtk_param_get_property;

  object_class->constructed = hyscan_gtk_param_object_constructed;
  object_class->finalize = hyscan_gtk_param_object_finalize;

  g_object_class_install_property (object_class, PROP_PARAM,
    g_param_spec_object ("param", "ParamInterface", "HyScanParam Interface",
                         HYSCAN_TYPE_PARAM,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_ROOT,
    g_param_spec_string ("root", "RootNode", "Show only nodes starting with root",
                         "/",
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_HIDDEN,
    g_param_spec_boolean ("hidden", "ShowHidden", "Show hidden keys",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_param_init (HyScanGtkParam *self)
{
  self->priv = hyscan_gtk_param_get_instance_private (self);

  self->priv->previous = hyscan_param_list_new ();
  self->priv->watch = hyscan_param_list_new ();
  self->priv->apply = hyscan_param_list_new ();
  self->priv->discard = hyscan_param_list_new ();

  /* Хэш-таблица для виджетов и путей к ним. */
  self->priv->widgets = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, g_object_unref);
}

static void
hyscan_gtk_param_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HyScanGtkParam *self = HYSCAN_GTK_PARAM (object);
  HyScanGtkParamPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_PARAM:
      priv->param = g_value_dup_object (value);
      break;

    case PROP_ROOT:
      priv->root = hyscan_gtk_param_validate_root (g_value_get_string (value));
      break;

    case PROP_HIDDEN:
      priv->show_hidden = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hyscan_gtk_param_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  HyScanGtkParam *self = HYSCAN_GTK_PARAM (object);
  HyScanGtkParamPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_PARAM:
      g_value_set_object (value, priv->param);
      break;

    case PROP_HIDDEN:
      g_value_set_boolean (value, priv->show_hidden);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hyscan_gtk_param_object_constructed (GObject *object)
{
  HyScanGtkParam *self = HYSCAN_GTK_PARAM (object);

  hyscan_gtk_param_reinitialize (self);
}

static void
hyscan_gtk_param_object_finalize (GObject *object)
{
  HyScanGtkParam *self = HYSCAN_GTK_PARAM (object);
  HyScanGtkParamPrivate *priv= self->priv;

  g_clear_object (&priv->param);
  g_free (priv->root);

  g_clear_object (&priv->schema);

  g_clear_object (&priv->watch);
  g_clear_object (&priv->previous);
  g_clear_object (&priv->apply);
  g_clear_object (&priv->discard);

  g_hash_table_unref (priv->widgets);

  if (priv->updater != 0)
    g_source_remove (priv->updater);

  G_OBJECT_CLASS (hyscan_gtk_param_parent_class)->finalize (object);
}

static gchar *
hyscan_gtk_param_validate_root (const gchar *root)
{
  /* Корень это "/", но я позволяю использовать NULL и пустую строку. */
  if (root == NULL || g_str_equal (root, ""))
    return g_strdup ("/");
  else
    return g_strdup (root);
}

/* Функция обновляет содержимое текущего списка параметров. */
static gboolean
hyscan_gtk_param_updater (gpointer data)
{
  HyScanGtkParam *self = data;
  HyScanGtkParamPrivate *priv = self->priv;
  const gchar * const * paths;
  gboolean status;

  status = hyscan_param_get (priv->param, priv->watch);
  paths = hyscan_param_list_params (priv->watch);

  if (!status || paths == NULL)
    return G_SOURCE_CONTINUE;

  for (; *paths != NULL; ++paths)
    {
      GVariant *curr = hyscan_param_list_get (priv->watch, *paths);
      GVariant *prev = hyscan_param_list_get (priv->previous, *paths);

      /* Не могу представить такую ситуацию, но проверить не мешает. */
      if (curr == NULL)
        continue;

      /* Если значение отсутствовало и появилось,
       * либо присутствовало и поменялось, задаем его из бэкэнда. */
      if (prev == NULL || !g_variant_equal (prev, curr))
        {
          HyScanDataSchemaKeyAccess access;

          hyscan_gtk_param_set_key (curr, *paths, priv->widgets);

          /* Выясняем права доступа к параметру, чтобы не записать в список
           * измененных readonly-параметры. */
          access = hyscan_data_schema_key_get_access (priv->schema, *paths);
          if (access & HYSCAN_DATA_SCHEMA_ACCESS_WRITE)
            hyscan_param_list_set (priv->apply, *paths, curr);
        }

      g_clear_pointer (&curr, g_variant_unref);
      g_clear_pointer (&prev, g_variant_unref);
    }

  /* Чтобы не переписывать, просто меняем местами списки. */
  {
    HyScanParamList *temp = priv->previous;
    priv->previous = priv->watch;
    priv->watch = temp;
  }

  return G_SOURCE_CONTINUE;
}

/* Функция устанавливает значения виджетов. */
static void
hyscan_gtk_param_set_key (GVariant    *value,
                          const gchar *path,
                          GHashTable  *widgets)
{
  HyScanGtkParamKey *widget = g_hash_table_lookup (widgets, path);

  if (widget == NULL)
    {
      g_warning ("HyScanGtkParam: widget not found.");
      return;
    }

  hyscan_gtk_param_key_set (widget, value);
}

/* Функция устанавливает значения всех виджетов. */
static void
hyscan_gtk_param_set_all_keys (HyScanParamList *plist,
                               GHashTable      *widgets)
{
  const gchar * const * params;

  params = hyscan_param_list_params (plist);

  if (params == NULL)
    return;

  for (; *params != NULL; ++params)
    {
      GVariant *value = hyscan_param_list_get (plist, *params);

      if (value == NULL)
        continue;

      hyscan_gtk_param_set_key (value, *params, widgets);
      g_variant_unref (value);
    }
}

/* Функция, обрабатывающая пользовательский ввод в виджет. */
static void
hyscan_gtk_param_key_changed (HyScanGtkParamKey *pkey,
                              const gchar       *path,
                              GVariant          *val,
                              gpointer           udata)
{
  GVariant *old;
  HyScanGtkParam *self = udata;
  HyScanGtkParamPrivate *priv = self->priv;

  hyscan_param_list_set (priv->apply, path, val);

  old = hyscan_param_list_get (priv->watch, path);
  if (old == NULL)
    {
      g_warning ("HyScanGtkParam: Previous value not set.");
      return;
    }
  hyscan_param_list_set (priv->discard, path, old);
  g_variant_unref (old);
}

/* Функция рекурсивно создает виджеты, кладет их в хэш-таблицу и
 * подключает сигналы. */
static void
hyscan_gtk_param_make_keys (HyScanGtkParam             *self,
                            const HyScanDataSchemaNode *node,
                            GHashTable                 *ht)
{
  GList *link;
  GtkWidget *widget;
  HyScanDataSchemaKey *key;

  if (node == NULL)
    return;

  /* Рекурсивно идем по всем узлам. */
  for (link = node->nodes; link != NULL; link = link->next)
    {
      HyScanDataSchemaNode *subnode = link->data;
      hyscan_gtk_param_make_keys (self, subnode, ht);
    }

  /* А теперь по всем ключам. */
  for (link = node->keys; link != NULL; link = link->next)
    {
      key = link->data;

      widget = hyscan_gtk_param_key_new (self->priv->schema, key);

      g_signal_connect (widget, "changed",
                        G_CALLBACK (hyscan_gtk_param_key_changed), self);

      g_hash_table_insert (ht, g_strdup (key->id), g_object_ref (widget));
    }
}

static void
hyscan_gtk_param_reinitialize (HyScanGtkParam *self)
{
  HyScanGtkParamPrivate *priv = self->priv;
  HyScanGtkParamClass *klass = HYSCAN_GTK_PARAM_GET_CLASS (self);
  const HyScanDataSchemaNode *nodes;

  /* Очищаем все структуры и списки. */
  hyscan_param_list_clear (priv->previous);
  hyscan_param_list_clear (priv->watch);
  hyscan_param_list_clear (priv->apply);
  hyscan_param_list_clear (priv->discard);
  g_hash_table_remove_all (priv->widgets);
  g_clear_object (&priv->schema);
  priv->root_node = NULL;

  /* Получаем схему и создаем все виджеты ключей. */
  if (priv->param == NULL)
    return;

  priv->schema = hyscan_param_schema (priv->param);
  nodes = hyscan_data_schema_list_nodes (priv->schema);
  priv->root_node = hyscan_gtk_param_find_node (nodes, priv->root);

  /* Создаем виджеты. */
  hyscan_gtk_param_make_keys (self, priv->root_node, priv->widgets);
  klass->update (self);
}

void
hyscan_gtk_param_set_param (HyScanGtkParam  *self,
                            HyScanParam     *param,
                            const gchar     *root,
                            gboolean         hidden)
{
  HyScanGtkParamPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_PARAM (self));
  priv = self->priv;

  g_clear_object (&priv->param);
  g_clear_pointer (&priv->root, g_free);
  priv->show_hidden = FALSE;

  priv->param = g_object_ref (param);
  priv->root = hyscan_gtk_param_validate_root (root);
  priv->show_hidden = hidden;

  /* Инициируем обновление виджета. */
  hyscan_gtk_param_reinitialize (self);
}

/**
 * hyscan_gtk_param_get_schema:
 * @self: виджет #HyScanGtkParam
 *
 * Функция возвращает полную схему данных.
 * Из этой схемы данных можно получить узлы и ключи, но
 * в этом случае придется самостоятельно отфильровавать ключи.
 *
 * Returns: (transfer none): #HyScanDataSchema.
 */
HyScanDataSchema *
hyscan_gtk_param_get_schema (HyScanGtkParam *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_PARAM (self), NULL);

  return self->priv->schema;
}

/**
 * hyscan_gtk_param_get_nodes:
 * @self: виджет #HyScanGtkParam

 * Функция возвращает первый узел, соответствующий фильтру (root).
 *
 * Returns: (transfer none): #HyScanDataSchema.
 */
const HyScanDataSchemaNode *
hyscan_gtk_param_get_nodes (HyScanGtkParam *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_PARAM (self), NULL);

  return self->priv->root_node;
}

/**
 * hyscan_gtk_param_get_nodes:
 * @self: виджет #HyScanGtkParam

 * Функция возвращает флаг, определяющий, надо ли показывать скрытые ключи.
 *
 * Returns: %TRUE, если скрытые ключи показывать надо.
 */
gboolean
hyscan_gtk_param_get_show_hidden (HyScanGtkParam *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_PARAM (self), FALSE);

  return self->priv->show_hidden;
}

/**
 * hyscan_gtk_param_set_watch_list:
 * @self: виджет #HyScanGtkParam
 * @plist: #HyScanParamList с текущими путями

 * Функция задает список видимых в настоящее время виджетов.
 *
 * Фронтенд отображает виджеты, соответствующие определенному набору путей.
 * Для задания списка путей используется эта функция. Она должна быть вызвана
 * в момент смены отображаемых виджетов (например, переключение страницы)
 */
void
hyscan_gtk_param_set_watch_list (HyScanGtkParam  *self,
                                 HyScanParamList *plist)
{
  HyScanGtkParamPrivate *priv;
  const gchar * const * params;

  g_return_if_fail (HYSCAN_IS_GTK_PARAM (self));
  priv = self->priv;

  hyscan_param_list_clear (priv->watch);
  hyscan_param_list_clear (priv->previous);

  if (!HYSCAN_IS_PARAM_LIST (plist))
    return;

  params = hyscan_param_list_params (plist);

  if (params == NULL)
    return;

  for (; *params != NULL; ++params)
    {
      hyscan_param_list_add (priv->watch, *params);
      hyscan_param_list_add (priv->previous, *params);
    }

  hyscan_gtk_param_updater (self);
}

/**
 * hyscan_gtk_param_apply:
 * @self: виджет #HyScanGtkParam

 * Функция отправляет изменения в бэкэнд HyScanParam.
 * Такая реализация позволяет сделать кнопку "применить".
 * После вызова этой функции список изменений полностью очищается.
 */
void
hyscan_gtk_param_apply (HyScanGtkParam *self)
{
  HyScanGtkParamPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_PARAM (self));
  priv = self->priv;

  /* Отправляем изменения в бэкэнд. */
  hyscan_param_set (priv->param, priv->apply);
  hyscan_gtk_param_updater (self);

  /* Применять и отменять больше нечего. */
  hyscan_param_list_clear (priv->apply);
  hyscan_param_list_clear (priv->discard);
}

/**
 * hyscan_gtk_param_discard:
 * @self: виджет #HyScanGtkParam

 * Функция сбрасывает сделанные изменения.
 * Функцию имеет смысл вызывать только до вызова функции #hyscan_gtk_param_apply.
 * После вызова этой функции список изменений полностью очищается.
 */
void
hyscan_gtk_param_discard (HyScanGtkParam  *self)
{
  HyScanGtkParamPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_PARAM (self));
  priv = self->priv;

  /* Возвращаем старые значения в виджеты. */
  hyscan_gtk_param_set_all_keys (priv->discard, priv->widgets);

  /* Применять и отменять больше нечего. */
  hyscan_param_list_clear (priv->apply);
  hyscan_param_list_clear (priv->discard);
}

/**
 * hyscan_gtk_param_get_widgets:
 * @self: виджет #HyScanGtkParam

 * Функция возвращает хэш-таблицу, в которой ключом является путь к параметру,
 * а значением - соответствующий виджет. И ключи, и виджеты, и хэш-таблица
 * принадлежат классу и изменяться не должны.
 *
 * Returns: (transfer none): хэш-таблица с путями и виджетами.
 */
GHashTable *
hyscan_gtk_param_get_widgets (HyScanGtkParam *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_PARAM (self), NULL);

  return self->priv->widgets;
}

/**
 * hyscan_gtk_param_set_watch_period:
 * @self: виджет #HyScanGtkParam
 * @period: период опроса #HyScanParam в миллисекундах.

 * Функция задает период опроса HyScanParam.
 */
void
hyscan_gtk_param_set_watch_period (HyScanGtkParam *self,
                                   guint           period)
{
  HyScanGtkParamPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_PARAM (self));
  priv = self->priv;

  if (priv->updater != 0)
    {
      g_source_remove (priv->updater);
      priv->updater = 0;
    }

  priv->period = period;

  if (period == 0)
    return;

  priv->updater = g_timeout_add (period, hyscan_gtk_param_updater, self);
}

/**
 * hyscan_gtk_param_get_node_name:
 * @node: интересующй #HyScanDataSchemaNode.

 * Функция возвращает имя узла.
 * Если имя узла не задано, функция отрезает от пути все предыдущие узлы
 * и возвращает только название текущего.
 *
 * Returns: (transfer full): название узла.
 */
gchar *
hyscan_gtk_param_get_node_name (const HyScanDataSchemaNode *node)
{
  gchar **iter;
  gchar **sub;
  gchar *name;

  if (node->name != NULL)
    return g_strdup (node->name);

  sub = g_strsplit (node->path, "/", -1);

  /* g_strsplit возвращает нуль-терминированный список.
   * Если первый же элемент - NULL, значит, ловить тут нечего. */
  if (*sub == NULL)
    {
      g_strfreev (sub);
      return NULL;
    }

  /* Ищем последний элемент... */
  for (iter = sub; *iter != NULL; ++iter)
    ;

  /* А находим предпоследний! */
  --iter;

  name = g_strdup (*iter);
  g_strfreev (sub);

  return name;
}

const HyScanDataSchemaNode *
hyscan_gtk_param_find_node (const HyScanDataSchemaNode *node,
                            const gchar                *path)
{
  GList *link;
  const HyScanDataSchemaNode *found, *subnode;

  /* Либо это искомый узел... */
  if (g_str_equal (node->path, path))
    return node;

  /* Если узлов ниже нет, то и искать нечего. */
  if (node->nodes == NULL)
    return NULL;

  /* Либо рекурсивно идем по всем узлам. */
  for (link = node->nodes; link != NULL; link = link->next)
    {
      subnode = link->data;
      found = hyscan_gtk_param_find_node (subnode, path);
      if (found != NULL)
        return found;
    }

  return NULL;
}

/**
 * hyscan_gtk_param_get_node_name:
 * @node: интересующй #HyScanDataSchemaNode.
 * @show_hidden: надо ли показывать скрытые параметры.

 * Функция определяет, есть ли в заданном узле (без учета подузлов!)
 * видимые ключи.
 * Ключи видимы, если у них не стоит флаг %HYSCAN_DATA_SCHEMA_ACCESS_HIDDEN
 * или флаг стоит И `show_hidden = TRUE`
 *
 * Returns: есть ли видимые ключи в узле.
 */
gboolean
hyscan_gtk_param_node_has_visible_keys (const HyScanDataSchemaNode *node,
                                        gboolean                    show_hidden)
{
  GList *link;
  HyScanDataSchemaKey *key;

  g_return_val_if_fail (node != NULL, FALSE);

  for (link = node->keys; link != NULL; link = link->next)
    {
      key = link->data;

      /* Ключ видим, если он не спрятан или спрятан, но поднят флаг
       * show_hidden. */
      if (!(key->access & HYSCAN_DATA_SCHEMA_ACCESS_HIDDEN))
        return TRUE;
      if ((key->access & HYSCAN_DATA_SCHEMA_ACCESS_HIDDEN) && show_hidden)
        return TRUE;
    }

  return FALSE;
}

void
hyscan_gtk_param_clear_container (GtkContainer *container)
{
  GList *list, *link;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  list = gtk_container_get_children (container);
  for (link = list; link != NULL; link = g_list_next (link))
    gtk_widget_destroy (GTK_WIDGET (link->data));

  g_list_free (list);
}
