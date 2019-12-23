/* hyscan-gtk-waterfall-shadowm.c
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
 * SECTION: hyscan-gtk-waterfall-shadowm
 * @Title HyScanGtkWaterfallShadowm
 * @Short_description Слой линеек водопада
 *
 * Данный слой позволяет производить простые линейные измерения на водопаде.
 * Он поддерживает бесконечное число линеек, каждая из которых состоит из
 * двух точек.
 */
#include "hyscan-gtk-waterfall-shadowm.h"
#include "hyscan-gtk-waterfall-tools.h"
#include <math.h>

typedef struct
{
  guint64  id;
  gdouble  y;
  gdouble  _start;
  gdouble  _mid;
  gdouble  _end;
} HyScanGtkWaterfallShadowmItem;

enum
{
  NONE,
  FIRST,
  SECOND,
  EDIT_START,
  EDIT_MID,
  EDIT_END
};

struct _HyScanGtkWaterfallShadowmPrivate
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
  HyScanGtkWaterfallShadowmItem   current;             /* Редактируемая прямо сейчас линейка. */
  gboolean                      mode;             /* Режим. */

  struct
  {
    HyScanCoordinates _0;
    HyScanCoordinates _1;
    HyScanCoordinates _2;
  } view;

  struct
  {
    GdkRGBA                     shadowm;
    GdkRGBA                     shadow;
    GdkRGBA                     frame;
    gdouble                     shadowm_width;
    gdouble                     shadow_width;
    gboolean                    draw_shadow;
  } color;

};

static void     hyscan_gtk_waterfall_shadowm_interface_init          (HyScanGtkLayerInterface *iface);
static void     hyscan_gtk_waterfall_shadowm_object_constructed      (GObject                 *object);
static void     hyscan_gtk_waterfall_shadowm_object_finalize         (GObject                 *object);

static gint     hyscan_gtk_waterfall_shadowm_find_by_id              (gconstpointer            a,
                                                                    gconstpointer            b);

static GList*   hyscan_gtk_waterfall_shadowm_find_closest            (HyScanGtkWaterfallShadowm *self,
                                                                    HyScanCoordinates       *pointer);
static gboolean hyscan_gtk_waterfall_shadowm_handle_create           (HyScanGtkLayer          *layer,
                                                                    GdkEventButton          *event);
static gboolean hyscan_gtk_waterfall_shadowm_handle_release          (HyScanGtkLayer          *layer,
                                                                    GdkEventButton          *event,
                                                                    gconstpointer            howner);
static gboolean hyscan_gtk_waterfall_shadowm_handle_find             (HyScanGtkLayer          *layer,
                                                                    gdouble                  x,
                                                                    gdouble                  y,
                                                                    HyScanGtkLayerHandle    *handle);
static void     hyscan_gtk_waterfall_shadowm_handle_show             (HyScanGtkLayer          *layer,
                                                                    HyScanGtkLayerHandle    *handle);
static void     hyscan_gtk_waterfall_shadowm_handle_click            (HyScanGtkLayer          *layer,
                                                                    GdkEventButton          *event,
                                                                    HyScanGtkLayerHandle    *handle);

static gboolean hyscan_gtk_waterfall_shadowm_key                     (GtkWidget               *widget,
                                                                    GdkEventKey             *event,
                                                                    HyScanGtkWaterfallShadowm *self);
static gboolean hyscan_gtk_waterfall_shadowm_button                  (GtkWidget               *widget,
                                                                    GdkEventButton          *event,
                                                                    HyScanGtkWaterfallShadowm *self);

static gboolean hyscan_gtk_waterfall_shadowm_motion                  (GtkWidget               *widget,
                                                                    GdkEventMotion          *event,
                                                                    HyScanGtkWaterfallShadowm *self);
static void     hyscan_gtk_waterfall_shadowm_draw_helper             (cairo_t                 *cairo,
                                                                    HyScanCoordinates       *start,
                                                                    HyScanCoordinates       *end,
                                                                    GdkRGBA                 *color,
                                                                    gdouble                  width);
