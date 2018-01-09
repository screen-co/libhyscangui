/*
 * \file hyscan-gtk-waterfall-meter.c
 *
 * \brief Исходный файл слоя измерений
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-gtk-waterfall-meter.h"
#include "hyscan-gtk-waterfall-tools.h"
#include <math.h>

enum
{
  PROP_WATERFALL = 1
};

enum
{
  LOCAL_EMPTY,
  LOCAL_CREATE,
  LOCAL_EDIT,
  LOCAL_REMOVE,
  LOCAL_CANCEL,
};

typedef struct
{
  guint64            id;
  HyScanCoordinates  start;
  HyScanCoordinates  end;
  HyScanCoordinates  px_start; /* Вспомогательное поле для определения хэндлов. */
  HyScanCoordinates  px_end;   /* Вспомогательное поле для определения хэндлов. */
} HyScanGtkWaterfallMeterItem;

struct _HyScanGtkWaterfallMeterPrivate
{
  HyScanGtkWaterfallState      *wf_state;
  HyScanGtkWaterfall           *wfall;
  PangoLayout                  *font;              /* Раскладка шрифта. */

  gboolean                      layer_visibility;

  HyScanCoordinates             press;
  HyScanCoordinates             release;
  HyScanCoordinates             pointer;

  guint64                       last_id;
  GList                        *cancellable;
  GList                        *drawable;
  GList                        *visible;

  gint                          mode;
  HyScanGtkWaterfallMeterItem   current;

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

static void     hyscan_gtk_waterfall_meter_interface_init          (HyScanGtkWaterfallLayerInterface *iface);
static void     hyscan_gtk_waterfall_meter_set_property            (GObject                 *object,
                                                                    guint                    prop_id,
                                                                    const GValue            *value,
                                                                    GParamSpec              *pspec);
static void     hyscan_gtk_waterfall_meter_object_constructed      (GObject                 *object);
static void     hyscan_gtk_waterfall_meter_object_finalize         (GObject                 *object);

static gint     hyscan_gtk_waterfall_meter_find_by_id              (gconstpointer            a,
                                                                    gconstpointer            b);
static gpointer hyscan_gtk_waterfall_meter_handle                  (HyScanGtkWaterfallState *state,
                                                                    GdkEventButton          *event,
                                                                    HyScanGtkWaterfallMeter *self);

static gboolean hyscan_gtk_waterfall_meter_button                  (GtkWidget               *widget,
                                                                    GdkEventButton          *event,
                                                                    HyScanGtkWaterfallMeter *self);
static gboolean hyscan_gtk_waterfall_meter_key                     (GtkWidget               *widget,
                                                                    GdkEventKey             *event,
                                                                    HyScanGtkWaterfallMeter *self);

static gboolean hyscan_gtk_waterfall_meter_interaction_processor   (GtkWidget               *widget,
                                                                    HyScanGtkWaterfallMeter *self);
static gboolean hyscan_gtk_waterfall_meter_interaction_resolver    (GtkWidget               *widget,
                                                                    GdkEventAny             *event,
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

G_DEFINE_TYPE_WITH_CODE (HyScanGtkWaterfallMeter, hyscan_gtk_waterfall_meter, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkWaterfallMeter)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_WATERFALL_LAYER, hyscan_gtk_waterfall_meter_interface_init));

