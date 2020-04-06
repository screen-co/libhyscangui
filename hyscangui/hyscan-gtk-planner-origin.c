/* hyscan-gtk-planner-origin.c
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
 * SECTION: hyscan-gtk-planner-origin
 * @Short_description: Виджет для редактирования начала координат планировщика
 * @Title: HyScanGtkPlannerEditor
 *
 * Виджет HyScanGtkPlannerOrigin содержит в себе текстовые поля для редактирования
 * начала координат планировщика:
 * - широты,
 * - долготы,
 * - направления оси OX.
 *
 * При измении значений в полях информация записывается в базу данных.
 *
 */

#include <glib/gi18n-lib.h>
#include "hyscan-gtk-planner-origin.h"

enum
{
  PROP_O,
  PROP_MODEL
};

struct _HyScanGtkPlannerOriginPrivate
{
  HyScanPlannerModel *model;      /* Модель объектов планировщика. */

  GtkWidget          *lat;        /* Поле для ввода широты. */
  GtkWidget          *lon;        /* Поле для ввода долготы. */
  GtkWidget          *azimuth;    /* Поле для ввода азимута. */
};

static void    hyscan_gtk_planner_origin_set_property             (GObject                    *object,
                                                                   guint                       prop_id,
                                                                   const GValue               *value,
                                                                   GParamSpec                 *pspec);
static void    hyscan_gtk_planner_origin_object_constructed       (GObject                    *object);
static void    hyscan_gtk_planner_origin_object_finalize          (GObject                    *object);
static void    hyscan_gtk_planner_origin_model_changed            (HyScanGtkPlannerOrigin     *gtk_origin);
static void    hyscan_gtk_planner_origin_val_changed              (GtkSpinButton              *spin_btn,
                                                                   HyScanGtkPlannerOrigin     *gtk_origin);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkPlannerOrigin, hyscan_gtk_planner_origin, GTK_TYPE_BOX)

static void
hyscan_gtk_planner_origin_class_init (HyScanGtkPlannerOriginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = hyscan_gtk_planner_origin_set_property;

  object_class->constructed = hyscan_gtk_planner_origin_object_constructed;
  object_class->finalize = hyscan_gtk_planner_origin_object_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/hyscan/gtk/hyscan-gtk-planner-origin.ui");
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerOrigin, lat);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerOrigin, lon);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerOrigin, azimuth);

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "HyScanPlannerModel", "HyScanPlannerModel with planner data",
                         HYSCAN_TYPE_PLANNER_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_planner_origin_init (HyScanGtkPlannerOrigin *gtk_planner_origin)
{
  gtk_planner_origin->priv = hyscan_gtk_planner_origin_get_instance_private (gtk_planner_origin);
  gtk_widget_init_template (GTK_WIDGET (gtk_planner_origin));
}

static void
hyscan_gtk_planner_origin_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  HyScanGtkPlannerOrigin *gtk_planner_origin = HYSCAN_GTK_PLANNER_ORIGIN (object);
  HyScanGtkPlannerOriginPrivate *priv = gtk_planner_origin->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->model = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_planner_origin_object_constructed (GObject *object)
{
  HyScanGtkPlannerOrigin *gtk_origin = HYSCAN_GTK_PLANNER_ORIGIN (object);
  HyScanGtkPlannerOriginPrivate *priv = gtk_origin->priv;

  G_OBJECT_CLASS (hyscan_gtk_planner_origin_parent_class)->constructed (object);

  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (priv->lat),     gtk_adjustment_new (0, -90,   90, 1e-8, 0, 0));
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (priv->lon),     gtk_adjustment_new (0, -180, 180, 1e-8, 0, 0));
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (priv->azimuth), gtk_adjustment_new (0,   0,  360, 1e-2, 0, 0));

  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (priv->lat), 8);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (priv->lon), 8);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (priv->azimuth), 2);

  g_signal_connect (priv->lat, "value-changed", G_CALLBACK (hyscan_gtk_planner_origin_val_changed), gtk_origin);
  g_signal_connect (priv->lon, "value-changed", G_CALLBACK (hyscan_gtk_planner_origin_val_changed), gtk_origin);
  g_signal_connect (priv->azimuth, "value-changed", G_CALLBACK (hyscan_gtk_planner_origin_val_changed), gtk_origin);
  g_signal_connect_swapped (priv->model, "changed", G_CALLBACK (hyscan_gtk_planner_origin_model_changed), gtk_origin);
}

static void
hyscan_gtk_planner_origin_object_finalize (GObject *object)
{
  HyScanGtkPlannerOrigin *gtk_origin = HYSCAN_GTK_PLANNER_ORIGIN (object);
  HyScanGtkPlannerOriginPrivate *priv = gtk_origin->priv;

  g_clear_object (&priv->model);

  G_OBJECT_CLASS (hyscan_gtk_planner_origin_parent_class)->finalize (object);
}

static void
hyscan_gtk_planner_origin_val_changed (GtkSpinButton          *spin_btn,
                                       HyScanGtkPlannerOrigin *gtk_origin)
{
  HyScanGtkPlannerOriginPrivate *priv = gtk_origin->priv;
  HyScanPlannerOrigin origin;

  origin.type = HYSCAN_PLANNER_ORIGIN;
  origin.origin.lat = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->lat));
  origin.origin.lon = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->lon));
  origin.ox = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->azimuth));

  hyscan_planner_model_set_origin (priv->model, &origin);
}

static void
hyscan_gtk_planner_origin_model_changed (HyScanGtkPlannerOrigin *gtk_origin)
{
  HyScanGtkPlannerOriginPrivate *priv = gtk_origin->priv;
  HyScanPlannerOrigin *origin;

  origin = hyscan_planner_model_get_origin (priv->model);
  gtk_widget_set_sensitive (GTK_WIDGET (gtk_origin), origin != NULL);

  if (origin == NULL)
    {
      gtk_entry_set_text (GTK_ENTRY (priv->lat), "");
      gtk_entry_set_text (GTK_ENTRY (priv->lon), "");
      gtk_entry_set_text (GTK_ENTRY (priv->azimuth), "");

      return;
    }

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->lat), origin->origin.lat);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->lon), origin->origin.lon);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->azimuth), origin->ox);

  hyscan_planner_origin_free (origin);
}

/**
 * hyscan_gtk_planner_origin_new:
 * @model: указатель на модель данных планировщика #HyScanPlannerModel
 *
 * Создаёт виджет редактирования начала координат
 *
 * Returns: указатель на виджет
 */
GtkWidget *
hyscan_gtk_planner_origin_new (HyScanPlannerModel *model)
{
  return g_object_new (HYSCAN_TYPE_GTK_PLANNER_ORIGIN,
                       "model", model,
                       NULL);
}