static gboolean hyscan_gtk_waterfall_shadowm_draw_task               (HyScanGtkWaterfallShadowm     *self,
                                                                    cairo_t                     *cairo,
                                                                    HyScanGtkWaterfallShadowmItem *task);
static void     hyscan_gtk_waterfall_shadowm_draw                    (GtkWidget               *widget,
                                                                    cairo_t                 *cairo,
                                                                    HyScanGtkWaterfallShadowm *self);

static gboolean hyscan_gtk_waterfall_shadowm_configure               (GtkWidget               *widget,
                                                                    GdkEventConfigure       *event,
                                                                    HyScanGtkWaterfallShadowm *self);
static void     hyscan_gtk_waterfall_shadowm_track_changed           (HyScanGtkWaterfallState *state,
                                                                    HyScanGtkWaterfallShadowm *self);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkWaterfallShadowm, hyscan_gtk_waterfall_shadowm, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkWaterfallShadowm)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_waterfall_shadowm_interface_init));

static void
hyscan_gtk_waterfall_shadowm_class_init (HyScanGtkWaterfallShadowmClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_waterfall_shadowm_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_shadowm_object_finalize;
}

static void
hyscan_gtk_waterfall_shadowm_init (HyScanGtkWaterfallShadowm *self)
{
  self->priv = hyscan_gtk_waterfall_shadowm_get_instance_private (self);
}


static void
hyscan_gtk_waterfall_shadowm_object_constructed (GObject *object)
{
  GdkRGBA color_shadowm, color_shadow, color_frame;
  HyScanGtkWaterfallShadowm *self = HYSCAN_GTK_WATERFALL_SHADOWM (object);

  gdk_rgba_parse (&color_shadowm, "#29bf12");
  gdk_rgba_parse (&color_shadow, SHADOW_DEFAULT);
  gdk_rgba_parse (&color_frame, FRAME_DEFAULT);

  hyscan_gtk_waterfall_shadowm_set_main_color   (self, color_shadowm);
  hyscan_gtk_waterfall_shadowm_set_shadow_color (self, color_shadow);
  hyscan_gtk_waterfall_shadowm_set_frame_color  (self, color_frame);
  hyscan_gtk_waterfall_shadowm_set_shadowm_width  (self, 1);
  hyscan_gtk_waterfall_shadowm_set_shadow_width (self, 3);

  /* Включаем видимость слоя. */
  hyscan_gtk_layer_set_visible (HYSCAN_GTK_LAYER (self), TRUE);
}

static void
hyscan_gtk_waterfall_shadowm_object_finalize (GObject *object)
{
  HyScanGtkWaterfallShadowm *self = HYSCAN_GTK_WATERFALL_SHADOWM (object);
  HyScanGtkWaterfallShadowmPrivate *priv = self->priv;

  /* Отключаемся от всех сигналов. */
  if (priv->wfall != NULL)
    g_signal_handlers_disconnect_by_data (priv->wfall, self);
  g_clear_object (&priv->wfall);

  g_clear_pointer (&priv->font, g_object_unref);

  g_list_free_full (priv->all, g_free);
  g_list_free_full (priv->cancellable, g_free);
  g_list_free_full (priv->visible, g_free);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_shadowm_parent_class)->finalize (object);
}