static void
hyscan_gtk_waterfall_meter_class_init (HyScanGtkWaterfallMeterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_waterfall_meter_set_property;

  object_class->constructed = hyscan_gtk_waterfall_meter_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_meter_object_finalize;

  g_object_class_install_property (object_class, PROP_WATERFALL,
    g_param_spec_object ("waterfall", "Waterfall", "GtkWaterfall object",
                         HYSCAN_TYPE_GTK_WATERFALL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_waterfall_meter_init (HyScanGtkWaterfallMeter *self)
{
  self->priv = hyscan_gtk_waterfall_meter_get_instance_private (self);
}

static void
hyscan_gtk_waterfall_meter_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (object);

  if (prop_id == PROP_WATERFALL)
    {
      self->priv->wfall = g_value_dup_object (value);
      self->priv->wf_state = HYSCAN_GTK_WATERFALL_STATE (self->priv->wfall);
    }
  else
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hyscan_gtk_waterfall_meter_object_constructed (GObject *object)
{
  GdkRGBA color_meter, color_shadow, color_frame;
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (object);
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  g_signal_connect (priv->wf_state, "visible-draw",               G_CALLBACK (hyscan_gtk_waterfall_meter_draw), self);
  g_signal_connect (priv->wf_state, "configure-event",            G_CALLBACK (hyscan_gtk_waterfall_meter_configure), self);
  g_signal_connect_after (priv->wf_state, "key-press-event",      G_CALLBACK (hyscan_gtk_waterfall_meter_interaction_resolver), self);
  g_signal_connect_after (priv->wf_state, "button-release-event", G_CALLBACK (hyscan_gtk_waterfall_meter_interaction_resolver), self);
  g_signal_connect_after (priv->wf_state, "button-press-event",   G_CALLBACK (hyscan_gtk_waterfall_meter_button), self);
  g_signal_connect_after (priv->wf_state, "motion-notify-event",  G_CALLBACK (hyscan_gtk_waterfall_meter_motion), self);

  g_signal_connect (priv->wf_state, "handle",  G_CALLBACK (hyscan_gtk_waterfall_meter_handle), self);
  g_signal_connect (priv->wf_state, "changed::track",  G_CALLBACK (hyscan_gtk_waterfall_meter_track_changed), self);

  gdk_rgba_parse (&color_meter, "#fff44f");
  gdk_rgba_parse (&color_shadow, SHADOW_DEFAULT);
  gdk_rgba_parse (&color_frame, FRAME_DEFAULT);

  hyscan_gtk_waterfall_meter_set_main_color   (self, color_meter);
  hyscan_gtk_waterfall_meter_set_shadow_color (self, color_shadow);
  hyscan_gtk_waterfall_meter_set_frame_color  (self, color_frame);
  hyscan_gtk_waterfall_meter_set_meter_width  (self, 1);
  hyscan_gtk_waterfall_meter_set_shadow_width (self, 3);

  /* Включаем видимость слоя. */
  priv->layer_visibility = TRUE;
}

static void
hyscan_gtk_waterfall_meter_object_finalize (GObject *object)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (object);
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  /* Отключаемся от всех сигналов. */
  g_signal_handlers_disconnect_by_data (priv->wf_state, self);

  g_clear_object (&priv->wf_state);
  g_clear_pointer (&priv->font, g_object_unref);

  g_list_free_full (priv->drawable, g_free);
  g_list_free_full (priv->cancellable, g_free);
  g_list_free_full (priv->visible, g_free);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_meter_parent_class)->finalize (object);
}

/* Функция захвата ввода. */
static void
hyscan_gtk_waterfall_meter_grab_input (HyScanGtkWaterfallLayer *iface)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (iface);

  /* Мы не можем захватить ввод, если слой отключен. */
  if (!self->priv->layer_visibility)
    return;

  hyscan_gtk_waterfall_state_set_input_owner (self->priv->wf_state, self);
  hyscan_gtk_waterfall_state_set_changes_allowed (self->priv->wf_state, TRUE);
}

/* Функция захвата ввода. */
static void
hyscan_gtk_waterfall_meter_set_visible (HyScanGtkWaterfallLayer *iface,
                                        gboolean                 visible)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (iface);

  self->priv->layer_visibility = visible;
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция возвращает название иконки. */
static const gchar*
hyscan_gtk_waterfall_meter_get_mnemonic (HyScanGtkWaterfallLayer *iface)
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
  gdouble thres = HANDLE_RADIUS;
  GList *link, *mlink = NULL;
  gdouble rs, re, r;
  gdouble rmin = G_MAXDOUBLE;

  for (link = priv->visible; link != NULL; link = link->next)
    {
      HyScanGtkWaterfallMeterItem *drawn = link->data;

      rs = hyscan_gtk_waterfall_tools_distance(pointer, &drawn->px_start);
      re = hyscan_gtk_waterfall_tools_distance(pointer, &drawn->px_end);

      r = MIN (rs, re);
      if (r > rmin || r > thres)
        continue;

      rmin = r;
      mlink = link;
    }

  return mlink;
}

