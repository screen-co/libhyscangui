#include "hyscan-gtk-layer-param.h"

enum
{
  SIGNAL_SET,
  SIGNAL_LAST
};

struct _HyScanGtkLayerParamPrivate
{
  gboolean                     detach;
};

static void             hyscan_gtk_layer_param_interface_init       (HyScanParamInterface   *iface);
static gboolean         hyscan_gtk_layer_common_rgba_set            (const gchar            *name,
                                                                      GVariant              *value,
                                                                      GdkRGBA               *rgba);                                                                                           
static GVariant *       hyscan_gtk_layer_common_rgba_get            (const gchar            *name,
                                                                     GdkRGBA                *rgba);

static HyScanParamInterface *hyscan_param_parent_iface;
static guint                 hyscan_gtk_layer_param_signals[SIGNAL_LAST];

G_DEFINE_TYPE_WITH_CODE (HyScanGtkLayerParam, hyscan_gtk_layer_param, HYSCAN_TYPE_PARAM_CONTROLLER,
                         G_ADD_PRIVATE (HyScanGtkLayerParam)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM, hyscan_gtk_layer_param_interface_init))

static void
hyscan_gtk_layer_param_class_init (HyScanGtkLayerParamClass *klass)
{
  /**
   * HyScanGtkLayerParam::set:
   * @layer_param: указатель на #HyScanGtkLayerParam
   * @param_list: указатель на #HyScanParamList
   *
   * Сигнал посылается после успешного вызова hyscan_param_set().
   */
  hyscan_gtk_layer_param_signals[SIGNAL_SET] =
    g_signal_new ("set", HYSCAN_TYPE_GTK_LAYER_PARAM, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, HYSCAN_TYPE_PARAM);
}

static void
hyscan_gtk_layer_param_init (HyScanGtkLayerParam *gtk_layer_param)
{
  gtk_layer_param->priv = hyscan_gtk_layer_param_get_instance_private (gtk_layer_param);
}

static gboolean
hyscan_gtk_layer_param_set (HyScanParam     *param,
                            HyScanParamList *list)
{
  HyScanGtkLayerParam *layer_param = HYSCAN_GTK_LAYER_PARAM (param);
  HyScanGtkLayerParamPrivate *priv = layer_param->priv;

  if (priv->detach)
    return FALSE;

  if (!hyscan_param_parent_iface->set (param, list))
    return FALSE;
  
  g_signal_emit (param, hyscan_gtk_layer_param_signals[SIGNAL_SET], 0, NULL);
  return TRUE;
}

static gboolean
hyscan_gtk_layer_param_get (HyScanParam     *param,
                            HyScanParamList *list)
{
  HyScanGtkLayerParam *layer_param = HYSCAN_GTK_LAYER_PARAM (param);
  HyScanGtkLayerParamPrivate *priv = layer_param->priv;

  if (priv->detach)
    return FALSE;

  return hyscan_param_parent_iface->get (param, list);
}

static void
hyscan_gtk_layer_param_interface_init (HyScanParamInterface *iface)
{
  hyscan_param_parent_iface = g_type_interface_peek_parent (iface);
  iface->set = hyscan_gtk_layer_param_set;
  iface->get = hyscan_gtk_layer_param_get;
}

static gboolean
hyscan_gtk_layer_common_rgba_set (const gchar *name,
                                  GVariant    *value,
                                  GdkRGBA     *rgba)
{
  const gchar *spec;
  spec = g_variant_get_string (value, NULL);

  return spec != NULL && gdk_rgba_parse (rgba, spec);
}


static GVariant *
hyscan_gtk_layer_common_rgba_get (const gchar *name,
                                  GdkRGBA     *rgba)
{
  gchar *spec;
  spec = gdk_rgba_to_string (rgba);

  return g_variant_new_take_string (spec);
}

/* Разбивает ключ дата-схемы на группу и ключ ini-файла. */
static void
hyscan_gtk_layer_param_split_key (const gchar  *key,
                                  gchar       **group,
                                  gchar       **name)
{
  gchar **parts;

  parts = g_strsplit (key + 1, "/", 2);

  if (parts[0] == NULL)
    {
      *group = g_strdup ("_");
      *name = g_strdup ("_");
    }
  else if (parts[1] == NULL)
    {
      *group = g_strdup ("_");
      *name = parts[0];
    }
  else
    {
      *group = parts[0];
      *name = parts[1];
    }

  g_free (parts);
}

