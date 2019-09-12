/* hyscan-gtk-waterfall-meter.c
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

/**
 * SECTION: hyscan-gtk-waterfall-meter
 * @Title HyScanGtkWaterfallMeter
 * @Short_description Слой линеек водопада
 *
 * Данный слой позволяет производить простые линейные измерения на водопаде.
 * Он поддерживает бесконечное число линеек, каждая из которых состоит из
 * двух точек.
 */
#include "hyscan-gtk-waterfall-meter.h"
#include "hyscan-gtk-waterfall-tools.h"
#include <math.h>

typedef struct
{
  guint64            id;
  HyScanCoordinates  start;
  HyScanCoordinates  end;
} HyScanGtkWaterfallMeterItem;

struct _HyScanGtkWaterfallMeterPrivate
{
  HyScanGtkWaterfall           *wfall;
  PangoLayout                  *font;              /* Раскладка шрифта. */

  gboolean                      layer_visibility;

  HyScanCoordinates             press;
  HyScanCoordinates             pointer;

  guint64                       last_id;           /* Идентификатор л*/
  GList                        *cancellable;       /* Для отмены. */
  GList                        *all;               /* Все линейки вообще. */
  GList                        *visible;           /* Видимые прямо сейчас. */

  HyScanCoordinates             handle_to_highlight; /* Хендл для подсвечивания. */
  gboolean                      highlight;           /* Флаг подсвечивания. */
  HyScanGtkWaterfallMeterItem   current;             /* Редактируемая прямо сейчас линейка. */
  gboolean                      editing;             /* Режим ред-я включен. */

  struct
  {
    HyScanCoordinates _0;
    HyScanCoordinates _1;
  } view;

  struct
  {
    GdkRGBA                     meter;
    GdkRGBA                     shadow;
    GdkRGBA                     frame;
    gdouble                     meter_width;
    gdouble                     shadow_width;
    gboolean                    draw_shadow;
  } color;

};

static void     hyscan_gtk_waterfall_meter_interface_init          (HyScanGtkLayerInterface *iface);
static void     hyscan_gtk_waterfall_meter_object_constructed      (GObject                 *object);
static void     hyscan_gtk_waterfall_meter_object_finalize         (GObject                 *object);

static gint     hyscan_gtk_waterfall_meter_find_by_id              (gconstpointer            a,
                                                                    gconstpointer            b);

static GList*   hyscan_gtk_waterfall_meter_find_closest            (HyScanGtkWaterfallMeter *self,
                                                                    HyScanCoordinates       *pointer);
static gboolean hyscan_gtk_waterfall_meter_handle_create           (HyScanGtkLayer          *layer,
                                                                    GdkEventButton          *event);
static gboolean hyscan_gtk_waterfall_meter_handle_release          (HyScanGtkLayer          *layer,
                                                                    gconstpointer            howner);
static gboolean hyscan_gtk_waterfall_meter_handle_find             (HyScanGtkLayer          *layer,
                                                                    gdouble                  x,
                                                                    gdouble                  y,
                                                                    HyScanGtkLayerHandle    *handle);
static void     hyscan_gtk_waterfall_meter_handle_show             (HyScanGtkLayer          *layer,
                                                                    HyScanGtkLayerHandle    *handle);
static gconstpointer hyscan_gtk_waterfall_meter_handle_grab        (HyScanGtkLayer          *layer,
                                                                    HyScanGtkLayerHandle    *handle);

static gboolean hyscan_gtk_waterfall_meter_key                     (GtkWidget               *widget,
                                                                    GdkEventKey             *event,
                                                                    HyScanGtkWaterfallMeter *self);
static gboolean hyscan_gtk_waterfall_meter_button                  (GtkWidget               *widget,
                                                                    GdkEventButton          *event,
                                                                    HyScanGtkWaterfallMeter *self);

static gboolean hyscan_gtk_waterfall_meter_motion                  (GtkWidget               *widget,
                                                                    GdkEventMotion          *event,
                                                                    HyScanGtkWaterfallMeter *self);
static void     hyscan_gtk_waterfall_meter_draw_helper             (cairo_t                 *cairo,
                                                                    HyScanCoordinates       *start,
                                                                    HyScanCoordinates       *end,
                                                                    GdkRGBA                 *color,
                                                                    gdouble                  width);