void
hyscan_gtk_waterfall_shadowm_added (HyScanGtkLayer          *layer,
                                  HyScanGtkLayerContainer *container)
{
  HyScanGtkWaterfall *wfall;
  HyScanGtkWaterfallShadowm *self;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (container));

  self = HYSCAN_GTK_WATERFALL_SHADOWM (layer);
  wfall = HYSCAN_GTK_WATERFALL (container);
  self->priv->wfall = g_object_ref (wfall);

  g_signal_connect (wfall, "visible-draw",               G_CALLBACK (hyscan_gtk_waterfall_shadowm_draw), self);
  g_signal_connect (wfall, "configure-event",            G_CALLBACK (hyscan_gtk_waterfall_shadowm_configure), self);
  g_signal_connect_after (wfall, "key-press-event",      G_CALLBACK (hyscan_gtk_waterfall_shadowm_key), self);
  g_signal_connect_after (wfall, "button-press-event",   G_CALLBACK (hyscan_gtk_waterfall_shadowm_button), self);
  g_signal_connect_after (wfall, "motion-notify-event",  G_CALLBACK (hyscan_gtk_waterfall_shadowm_motion), self);

  g_signal_connect (wfall, "changed::track",  G_CALLBACK (hyscan_gtk_waterfall_shadowm_track_changed), self);
}

void
hyscan_gtk_waterfall_shadowm_removed (HyScanGtkLayer *layer)
{
  HyScanGtkWaterfallShadowm *self = HYSCAN_GTK_WATERFALL_SHADOWM (layer);

  g_signal_handlers_disconnect_by_data (self->priv->wfall, self);
  g_clear_object (&self->priv->wfall);
}

/* Функция захвата ввода. */
static gboolean
hyscan_gtk_waterfall_shadowm_grab_input (HyScanGtkLayer *layer)
{
  HyScanGtkWaterfallShadowm *self = HYSCAN_GTK_WATERFALL_SHADOWM (layer);

  /* Мы не можем захватить ввод, если слой отключен. */
  if (!hyscan_gtk_layer_get_visible (layer))
    return FALSE;

  hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (self->priv->wfall), self);
  hyscan_gtk_layer_container_set_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (self->priv->wfall), TRUE);

  return TRUE;
}

/* Функция задает видимость слоя. */
static void
hyscan_gtk_waterfall_shadowm_set_visible (HyScanGtkLayer *layer,
                                        gboolean        visible)
{
  HyScanGtkWaterfallShadowm *self = HYSCAN_GTK_WATERFALL_SHADOWM (layer);

  self->priv->layer_visibility = visible;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция возвращает видимость слоя. */
static gboolean
hyscan_gtk_waterfall_shadowm_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkWaterfallShadowm *self = HYSCAN_GTK_WATERFALL_SHADOWM (layer);

  return self->priv->layer_visibility;
}

/* Функция возвращает название иконки. */
static const gchar*
hyscan_gtk_waterfall_shadowm_get_icon_name (HyScanGtkLayer *iface)
{
  return "multimedia-volume-control-symbolic";
}

/* Функция ищет линейку по идентификатору. */
static gint
hyscan_gtk_waterfall_shadowm_find_by_id (gconstpointer _a,
                                       gconstpointer _b)
{
  const HyScanGtkWaterfallShadowmItem *a = _a;
  const HyScanGtkWaterfallShadowmItem *b = _b;

  if (a->id < b->id)
    return -1;
  else if (a->id == b->id)
    return 0;
  else
    return 1;
}