/* Функция определяет, есть ли под указателем хэндл. */
static gpointer
hyscan_gtk_waterfall_meter_handle (HyScanGtkWaterfallState *state,
                                   GdkEventButton          *event,
                                   HyScanGtkWaterfallMeter *self)
{
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;
  HyScanGtkWaterfallMeterItem *drawn;
  HyScanCoordinates mouse;
  gdouble re, rs;
  GList *link;

  mouse.x = event->x;
  mouse.y = event->y;

  /* Мы не можем обрабатывать действия, если слой отключен. */
  if (!self->priv->layer_visibility)
    return NULL;

  if (hyscan_gtk_waterfall_tools_distance (&priv->press, &mouse) > 2)
    return NULL;

  link = hyscan_gtk_waterfall_meter_find_closest (self, &mouse);

  /* Если ничего не найдено, выходим отсюда. */
  if (link == NULL)
    return NULL;

  /* Иначе определяем, конец или начало найдено. */
  drawn = link->data;

  rs = hyscan_gtk_waterfall_tools_distance (&mouse, &drawn->px_start);
  re = hyscan_gtk_waterfall_tools_distance (&mouse, &drawn->px_end);

  priv->current = *drawn;
  priv->current.start = (rs > re) ? drawn->start : drawn->end;
  priv->current.end   = (rs > re) ? drawn->end : drawn->start;
  priv->mode = LOCAL_EDIT;

  return self;
}

/* Функция обрабатывает нажатия клавиш клавиатуры. */
static gboolean
hyscan_gtk_waterfall_meter_key (GtkWidget               *widget,
                                GdkEventKey             *event,
                                HyScanGtkWaterfallMeter *self)
{
  if (self->priv->mode == LOCAL_EMPTY)
    return FALSE;

  if (event->keyval == GDK_KEY_Escape)
    self->priv->mode = LOCAL_CANCEL;
  else if (event->keyval == GDK_KEY_Delete)
    self->priv->mode = LOCAL_REMOVE;
  else
    return FALSE;

  return hyscan_gtk_waterfall_meter_interaction_processor (widget, self);
}

/* В эту функцию вынесена обработка нажатия. */
static gboolean
hyscan_gtk_waterfall_meter_button (GtkWidget               *widget,
                                   GdkEventButton          *event,
                                   HyScanGtkWaterfallMeter *self)
{
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  /* Игнорируем нажатия всех кнопок, кроме левой. */
  if (event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  /* Если это НАЖАТИЕ, то просто запомним координату. */
  if (event->type == GDK_BUTTON_PRESS)
    {
      self->priv->press.x = event->x;
      self->priv->press.y = event->y;
      return FALSE;
    }

  /* Игнорируем события двойного и тройного нажатия. */
  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

  priv->release.x = event->x;
  priv->release.y = event->y;

  /* Слишком большое перемещение говорит о том, что пользователь двигает область.  */
  if (hyscan_gtk_waterfall_tools_distance(&priv->press, &priv->release) > 2)
    return FALSE;

  return hyscan_gtk_waterfall_meter_interaction_processor (widget, self);
}

/* В эту функцию вынесена обработка нажатия. */
static gboolean
hyscan_gtk_waterfall_meter_interaction_processor (GtkWidget               *widget,
                                                  HyScanGtkWaterfallMeter *self)
{
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;
  /* Мы оказываемся в этой функции только когда функция
   * hyscan_gtk_waterfall_meter_interaction_resolver решила,
   * что мы имеем право обработать это воздействие.
   */
  /* Если сейчас не идет */
  if (priv->mode == LOCAL_EMPTY)
    {
      /* Устанавливаем режим. */
      priv->mode = LOCAL_CREATE;
      hyscan_gtk_waterfall_state_set_handle_grabbed (priv->wf_state, self);

      /* Запоминаем координаты начала. */
      gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (widget), priv->release.x, priv->release.y,
                                             &priv->current.start.x, &priv->current.start.y);
      priv->current.end.x = priv->current.start.x;
      priv->current.end.y = priv->current.start.y;
      priv->current.id = ++priv->last_id;
    }
  else if (priv->mode == LOCAL_EDIT)
    {
      GList *link = NULL;

      link = g_list_find_custom (priv->drawable, &priv->current, hyscan_gtk_waterfall_meter_find_by_id);
      priv->drawable = g_list_remove_link (priv->drawable, link);

      /* Сохраняем на случай если пользователю захочется вернуть как было. */
      priv->cancellable = link;

      /* Устанавливаем режим. */
      priv->mode = LOCAL_CREATE;
      hyscan_gtk_waterfall_state_set_handle_grabbed (priv->wf_state, self);
    }
  else if (priv->mode == LOCAL_CREATE)
    {
      HyScanGtkWaterfallMeterItem *new_item = g_new (HyScanGtkWaterfallMeterItem, 1);
      *new_item = priv->current;
      priv->drawable = g_list_append (priv->drawable, new_item);

      g_list_free_full (priv->cancellable, g_free);
      priv->cancellable = NULL;

      /* Сбрасываем режим. */
      priv->mode = LOCAL_EMPTY;
      hyscan_gtk_waterfall_state_set_handle_grabbed (priv->wf_state, NULL);
    }
  else if (priv->mode == LOCAL_CANCEL || priv->mode == LOCAL_REMOVE)
    {
      if (priv->mode == LOCAL_CANCEL && priv->cancellable != NULL)
        priv->drawable = g_list_concat (priv->drawable, priv->cancellable);

      priv->cancellable = NULL;

      priv->mode = LOCAL_EMPTY;
      hyscan_gtk_waterfall_state_set_handle_grabbed (priv->wf_state, NULL);
    }

  hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (priv->wf_state));
  return TRUE;
}