static gboolean hyscan_gtk_waterfall_meter_draw_task               (HyScanGtkWaterfallMeter     *self,
                                                                    cairo_t                     *cairo,
                                                                    HyScanGtkWaterfallMeterItem *task);
static void     hyscan_gtk_waterfall_meter_draw                    (GtkWidget               *widget,
                                                                    cairo_t                 *cairo,
                                                                    HyScanGtkWaterfallMeter *self);

static gboolean hyscan_gtk_waterfall_meter_configure               (GtkWidget               *widget,
                                                                    GdkEventConfigure       *event,
                                                                    HyScanGtkWaterfallMeter *self);
static void     hyscan_gtk_waterfall_meter_track_changed           (HyScanGtkWaterfallState *state,
                                                                    HyScanGtkWaterfallMeter *self);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkWaterfallMeter, hyscan_gtk_waterfall_meter, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkWaterfallMeter)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_waterfall_meter_interface_init));

static void
hyscan_gtk_waterfall_meter_class_init (HyScanGtkWaterfallMeterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_waterfall_meter_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_meter_object_finalize;
}

static void
hyscan_gtk_waterfall_meter_init (HyScanGtkWaterfallMeter *self)
{
  self->priv = hyscan_gtk_waterfall_meter_get_instance_private (self);
}


static void
hyscan_gtk_waterfall_meter_object_constructed (GObject *object)
{
  GdkRGBA color_meter, color_shadow, color_frame;
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (object);

  gdk_rgba_parse (&color_meter, "#f9c80e");
  gdk_rgba_parse (&color_shadow, SHADOW_DEFAULT);
  gdk_rgba_parse (&color_frame, FRAME_DEFAULT);

  hyscan_gtk_waterfall_meter_set_main_color   (self, color_meter);
  hyscan_gtk_waterfall_meter_set_shadow_color (self, color_shadow);
  hyscan_gtk_waterfall_meter_set_frame_color  (self, color_frame);
  hyscan_gtk_waterfall_meter_set_meter_width  (self, 1);
  hyscan_gtk_waterfall_meter_set_shadow_width (self, 3);

  /* Включаем видимость слоя. */
  hyscan_gtk_layer_set_visible (HYSCAN_GTK_LAYER (self), TRUE);
}

static void
hyscan_gtk_waterfall_meter_object_finalize (GObject *object)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (object);
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  /* Отключаемся от всех сигналов. */
  if (priv->wfall != NULL)
    g_signal_handlers_disconnect_by_data (priv->wfall, self);
  g_clear_object (&priv->wfall);

  g_clear_pointer (&priv->font, g_object_unref);

  g_list_free_full (priv->all, g_free);
  g_list_free_full (priv->cancellable, g_free);
  g_list_free_full (priv->visible, g_free);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_meter_parent_class)->finalize (object);
}

void
hyscan_gtk_waterfall_meter_added (HyScanGtkLayer          *layer,
                                  HyScanGtkLayerContainer *container)
{
  HyScanGtkWaterfall *wfall;
  HyScanGtkWaterfallMeter *self;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (container));

  self = HYSCAN_GTK_WATERFALL_METER (layer);
  wfall = HYSCAN_GTK_WATERFALL (container);
  self->priv->wfall = g_object_ref (wfall);

  g_signal_connect (wfall, "visible-draw",               G_CALLBACK (hyscan_gtk_waterfall_meter_draw), self);
  g_signal_connect (wfall, "configure-event",            G_CALLBACK (hyscan_gtk_waterfall_meter_configure), self);
  g_signal_connect_after (wfall, "key-press-event",      G_CALLBACK (hyscan_gtk_waterfall_meter_key), self);
  g_signal_connect_after (wfall, "button-press-event",   G_CALLBACK (hyscan_gtk_waterfall_meter_button), self);
  g_signal_connect_after (wfall, "motion-notify-event",  G_CALLBACK (hyscan_gtk_waterfall_meter_motion), self);

  g_signal_connect (wfall, "changed::track",  G_CALLBACK (hyscan_gtk_waterfall_meter_track_changed), self);
}