/* Функция ищет ближайшую к указателю линейку. */
static GList *
hyscan_gtk_waterfall_shadowm_find_closest (HyScanGtkWaterfallShadowm *self,
                                           HyScanCoordinates       *pointer)
{
  HyScanGtkWaterfallShadowmPrivate *priv = self->priv;
  GList *link, *mlink = NULL;
  gdouble rs, re, rm, r;
  gdouble rmin = G_MAXDOUBLE;

  for (link = priv->visible; link != NULL; link = link->next)
    {
      HyScanCoordinates coord;
      HyScanGtkWaterfallShadowmItem *drawn = link->data;

      coord.y = drawn->y;
      coord.x = drawn->_start;
      rs = hyscan_gtk_waterfall_tools_distance (pointer, &coord);
      coord.x = drawn->_mid;
      rm = hyscan_gtk_waterfall_tools_distance (pointer, &coord);
      coord.x = drawn->_end;
      re = hyscan_gtk_waterfall_tools_distance (pointer, &coord);

      r = MIN (MIN (rs, re), rm);
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
hyscan_gtk_waterfall_shadowm_handle_create  (HyScanGtkLayer *layer,
                                          GdkEventButton *event)
{
  HyScanGtkWaterfallShadowm *self = HYSCAN_GTK_WATERFALL_SHADOWM (layer);
  HyScanGtkWaterfallShadowmPrivate *priv = self->priv;

  /* Обязательные проверки: слой не владеет вводом, редактирование запрещено, слой скрыт. */
  if (self != hyscan_gtk_layer_container_get_input_owner (HYSCAN_GTK_LAYER_CONTAINER (priv->wfall)))
    return FALSE;

  /* Запоминаем координаты начала. */
  gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (priv->wfall),
                                         event->x,
                                         event->y,
                                         &priv->current._start,
                                         &priv->current.y);

  priv->current._end = priv->current._mid = priv->current._start;
  priv->current.id = ++priv->last_id;

  /* Устанавливаем режим. */
  priv->mode = FIRST;

  if (priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (priv->wfall);

  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->wfall), self);
  return TRUE;
}

/* Функция отпускает хэндл. */
static gboolean
hyscan_gtk_waterfall_shadowm_handle_release (HyScanGtkLayer *layer,
                                           GdkEventButton *event,
                                           gconstpointer   howner)
{
  HyScanGtkWaterfallShadowm *self = HYSCAN_GTK_WATERFALL_SHADOWM (layer);
  HyScanGtkWaterfallShadowmPrivate *priv = self->priv;
  gboolean done = FALSE;
  gdouble x, y;

  if (self != howner)
    return FALSE;

  gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (priv->wfall),
                                         event->x, event->y, &x, &y);
  if (priv->mode == FIRST)
    {
      priv->current._mid = x;
      priv->current._end = priv->current._mid;

      priv->mode = SECOND;

      hyscan_gtk_waterfall_queue_draw (priv->wfall);
      return FALSE;
    }
  else if (priv->mode == SECOND)
    {
      priv->current._end = x;
      done = TRUE;
    }
  else if (priv->mode == EDIT_START)
    {
      priv->current._start = x;
      done = TRUE;
    }
  else if (priv->mode == EDIT_MID)
    {
      priv->current._mid = x;
      done = TRUE;
    }
  else if (priv->mode == EDIT_END)
    {
      priv->current._end = x;
      done = TRUE;
    }
  else
    {
      g_message ("wrong flow");
    }

  if (done)
    {
      HyScanGtkWaterfallShadowmItem *item;

      item = g_new (HyScanGtkWaterfallShadowmItem, 1);
      *item = priv->current;
      priv->all = g_list_append (priv->all, item);

      g_list_free_full (priv->cancellable, g_free);
      priv->cancellable = NULL;

      /* Сбрасываем режим. */
      priv->mode = NONE;
      hyscan_gtk_waterfall_queue_draw (priv->wfall);
      return TRUE;
    }

  return FALSE;
}

/* Функция поиска хендла. */
static gboolean
hyscan_gtk_waterfall_shadowm_handle_find (HyScanGtkLayer       *layer,
                                        gdouble               x,
                                        gdouble               y,
                                        HyScanGtkLayerHandle *handle)
{
  HyScanGtkWaterfallShadowm *self = HYSCAN_GTK_WATERFALL_SHADOWM (layer);
  HyScanGtkWaterfallShadowmItem *item;
  HyScanCoordinates coord;
  HyScanCoordinates mouse = {.x = x, .y = y};
  gdouble re, rs, rm;
  GList *link;

  /* Поиск хэндла в списке видимых линеек. */
  link = hyscan_gtk_waterfall_shadowm_find_closest (self, &mouse);

  if (link == NULL)
    return FALSE;

  item = link->data;

  coord.y = item->y;
  coord.x = item->_start;
  rs = hyscan_gtk_waterfall_tools_distance (&mouse, &coord);
  coord.x = item->_mid;
  rm = hyscan_gtk_waterfall_tools_distance (&mouse, &coord);
  coord.x = item->_end;
  re = hyscan_gtk_waterfall_tools_distance (&mouse, &coord);

  if (rs <= rm && rs <= re)
    handle->val_x = item->_start;
  else if (rm <= rs && rm <= re)
    handle->val_x = item->_mid;
  else
    handle->val_x = item->_end;

  handle->val_y = item->y;
  handle->user_data = link;

  return TRUE;
}