HyScanGtkLayerParam *
hyscan_gtk_layer_param_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_LAYER_PARAM, NULL);
}

void
hyscan_gtk_layer_param_set_stock_schema (HyScanGtkLayerParam *layer_param,
                                         const gchar         *schema_id)
{
  HyScanDataSchema *schema;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_PARAM (layer_param));

  schema = hyscan_data_schema_new_from_resource ("/org/hyscan/schemas/layer-schema.xml", schema_id);
  if (schema == NULL)
    return;

  hyscan_param_controller_set_schema (HYSCAN_PARAM_CONTROLLER (layer_param), schema);
  g_object_unref (schema);
}

gboolean
hyscan_gtk_layer_param_add_rgba (HyScanGtkLayerParam *layer_param,
                                 const gchar         *name,
                                 GdkRGBA             *rgba)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER_PARAM (layer_param), FALSE);
  
  return hyscan_param_controller_add_user (HYSCAN_PARAM_CONTROLLER (layer_param), name,
                                           (hyscan_param_controller_set) hyscan_gtk_layer_common_rgba_set,
                                           (hyscan_param_controller_get) hyscan_gtk_layer_common_rgba_get,
                                           rgba);
}

void
hyscan_gtk_layer_param_set_default (HyScanGtkLayerParam *param)
{
  HyScanDataSchema *schema;
  HyScanParamList *list;
  const gchar *const *keys;
  gint i;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_PARAM (param));
  
  schema = hyscan_param_schema (HYSCAN_PARAM (param));
  if (schema == NULL)
    return;

  keys = hyscan_data_schema_list_keys (schema);
  if (keys == NULL)
    return;

  list = hyscan_param_list_new ();
  for (i = 0; keys[i] != NULL; i++)
    hyscan_param_list_add (list, keys[i]);

  hyscan_param_set (HYSCAN_PARAM (param), list);
  g_object_unref (list);
}

void
hyscan_gtk_layer_param_detach (HyScanGtkLayerParam *param)
{
  g_return_if_fail (HYSCAN_IS_GTK_LAYER_PARAM (param));

  param->priv->detach = TRUE;
}

/**
 * hyscan_gtk_layer_param_file_to_list:
 * @key_file: указатель на файл #GKeyFile для чтения
 * @list: указатель на список #HyScanParamList для записи
 * @schema: схема параметров
 *
 * Считывает параметры из файла @key_file и записывает их в список параметров
 * @list согласно схеме @schema.
 *
 * todo: код взят из hyscan-db-param-file.c, надо сделать общедоступную функцию для чтения HyScanParamList из файла
 */
void
hyscan_gtk_layer_param_file_to_list (GKeyFile         *key_file,
                                     HyScanParamList  *list,
                                     HyScanDataSchema *schema)
{
  const gchar *const *names;
  gint i;

  names = hyscan_data_schema_list_keys (schema);
  if (names == NULL)
    return;

  /* Считываем значения параметров. */
  for (i = 0; names[i] != NULL; i++)
    {
      HyScanDataSchemaKeyType type;
      GVariant *value;
      gchar *group, *name;

      hyscan_gtk_layer_param_split_key (names[i], &group, &name);

      /* Если значение не установлено, возвращаем значение по умолчанию. */
      if (!g_key_file_has_key (key_file, group, name, NULL))
        {
          value = hyscan_data_schema_key_get_default (schema, names[i]);
          hyscan_param_list_set (list, names[i], value);
          g_clear_pointer (&value, g_variant_unref);
          g_free (group);
          g_free (name);
          continue;
        }

      type = hyscan_data_schema_key_get_value_type (schema, names[i]);
      switch (type)
        {
        case HYSCAN_DATA_SCHEMA_KEY_BOOLEAN:
          {
            gboolean bvalue = g_key_file_get_boolean (key_file, group, name, NULL);
            value = g_variant_new_boolean (bvalue);
          }
          break;

        case HYSCAN_DATA_SCHEMA_KEY_INTEGER:
          {
            gint64 ivalue = g_key_file_get_int64 (key_file, group, name, NULL);
            value = g_variant_new_int64 (ivalue);
          }
          break;

        case HYSCAN_DATA_SCHEMA_KEY_DOUBLE:
          {
            gdouble dvalue = g_key_file_get_double (key_file, group, name, NULL);
            value = g_variant_new_double (dvalue);
          }
          break;

        case HYSCAN_DATA_SCHEMA_KEY_STRING:
          {
            gchar *svalue = g_key_file_get_string (key_file, group, name, NULL);
            value = g_variant_new_take_string (svalue);
          }
          break;

        case HYSCAN_DATA_SCHEMA_KEY_ENUM:
          {
            const HyScanDataSchemaEnumValue *enum_value;
            const gchar *enum_id;
            gchar *svalue;

            svalue = g_key_file_get_string (key_file, group, name, NULL);
            enum_id = hyscan_data_schema_key_get_enum_id (schema, names[i]);
            enum_value = hyscan_data_schema_enum_find_by_id (schema, enum_id, svalue);
            g_free (svalue);

            value = enum_value != NULL ? g_variant_new_int64 (enum_value->value) : NULL;
          }
          break;

        default:
          value = NULL;
          break;
        }

      hyscan_param_list_set (list, names[i], value);
      g_free (group);
      g_free (name);
    }
}