void
hyscan_gtk_waterfall_meter_removed (HyScanGtkLayer *layer)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (layer);

  g_signal_handlers_disconnect_by_data (self->priv->wfall, self);
  g_clear_object (&self->priv->wfall);
}

/* Функция захвата ввода. */
static gboolean
hyscan_gtk_waterfall_meter_grab_input (HyScanGtkLayer *layer)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (layer);

  /* Мы не можем захватить ввод, если слой отключен. */
  if (!hyscan_gtk_layer_get_visible (layer))
    return FALSE;

  hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (self->priv->wfall), self);
  hyscan_gtk_layer_container_set_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (self->priv->wfall), TRUE);

  return TRUE;
}

/* Функция задает видимость слоя. */
static void
hyscan_gtk_waterfall_meter_set_visible (HyScanGtkLayer *layer,
                                        gboolean        visible)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (layer);

  self->priv->layer_visibility = visible;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция возвращает видимость слоя. */
static gboolean
hyscan_gtk_waterfall_meter_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (layer);

  return self->priv->layer_visibility;
}

/* Функция возвращает название иконки. */
static const gchar*
hyscan_gtk_waterfall_meter_get_icon_name (HyScanGtkLayer *iface)
{
  return "preferences-desktop-display-symbolic";
}

/* Функция ищет линейку по идентификатору. */
static gint
hyscan_gtk_waterfall_meter_find_by_id (gconstpointer _a,
                                       gconstpointer _b)
{
  const HyScanGtkWaterfallMeterItem *a = _a;
  const HyScanGtkWaterfallMeterItem *b = _b;

  if (a->id < b->id)
    return -1;
  else if (a->id == b->id)
    return 0;
  else
    return 1;
}

/* Функция ищет ближайшую к указателю линейку. */
static GList*
hyscan_gtk_waterfall_meter_find_closest (HyScanGtkWaterfallMeter *self,
                                         HyScanCoordinates       *pointer)
{
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;
  GList *link, *mlink = NULL;
  gdouble rs, re, r;
  gdouble rmin = G_MAXDOUBLE;

  for (link = priv->visible; link != NULL; link = link->next)
    {
      HyScanGtkWaterfallMeterItem *drawn = link->data;

      rs = hyscan_gtk_waterfall_tools_distance (pointer, &drawn->start);
      re = hyscan_gtk_waterfall_tools_distance (pointer, &drawn->end);

      r = MIN (rs, re);
      if (r > rmin)
        continue;

      rmin = r;
      mlink = link;
    }

  /* Проверяем, что хендл в радиусе действия. */
  {
    HyScanCoordinates px_pointer, px_handle;
    gdouble thres = HANDLE_RADIUS;

    gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->wfall),
                                           &px_pointer.x, &px_pointer.y,
                                           pointer->x, pointer->y);
    gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->wfall),
                                           &px_handle.x, &px_handle.y,
                                           pointer->x + rmin, pointer->y);
    if (hyscan_gtk_waterfall_tools_distance (&px_handle, &px_pointer) > thres)
      return NULL;
  }

  return mlink;
}

/* Создание хэндла. */
static gboolean
hyscan_gtk_waterfall_meter_handle_create (HyScanGtkLayer *layer,
                                          GdkEventButton *event)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (layer);
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  /* Обязательные проверки: слой не владеет вводом, редактирование запрещено, слой скрыт. */
  if (self != hyscan_gtk_layer_container_get_input_owner (HYSCAN_GTK_LAYER_CONTAINER (priv->wfall)) ||
      !hyscan_gtk_layer_container_get_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (priv->wfall)) ||
      !hyscan_gtk_layer_get_visible (layer))
    {
      return FALSE;
    }


  /* Запоминаем координаты начала. */
  gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (priv->wfall),
                                         event->x,
                                         event->y,
                                         &priv->current.start.x,
                                         &priv->current.start.y);

  priv->current.end = priv->current.start;
  priv->current.id = ++priv->last_id;

  /* Устанавливаем режим. */
  priv->editing = TRUE;

  if (priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (priv->wfall);

  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->wfall), self);
  return TRUE;
}