/* Функция передачи хендла, который следует показать (или спрятать). */
static void
hyscan_gtk_waterfall_shadowm_handle_show (HyScanGtkLayer       *layer,
                                        HyScanGtkLayerHandle *handle)
{
  HyScanGtkWaterfallShadowm *self = HYSCAN_GTK_WATERFALL_SHADOWM (layer);
  HyScanGtkWaterfallShadowmPrivate *priv = self->priv;

  priv->highlight = handle != NULL;
  if (handle != NULL)
    {
      priv->handle_to_highlight.x = handle->val_x;
      priv->handle_to_highlight.y = handle->val_y;
    }
}

/* Функция хватает хэндл. */
static void
hyscan_gtk_waterfall_shadowm_handle_click (HyScanGtkLayer       *layer,
                                         GdkEventButton       *event,
                                         HyScanGtkLayerHandle *handle)
{
  HyScanGtkWaterfallShadowm *self = HYSCAN_GTK_WATERFALL_SHADOWM (layer);
  HyScanGtkWaterfallShadowmPrivate *priv = self->priv;
  HyScanCoordinates mouse = {.x = handle->val_x, .y = handle->val_y};
  GList *link;
  HyScanGtkWaterfallShadowmItem *item;
  HyScanCoordinates coord;
  gdouble re, rs, rm;

  item = ((GList*)handle->user_data)->data;

  priv->current = *item;
  /* Нужно найти линейку в списке отрисовываемых и перетащить её
   * в cancellable, на случай если пользователю захочется отменить редактирование. */
  link = g_list_find_custom (priv->all, &priv->current, hyscan_gtk_waterfall_shadowm_find_by_id);
  priv->all = g_list_remove_link (priv->all, link);
  priv->cancellable = link;

  coord.y = item->y;
  coord.x = item->_start;
  rs = hyscan_gtk_waterfall_tools_distance (&mouse, &coord);
  coord.x = item->_mid;
  rm = hyscan_gtk_waterfall_tools_distance (&mouse, &coord);
  coord.x = item->_end;
  re = hyscan_gtk_waterfall_tools_distance (&mouse, &coord);

  if (rs <= rm && rs <= re)
    priv->mode = EDIT_START;
  else if (rm <= rs && rm <= re)
    priv->mode = EDIT_MID;
  else
    priv->mode = EDIT_END;

  hyscan_gtk_waterfall_queue_draw (priv->wfall);

  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->wfall), self);
}

/* Функция обрабатывает нажатия клавиш клавиатуры. */
static gboolean
hyscan_gtk_waterfall_shadowm_key (GtkWidget               *widget,
                                GdkEventKey             *event,
                                HyScanGtkWaterfallShadowm *self)
{
  HyScanGtkWaterfallShadowmPrivate *priv = self->priv;

  if (priv->mode == NONE)
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
      priv->mode = NONE;
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
hyscan_gtk_waterfall_shadowm_button (GtkWidget               *widget,
                                   GdkEventButton          *event,
                                   HyScanGtkWaterfallShadowm *self)
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
hyscan_gtk_waterfall_shadowm_motion (GtkWidget               *widget,
                                   GdkEventMotion          *event,
                                   HyScanGtkWaterfallShadowm *self)
{
  HyScanGtkWaterfallShadowmPrivate *priv = self->priv;
  gdouble x;

  gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (widget),
                                         event->x,
                                         event->y,
                                         &x,
                                         NULL);
  switch (priv->mode)
    {
    case FIRST:
      priv->current._mid = x;

    case SECOND:
      priv->current._end = x;
      break;

    case EDIT_START:
      priv->current._start = x;
      break;

    case EDIT_MID:
      priv->current._mid = x;
      break;

    case EDIT_END:
      priv->current._end = x;
      break;

    default:
      break;
    }

  return GDK_EVENT_PROPAGATE;
}