/**
 * hyscan_gtk_layer_param_list_to_file:
 * @key_file: указатель на файл #GKeyFile для записи
 * @list: указатель на список #HyScanParamList для чтения
 * @schema: схема параметров
 *
 * Считывает параметры из списка параметров @list и записывает их в файл @key_file
 * согласно схеме @schema.
 *
 * todo: код взят из hyscan-db-param-file.c, надо сделать общедоступную функцию для записи HyScanParamList в файл
 */
void
hyscan_gtk_layer_param_list_to_file (GKeyFile         *key_file,
                                     HyScanParamList  *list,
                                     HyScanDataSchema *schema)
{
  gint i;
  const gchar *const *names;

  /* Список устанавливаемых параметров. */
  names = hyscan_param_list_params (list);
  if (names == NULL)
    return;

  /* Изменяем параметы. */
  for (i = 0; names[i] != NULL; i++)
    {
      GVariant *value;
      HyScanDataSchemaKeyType type;
      gchar *group, *name;

      value = hyscan_param_list_get (list, names[i]);
      type = hyscan_data_schema_key_get_value_type (schema, names[i]);
      hyscan_gtk_layer_param_split_key (names[i], &group, &name);

      /* Сбрасываем значение параметра до состояния по умолчанию. */
      if (value == NULL)
        {
          g_key_file_remove_key (key_file, group, name, NULL);
          g_free (group);
          g_free (name);
          continue;
        }

      switch (type)
        {
        case HYSCAN_DATA_SCHEMA_KEY_BOOLEAN:
          {
            g_key_file_set_boolean (key_file, group, name,
                                    g_variant_get_boolean (value));
          }
          break;

        case HYSCAN_DATA_SCHEMA_KEY_INTEGER:
          {
            g_key_file_set_int64 (key_file, group, name,
                                  g_variant_get_int64 (value));
          }
          break;

        case HYSCAN_DATA_SCHEMA_KEY_DOUBLE:
          {
            g_key_file_set_double (key_file, group, name,
                                   g_variant_get_double (value));
          }
          break;

        case HYSCAN_DATA_SCHEMA_KEY_STRING:
          {
            g_key_file_set_string (key_file, group, name,
                                   g_variant_get_string (value, NULL));
          }
          break;

        case HYSCAN_DATA_SCHEMA_KEY_ENUM:
          {
            const HyScanDataSchemaEnumValue *enum_value;
            const gchar *enum_id;

            enum_id = hyscan_data_schema_key_get_enum_id (schema, names[i]);
            enum_value = hyscan_data_schema_enum_find_by_value (schema, enum_id,
                                                                g_variant_get_int64 (value));

            g_key_file_set_string (key_file, group, names[i], enum_value != NULL ? enum_value->id : NULL);
          }
          break;

        default:
          break;
        }

      g_variant_unref (value);
      g_free (group);
      g_free (name);
    }
}