/* Функция отпускает хэндл. */
static gboolean
hyscan_gtk_waterfall_meter_handle_release (HyScanGtkLayer *layer,
                                           gconstpointer   howner)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (layer);
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;
  HyScanGtkWaterfallMeterItem *item;

  if (self != howner)
    return FALSE;

  if (!priv->editing)
    {
      g_warning ("release but no edit");
      return FALSE;
    }

  item = g_new (HyScanGtkWaterfallMeterItem, 1);
  *item = priv->current;
  priv->all = g_list_append (priv->all, item);

  g_list_free_full (priv->cancellable, g_free);
  priv->cancellable = NULL;

  /* Сбрасываем режим. */
  priv->editing = FALSE;
  hyscan_gtk_waterfall_queue_draw (priv->wfall);
  return TRUE;
}

/* Функция поиска хендла. */
static gboolean
hyscan_gtk_waterfall_meter_handle_find (HyScanGtkLayer       *layer,
                                        gdouble               x,
                                        gdouble               y,
                                        HyScanGtkLayerHandle *handle)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (layer);
  HyScanGtkWaterfallMeterItem *item;
  HyScanCoordinates mouse = {.x = x, .y = y};
  gdouble re, rs;
  GList *link;

  /* Обязательные проверки: редактирование запрещено, слой скрыт. */
  if (!hyscan_gtk_layer_container_get_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (self->priv->wfall)) ||
      !hyscan_gtk_layer_get_visible (layer))
    {
      return FALSE;
    }

  /* Поиск хэндла в списке видимых линеек. */
  link = hyscan_gtk_waterfall_meter_find_closest (self, &mouse);

  if (link == NULL)
    return FALSE;

  item = link->data;
  rs = hyscan_gtk_waterfall_tools_distance (&mouse, &item->start);
  re = hyscan_gtk_waterfall_tools_distance (&mouse, &item->end);

  handle->val_x = (rs < re) ? item->start.x : item->end.x;
  handle->val_y = (rs < re) ? item->start.y : item->end.y;
  handle->user_data = link;

  return TRUE;
}

/* Функция передачи хендла, который следует показать (или спрятать). */
static void
hyscan_gtk_waterfall_meter_handle_show (HyScanGtkLayer       *layer,
                                        HyScanGtkLayerHandle *handle)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (layer);
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  priv->highlight = handle != NULL;
  if (handle != NULL)
    {
      priv->handle_to_highlight.x = handle->val_x;
      priv->handle_to_highlight.y = handle->val_y;
    }
}

/* Функция хватает хэндл. */
static gconstpointer
hyscan_gtk_waterfall_meter_handle_grab (HyScanGtkLayer       *layer,
                                        HyScanGtkLayerHandle *handle)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (layer);
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;
  HyScanCoordinates mouse = {.x = handle->val_x, .y = handle->val_y};
  GList *link;
  HyScanGtkWaterfallMeterItem *item;
  gdouble re, rs;

  item = ((GList*)handle->user_data)->data;
  rs = hyscan_gtk_waterfall_tools_distance (&mouse, &item->start);
  re = hyscan_gtk_waterfall_tools_distance (&mouse, &item->end);

  priv->current = *item;
  priv->current.start = (rs > re) ? item->start : item->end;
  priv->current.end   = (rs > re) ? item->end : item->start;

  /* Теперь нужно найти линейку в списке отрисовываемых и перетащить её
  * в cancellable, на случай если пользователю захочется отменить редактирование. */
  link = g_list_find_custom (priv->all, &priv->current, hyscan_gtk_waterfall_meter_find_by_id);
  priv->all = g_list_remove_link (priv->all, link);
  priv->cancellable = link;

  /* Устанавливаем режим. */
  priv->editing = TRUE;
  hyscan_gtk_waterfall_queue_draw (priv->wfall);

  return self;
}