/* Функция определяет возможность обработки нажатия. */
static gboolean
hyscan_gtk_waterfall_meter_interaction_resolver (GtkWidget               *widget,
                                                 GdkEventAny             *event,
                                                 HyScanGtkWaterfallMeter *self)
{
  HyScanGtkWaterfallState *state = self->priv->wf_state;
  gconstpointer howner, iowner;

  /* Проверяем режим (просмотр/редактирование). */
  if (!hyscan_gtk_waterfall_state_get_changes_allowed (state))
    return FALSE;

  howner = hyscan_gtk_waterfall_state_get_handle_grabbed (state);
  iowner = hyscan_gtk_waterfall_state_get_input_owner (state);

  /* Можно обработать это взаимодействие пользователя в следующих случаях:
   * - howner - это мы (в данный момент идет обработка)
   * - под указателем есть хэндл, за который можно схватиться
   * - хэндла нет, но мы - iowner.
   */

  // if (!((howner == self) || (howner == NULL && iowner == self)))
  if ((howner != self) && (howner != NULL || iowner != self))
    return FALSE;

  /* Обработка кнопок мыши. */
  if (event->type == GDK_BUTTON_RELEASE)
    hyscan_gtk_waterfall_meter_button (widget, (GdkEventButton*)event, self);
  else if (event->type == GDK_KEY_PRESS)
    hyscan_gtk_waterfall_meter_key (widget, (GdkEventKey*)event, self);

  return TRUE;
}