/* Вспомогательная функция отрисовки. */
static void
hyscan_gtk_waterfall_shadowm_draw_helper (cairo_t           *cairo,
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
hyscan_gtk_waterfall_shadowm_draw_task (HyScanGtkWaterfallShadowm     *self,
                                      cairo_t                     *cairo,
                                      HyScanGtkWaterfallShadowmItem *task)
{
  HyScanCoordinates start, end, mid, zero = {0., 0.}; /* real */
  HyScanCoordinates ca_start, ca_end, ca_mid;       /* */
  gdouble dist, dist_px;
  gint w, h;
  gchar *text;
  gdouble x,y;

  HyScanGtkWaterfallShadowmPrivate *priv = self->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (self->priv->wfall);

  start.y = mid.y = end.y = zero.y = task->y;
  start.x = task->_start;
  mid.x = task->_mid;
  end.x = task->_end;

  gtk_cifro_area_visible_value_to_point (carea, &ca_start.x, &ca_start.y, start.x, start.y);
  gtk_cifro_area_visible_value_to_point (carea, &ca_mid.x, &ca_mid.y, mid.x, mid.y);
  gtk_cifro_area_visible_value_to_point (carea, &ca_end.x, &ca_end.y, end.x, end.y);

  start.x = ABS (start.x);
  start.y = ABS (start.y);
  mid.x = ABS (mid.x);
  mid.y = ABS (mid.y);
  end.x = ABS (end.x);
  end.y = ABS (end.y);

  /* Определяем, видна ли эта линейка на экране. Если нет, не рисуем её. */
  {
    gboolean visible;

    visible = hyscan_gtk_waterfall_tools_line_in_square (&start, &end,
                                                         &priv->view._0,
                                                         &priv->view._1);
    if (!visible)
      return FALSE;
  }

  /*
  gtk_cifro_area_visible_value_to_point (carea, &start.x, &start.y, task->start.x, task->start.y);
  gtk_cifro_area_visible_value_to_point (carea, &end.x, &end.y, task->end.x, task->end.y);

  cairo_save (cairo);

  hyscan_gtk_waterfall_meter_draw_helper (cairo, &start, &end, &priv->color.shadow, priv->color.shadow_width);
  hyscan_gtk_waterfall_meter_draw_helper (cairo, &start, &end, &priv->color.meter, priv->color.meter_width);

  angle = hyscan_gtk_waterfall_tools_angle (&task->start, &task->end);
  dist = hyscan_gtk_waterfall_tools_distance (&task->start, &task->end);
  mid = hyscan_gtk_waterfall_tools_middle (&start, &end);
  dist_px = hyscan_gtk_waterfall_tools_distance (&start, &end);
  */
  cairo_save (cairo);

  hyscan_gtk_waterfall_shadowm_draw_helper (cairo, &ca_start, &ca_mid, &priv->color.shadow, priv->color.shadow_width);
  hyscan_gtk_waterfall_shadowm_draw_helper (cairo, &ca_mid, &ca_end, &priv->color.shadow, priv->color.shadow_width);
  hyscan_gtk_waterfall_shadowm_draw_helper (cairo, &ca_start, &ca_mid, &priv->color.shadowm, priv->color.shadowm_width);
  hyscan_gtk_waterfall_shadowm_draw_helper (cairo, &ca_mid, &ca_end, &priv->color.shadowm, priv->color.shadowm_width);

  /* Вычисляю значения. */
  {
    /*
                               /--
                           /---  |
                D      /---      |
                   /---          | h
          d1   /-M-              |
           /---  |x              |
       /---      |               |
    S----------------------------E
    X = H * D1 / D
    */
    gdouble d1, d, h;
    d1 = hyscan_gtk_waterfall_tools_distance (&start, &mid);
    d = hyscan_gtk_waterfall_tools_distance (&start, &zero);
    h = hyscan_gtk_waterfall_tools_distance (&end, &zero);
    dist = h * d1 / d;

    dist_px = hyscan_gtk_waterfall_tools_distance (&ca_start, &ca_mid);
    ca_mid = hyscan_gtk_waterfall_tools_middle (&ca_start, &ca_mid);
  }

  text = g_strdup_printf ("%3.2f", dist);
  pango_layout_set_text (priv->font, text, -1);
  g_free (text);

  pango_layout_get_size (priv->font, &w, &h);

  w /= PANGO_SCALE;
  h /= PANGO_SCALE;

  if (dist_px < 0.25 * (gdouble)w)
    return TRUE;

  cairo_move_to (cairo, ca_mid.x, ca_mid.y + priv->color.shadow_width);
  cairo_get_current_point (cairo, &x, &y);
  cairo_translate (cairo, - w / 2.0, 0);

  hyscan_cairo_set_source_gdk_rgba (cairo, &priv->color.shadow);
  cairo_rectangle (cairo, x, y, w, h);
  cairo_fill (cairo);
  hyscan_cairo_set_source_gdk_rgba (cairo, &priv->color.frame);
  cairo_rectangle (cairo, x, y, w, h);
  cairo_stroke (cairo);

  hyscan_cairo_set_source_gdk_rgba (cairo, &priv->color.shadowm);

  cairo_move_to (cairo, x, y);
  pango_cairo_show_layout (cairo, priv->font);
  cairo_restore (cairo);

  return TRUE;
}

/* Функция отрисовки хэндла. */
static void
hyscan_gtk_waterfall_shadowm_draw_handle (cairo_t          *cairo,
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
hyscan_gtk_waterfall_shadowm_draw (GtkWidget               *widget,
                                 cairo_t                 *cairo,
                                 HyScanGtkWaterfallShadowm *self)
{
  /* -TODO: переделать, чтобы сначала рисовалась все подложки, потом все линейки
   * TODO: возможно, имеет смысл обновлять только когда реально поменялись параметры отображения
   * для этого надо ловить изменение вью/скейла, а на обычный мышевоз не реагировать
   */
  GList *link;
  HyScanGtkWaterfallShadowmPrivate *priv = self->priv;

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
      HyScanGtkWaterfallShadowmItem *new_item;
      HyScanGtkWaterfallShadowmItem *task = link->data;

      /* Функция вернет FALSE, если метку рисовать не надо.
       * В противном случае её надо отправить в список видимых. */
      if (!hyscan_gtk_waterfall_shadowm_draw_task (self, cairo, task))
        continue;

      new_item = g_new (HyScanGtkWaterfallShadowmItem, 1);
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
      hyscan_gtk_waterfall_shadowm_draw_handle (cairo, priv->color.shadowm, priv->color.shadow, center, HANDLE_RADIUS);
    }
  if (priv->mode != NONE)
    {
      hyscan_gtk_waterfall_shadowm_draw_task (self, cairo, &priv->current);
    }
}

/* Смена параметров виджета. */
static gboolean
hyscan_gtk_waterfall_shadowm_configure (GtkWidget               *widget,
                                      GdkEventConfigure       *event,
                                      HyScanGtkWaterfallShadowm *self)
{
  HyScanGtkWaterfallShadowmPrivate *priv = self->priv;

  g_clear_pointer (&priv->font, g_object_unref);

  priv->font = gtk_widget_create_pango_layout (widget, NULL);

  return FALSE;
}

static void
hyscan_gtk_waterfall_shadowm_track_changed (HyScanGtkWaterfallState *state,
                                          HyScanGtkWaterfallShadowm *self)
{
  HyScanGtkWaterfallShadowmPrivate *priv = self->priv;

  g_list_free_full (priv->all, g_free);
  priv->all = NULL;
  g_list_free_full (priv->cancellable, g_free);
  priv->cancellable = NULL;
  g_list_free_full (priv->visible, g_free);
  priv->visible = NULL;

  priv->mode = NONE;
}

/**
 * hyscan_gtk_waterfall_shadowm_new:
 *
 * Функция создает новый слой HyScanGtkWaterfallShadowm.
 *
 * Returns: (transfer full): новый слой HyScanGtkWaterfallShadowm.
 */
HyScanGtkWaterfallShadowm*
hyscan_gtk_waterfall_shadowm_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_SHADOWM, NULL);
}