/* Функция обрабатывает нажатия клавиш клавиатуры. */
static gboolean
hyscan_gtk_waterfall_meter_key (GtkWidget               *widget,
                                GdkEventKey             *event,
                                HyScanGtkWaterfallMeter *self)
{
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  if (!priv->editing)
    return GDK_EVENT_PROPAGATE;

  switch (event->keyval)
    {
    /* Отмена. */
    case GDK_KEY_Escape:
      if (priv->cancellable != NULL)
        {
          priv->all = g_list_concat (priv->all, priv->cancellable);
          priv->cancellable = NULL;
        }

    /* Удаление. */
    case GDK_KEY_Delete:
      g_list_free_full (priv->cancellable, g_free);
      priv->cancellable = NULL;
      priv->editing = FALSE;
      hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->wfall), NULL);
      break;

    default:
      return GDK_EVENT_PROPAGATE;
    }

  hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (widget));
  return GDK_EVENT_STOP;
}

/* В эту функцию вынесена обработка нажатия. */
static gboolean
hyscan_gtk_waterfall_meter_button (GtkWidget               *widget,
                                   GdkEventButton          *event,
                                   HyScanGtkWaterfallMeter *self)
{
  /* Игнорируем нажатия всех кнопок, кроме левой. */
  if (event->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  /* Запомним координату, если это НАЖАТИЕ. */
  if (event->type == GDK_BUTTON_PRESS)
    {
      self->priv->press.x = event->x;
      self->priv->press.y = event->y;
    }

  return GDK_EVENT_PROPAGATE;
}

/* Обработка движений мыши. */
static gboolean
hyscan_gtk_waterfall_meter_motion (GtkWidget               *widget,
                                   GdkEventMotion          *event,
                                   HyScanGtkWaterfallMeter *self)
{
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  if (priv->editing)
    {
      /* Запоминаем текущие координаты. */
      gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (widget), event->x, event->y,
                                             &priv->current.end.x, &priv->current.end.y);
    }

  priv->pointer.x = event->x;
  priv->pointer.y = event->y;

  return GDK_EVENT_PROPAGATE;
}

/* Вспомогательная функция отрисовки. */
static void
hyscan_gtk_waterfall_meter_draw_helper (cairo_t           *cairo,
                                        HyScanCoordinates *start,
                                        HyScanCoordinates *end,
                                        GdkRGBA           *color,
                                        gdouble            width)
{
  hyscan_cairo_set_source_gdk_rgba (cairo, color);

  cairo_move_to (cairo, start->x, start->y);
  cairo_arc (cairo, start->x, start->y, width, 0, 2 * G_PI);
  cairo_fill (cairo);

  cairo_set_line_width (cairo, width);
  cairo_move_to (cairo, start->x, start->y);
  cairo_line_to (cairo, end->x, end->y);
  cairo_stroke (cairo);

  cairo_move_to (cairo, end->x, end->y);
  cairo_arc (cairo, end->x, end->y, width, 0, 2 * G_PI);
  cairo_fill (cairo);
}

/* Функция отрисовки одной задачи. */
static gboolean
hyscan_gtk_waterfall_meter_draw_task (HyScanGtkWaterfallMeter     *self,
                                      cairo_t                     *cairo,
                                      HyScanGtkWaterfallMeterItem *task)
{
  HyScanCoordinates start, end, mid;
  gboolean visible;
  gdouble angle, dist, dist_px;
  gint w, h;
  gchar *text;

  gdouble x,y;

  HyScanGtkWaterfallMeterPrivate *priv = self->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (self->priv->wfall);

  /* Определяем, видна ли эта линейка на экране. */
  visible = hyscan_gtk_waterfall_tools_line_in_square (&task->start, &task->end,
                                                       &priv->view._0,
                                                       &priv->view._1);

  /* Если нет, не рисуем её. */
  if (!visible)
    return FALSE;

  gtk_cifro_area_visible_value_to_point (carea, &start.x, &start.y, task->start.x, task->start.y);
  gtk_cifro_area_visible_value_to_point (carea, &end.x, &end.y, task->end.x, task->end.y);

  cairo_save (cairo);

  hyscan_gtk_waterfall_meter_draw_helper (cairo, &start, &end, &priv->color.shadow, priv->color.shadow_width);
  hyscan_gtk_waterfall_meter_draw_helper (cairo, &start, &end, &priv->color.meter, priv->color.meter_width);

  angle = hyscan_gtk_waterfall_tools_angle (&task->start, &task->end);
  dist = hyscan_gtk_waterfall_tools_distance (&task->start, &task->end);
  mid = hyscan_gtk_waterfall_tools_middle (&start, &end);
  dist_px = hyscan_gtk_waterfall_tools_distance (&start, &end);

  text = g_strdup_printf ("%3.2f", dist);
  pango_layout_set_text (priv->font, text, -1);
  g_free (text);

  pango_layout_get_size (priv->font, &w, &h);

  w /= PANGO_SCALE;
  h /= PANGO_SCALE;

  if (dist_px < 0.25 * (gdouble)w)
    return TRUE;

  cairo_move_to (cairo, mid.x, mid.y + priv->color.shadow_width);
  cairo_rotate (cairo, angle);
  cairo_get_current_point (cairo, &x, &y);
  cairo_translate (cairo, - w / 2.0, 0);

  hyscan_cairo_set_source_gdk_rgba (cairo, &priv->color.shadow);
  cairo_rectangle (cairo, x, y, w, h);
  cairo_fill (cairo);
  hyscan_cairo_set_source_gdk_rgba (cairo, &priv->color.frame);
  cairo_rectangle (cairo, x, y, w, h);
  cairo_stroke (cairo);

  hyscan_cairo_set_source_gdk_rgba (cairo, &priv->color.meter);

  cairo_move_to (cairo, x, y);
  pango_cairo_show_layout (cairo, priv->font);
  cairo_restore (cairo);

  return TRUE;
}