/* Обработка движений мыши. */
static gboolean
hyscan_gtk_waterfall_meter_motion (GtkWidget               *widget,
                                   GdkEventMotion          *event,
                                   HyScanGtkWaterfallMeter *self)
{
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  if (priv->mode == LOCAL_CREATE)
    {
      /* Запоминаем текущие координаты. */
      gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (widget), event->x, event->y,
                                             &priv->current.end.x, &priv->current.end.y);
    }

  priv->pointer.x = event->x;
  priv->pointer.y = event->y;

  return FALSE;
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
  GtkCifroArea *carea = GTK_CIFRO_AREA (self->priv->wf_state);

  /* Определяем, видна ли эта линейка на экране. */
  visible = hyscan_gtk_waterfall_tools_line_in_square (&task->start, &task->end,
                                                       &priv->view._0,
                                                       &priv->view._1);

  /* Если нет, не рисуем её. */
  if (!visible)
    return FALSE;

  gtk_cifro_area_visible_value_to_point (carea, &start.x, &start.y, task->start.x, task->start.y);
  gtk_cifro_area_visible_value_to_point (carea, &end.x, &end.y, task->end.x, task->end.y);

  task->px_start = start;
  task->px_end = end;

  cairo_save (cairo);

  hyscan_gtk_waterfall_meter_draw_helper (cairo, &start, &end, &priv->color.shadow, priv->color.shadow_width);
  hyscan_gtk_waterfall_meter_draw_helper (cairo, &start, &end, &priv->color.meter, priv->color.meter_width);

  angle = hyscan_gtk_waterfall_tools_angle (&task->start, &task->end);
  dist = hyscan_gtk_waterfall_tools_distance(&task->start, &task->end);
  mid = hyscan_gtk_waterfall_tools_middle (&start, &end);
  dist_px = hyscan_gtk_waterfall_tools_distance(&start, &end);


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
hyscan_gtk_waterfall_meter_draw_handle  (cairo_t          *cairo,
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

  for (link = priv->drawable; link != NULL; link = link->next)
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

    priv->visible = g_list_reverse (priv->visible);

    if (priv->mode == LOCAL_EMPTY)
      {
        link = hyscan_gtk_waterfall_meter_find_closest (self, &priv->pointer);

        if (link != NULL)
          {
            gdouble rs, re;
            HyScanGtkWaterfallMeterItem *drawn = link->data;
            HyScanCoordinates *co;

            rs = hyscan_gtk_waterfall_tools_distance(&priv->pointer, &drawn->px_start);
            re = hyscan_gtk_waterfall_tools_distance(&priv->pointer, &drawn->px_end);

            if (rs < re)
              co = &drawn->px_start;
            else /*(re < thres)*/
              co = &drawn->px_end;

            hyscan_gtk_waterfall_meter_draw_handle (cairo, priv->color.meter, priv->color.shadow, *co, HANDLE_RADIUS);
          }
      }
  else if (priv->mode == LOCAL_CREATE)
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

  g_list_free_full (priv->drawable, g_free);
  priv->drawable = NULL;
  g_list_free_full (priv->cancellable, g_free);
  priv->cancellable = NULL;
  g_list_free_full (priv->visible, g_free);
  priv->visible = NULL;

  priv->mode = LOCAL_EMPTY;
}

/* Функция создает новый слой HyScanGtkWaterfallMeter. */
HyScanGtkWaterfallMeter*
hyscan_gtk_waterfall_meter_new (HyScanGtkWaterfall *waterfall)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_METER,
                       "waterfall", waterfall,
                       NULL);
}

/* Функция задает основной цвет. */
void
hyscan_gtk_waterfall_meter_set_main_color (HyScanGtkWaterfallMeter *self,
                                           GdkRGBA                  color)
{
   g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

   self->priv->color.meter = color;
   hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция устанавливает цвет подложки. */
void
hyscan_gtk_waterfall_meter_set_shadow_color (HyScanGtkWaterfallMeter *self,
                                             GdkRGBA                  color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

  self->priv->color.shadow = color;
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция задает цвет рамки. */
void
hyscan_gtk_waterfall_meter_set_frame_color (HyScanGtkWaterfallMeter *self,
                                            GdkRGBA                  color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

  self->priv->color.frame = color;
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция задает толщину основных линий. */
void
hyscan_gtk_waterfall_meter_set_meter_width (HyScanGtkWaterfallMeter *self,
                                            gdouble                  width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

  self->priv->color.meter_width = width;
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция задает толщину подложки. */
void
hyscan_gtk_waterfall_meter_set_shadow_width (HyScanGtkWaterfallMeter *self,
                                             gdouble                  width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

  self->priv->color.shadow_width = width;
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

static void
hyscan_gtk_waterfall_meter_interface_init (HyScanGtkWaterfallLayerInterface *iface)
{
  iface->grab_input = hyscan_gtk_waterfall_meter_grab_input;
  iface->set_visible = hyscan_gtk_waterfall_meter_set_visible;
  iface->get_mnemonic = hyscan_gtk_waterfall_meter_get_mnemonic;
}