/**
 * hyscan_gtk_waterfall_shadowm_set_main_color:
 * @shadowm: объект #HyScanGtkWaterfallShadowm
 * @color: основной цвет
 *
 * Функция задает основной цвет.
 * Влияет на цвет линеек и текста.
 */
void
hyscan_gtk_waterfall_shadowm_set_main_color (HyScanGtkWaterfallShadowm *self,
                                           GdkRGBA                  color)
{
   g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_SHADOWM (self));

   self->priv->color.shadowm = color;
   if (self->priv->wfall != NULL)
     hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_shadowm_set_shadow_color:
 * @shadowm: объект #HyScanGtkWaterfallShadowm
 * @color: цвет подложки
 *
 * Функция устанавливает цвет подложки.
 * Подложки рисуются под линейками и текстом.
 */
void
hyscan_gtk_waterfall_shadowm_set_shadow_color (HyScanGtkWaterfallShadowm *self,
                                             GdkRGBA                  color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_SHADOWM (self));

  self->priv->color.shadow = color;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_shadowm_set_frame_color:
 * @shadowm: объект #HyScanGtkWaterfallShadowm
 * @color: цвет рамок
 *
 * Функция задает цвет рамки.
 * Рамки рисуются вокруг подложки под текстом.
 */
void
hyscan_gtk_waterfall_shadowm_set_frame_color (HyScanGtkWaterfallShadowm *self,
                                            GdkRGBA                  color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_SHADOWM (self));

  self->priv->color.frame = color;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_shadowm_set_shadowm_width:
 * @shadowm: объект #HyScanGtkWaterfallShadowm
 * @width: толщина основных линий
 *
 * Функция задает толщину основных линий.
 */