/* Функция отрисовки хэндла. */
static void
hyscan_gtk_waterfall_meter_draw_handle (cairo_t          *cairo,
                                        GdkRGBA           inner,
                                        GdkRGBA           outer,
                                        HyScanCoordinates center,
                                        gdouble           radius)
{
  cairo_pattern_t *pttrn;
  cairo_matrix_t matrix;

  pttrn = hyscan_gtk_waterfall_tools_make_handle_pattern (radius, inner, outer);

  cairo_matrix_init_translate (&matrix, -center.x, -center.y);
  cairo_pattern_set_matrix (pttrn, &matrix);

  cairo_set_source (cairo, pttrn);
  cairo_arc (cairo, center.x, center.y, radius, 0, G_PI * 2);
  cairo_fill (cairo);

  cairo_pattern_destroy (pttrn);
}

/* Функция отрисовки. */
static void
hyscan_gtk_waterfall_meter_draw (GtkWidget               *widget,
                                 cairo_t                 *cairo,
                                 HyScanGtkWaterfallMeter *self)
{
  /* -TODO: переделать, чтобы сначала рисовалась все подложки, потом все линейки
   * TODO: возможно, имеет смысл обновлять только когда реально поменялись параметры отображения
   * для этого надо ловить изменение вью/скейла, а на обычный мышевоз не реагировать
   */
  GList *link;
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  /* Проверяем видимость слоя. */
  if (!priv->layer_visibility)
    return;

  gtk_cifro_area_get_view (GTK_CIFRO_AREA (widget),
                           &priv->view._0.x,
                           &priv->view._1.x,
                           &priv->view._0.y,
                           &priv->view._1.y);

  /* Очищаем список отрисованных меток. */
  g_list_free_full (priv->visible, g_free);
  priv->visible = NULL;

  for (link = priv->all; link != NULL; link = link->next)
    {
      HyScanGtkWaterfallMeterItem *new_item;
      HyScanGtkWaterfallMeterItem *task = link->data;

      /* Функция вернет FALSE, если метку рисовать не надо.
       * В противном случае её надо отправить в список видимых. */
      if (!hyscan_gtk_waterfall_meter_draw_task (self, cairo, task))
        continue;

      new_item = g_new (HyScanGtkWaterfallMeterItem, 1);
      *new_item = *task;
      priv->visible = g_list_prepend (priv->visible, new_item);
    }

  if (priv->highlight)
    {
      HyScanCoordinates center;
      gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->wfall),
                                             &center.x, &center.y,
                                             priv->handle_to_highlight.x,
                                             priv->handle_to_highlight.y);
      hyscan_gtk_waterfall_meter_draw_handle (cairo, priv->color.meter, priv->color.shadow, center, HANDLE_RADIUS);
    }
  if (priv->editing)
    {
      hyscan_gtk_waterfall_meter_draw_task (self, cairo, &priv->current);
    }
}

/* Смена параметров виджета. */
static gboolean
hyscan_gtk_waterfall_meter_configure (GtkWidget               *widget,
                                      GdkEventConfigure       *event,
                                      HyScanGtkWaterfallMeter *self)
{
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  g_clear_pointer (&priv->font, g_object_unref);

  priv->font = gtk_widget_create_pango_layout (widget, NULL);

  return FALSE;
}

static void
hyscan_gtk_waterfall_meter_track_changed (HyScanGtkWaterfallState *state,
                                          HyScanGtkWaterfallMeter *self)
{
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  g_list_free_full (priv->all, g_free);
  priv->all = NULL;
  g_list_free_full (priv->cancellable, g_free);
  priv->cancellable = NULL;
  g_list_free_full (priv->visible, g_free);
  priv->visible = NULL;

  priv->editing = FALSE;
}

/**
 * hyscan_gtk_waterfall_meter_new:
 *
 * Функция создает новый слой HyScanGtkWaterfallMeter.
 *
 * Returns: (transfer full): новый слой HyScanGtkWaterfallMeter.
 */
HyScanGtkWaterfallMeter*
hyscan_gtk_waterfall_meter_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_METER, NULL);
}

/**
 * hyscan_gtk_waterfall_meter_set_main_color:
 * @meter: объект #HyScanGtkWaterfallMeter
 * @color: основной цвет
 *
 * Функция задает основной цвет.
 * Влияет на цвет линеек и текста.
 */
void
hyscan_gtk_waterfall_meter_set_main_color (HyScanGtkWaterfallMeter *self,
                                           GdkRGBA                  color)
{
   g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

   self->priv->color.meter = color;
   if (self->priv->wfall != NULL)
     hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_meter_set_shadow_color:
 * @meter: объект #HyScanGtkWaterfallMeter
 * @color: цвет подложки
 *
 * Функция устанавливает цвет подложки.
 * Подложки рисуются под линейками и текстом.
 */
void
hyscan_gtk_waterfall_meter_set_shadow_color (HyScanGtkWaterfallMeter *self,
                                             GdkRGBA                  color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

  self->priv->color.shadow = color;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_meter_set_frame_color:
 * @meter: объект #HyScanGtkWaterfallMeter
 * @color: цвет рамок
 *
 * Функция задает цвет рамки.
 * Рамки рисуются вокруг подложки под текстом.
 */
void
hyscan_gtk_waterfall_meter_set_frame_color (HyScanGtkWaterfallMeter *self,
                                            GdkRGBA                  color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

  self->priv->color.frame = color;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_meter_set_meter_width:
 * @meter: объект #HyScanGtkWaterfallMeter
 * @width: толщина основных линий
 *
 * Функция задает толщину основных линий.
 */
void
hyscan_gtk_waterfall_meter_set_meter_width (HyScanGtkWaterfallMeter *self,
                                            gdouble                  width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

  self->priv->color.meter_width = width;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_meter_set_shadow_width:
 * @meter: объект #HyScanGtkWaterfallMeter
 * @width: толщина подложки
 *
 * Функция задает толщину подложки.
 */
void
hyscan_gtk_waterfall_meter_set_shadow_width (HyScanGtkWaterfallMeter *self,
                                             gdouble                  width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

  self->priv->color.shadow_width = width;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

static void
hyscan_gtk_waterfall_meter_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_waterfall_meter_added;
  iface->removed = hyscan_gtk_waterfall_meter_removed;
  iface->grab_input = hyscan_gtk_waterfall_meter_grab_input;
  iface->set_visible = hyscan_gtk_waterfall_meter_set_visible;
  iface->get_visible = hyscan_gtk_waterfall_meter_get_visible;
  iface->get_icon_name = hyscan_gtk_waterfall_meter_get_icon_name;

  iface->handle_create = hyscan_gtk_waterfall_meter_handle_create;
  iface->handle_release = hyscan_gtk_waterfall_meter_handle_release;
  iface->handle_find = hyscan_gtk_waterfall_meter_handle_find;
  iface->handle_show = hyscan_gtk_waterfall_meter_handle_show;
  iface->handle_grab = hyscan_gtk_waterfall_meter_handle_grab;
}