void
hyscan_gtk_waterfall_shadowm_set_shadowm_width (HyScanGtkWaterfallShadowm *self,
                                            gdouble                  width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_SHADOWM (self));

  self->priv->color.shadowm_width = width;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_shadowm_set_shadow_width:
 * @shadowm: объект #HyScanGtkWaterfallShadowm
 * @width: толщина подложки
 *
 * Функция задает толщину подложки.
 */
void
hyscan_gtk_waterfall_shadowm_set_shadow_width (HyScanGtkWaterfallShadowm *self,
                                             gdouble                  width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_SHADOWM (self));

  self->priv->color.shadow_width = width;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

static void
hyscan_gtk_waterfall_shadowm_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_waterfall_shadowm_added;
  iface->removed = hyscan_gtk_waterfall_shadowm_removed;
  iface->grab_input = hyscan_gtk_waterfall_shadowm_grab_input;
  iface->set_visible = hyscan_gtk_waterfall_shadowm_set_visible;
  iface->get_visible = hyscan_gtk_waterfall_shadowm_get_visible;
  iface->get_icon_name = hyscan_gtk_waterfall_shadowm_get_icon_name;

  iface->handle_create = hyscan_gtk_waterfall_shadowm_handle_create;
  iface->handle_release = hyscan_gtk_waterfall_shadowm_handle_release;
  iface->handle_find = hyscan_gtk_waterfall_shadowm_handle_find;
  iface->handle_show = hyscan_gtk_waterfall_shadowm_handle_show;
  iface->handle_click = hyscan_gtk_waterfall_shadowm_handle_click;
}
