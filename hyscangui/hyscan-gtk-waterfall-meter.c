#include "hyscan-gtk-waterfall-meter.h"
#include "hyscan-gtk-waterfall-tools.h"

enum
{
  PROP_WATERFALL = 1
};

enum
{
  EMPTY,
  CREATE,
  EDIT
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
  HyScanGtkWaterfallState      *wfall;
  PangoLayout                  *font;              /* Раскладка шрифта. */

  HyScanCoordinates press;
  HyScanCoordinates release;

  guint64                       last_id;
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

static gint     hyscan_gtk_waterfall_meter_find_by_id              (gconstpointer            a,
                                                                    gconstpointer            b);
static gpointer hyscan_gtk_waterfall_meter_handle                  (HyScanGtkWaterfallState *state,
                                                                    GdkEventButton          *event,
                                                                    HyScanGtkWaterfallMeter *self);

static gboolean hyscan_gtk_waterfall_meter_mouse_button_press      (GtkWidget               *widget,
                                                                    GdkEventButton          *event,
                                                                    HyScanGtkWaterfallMeter *self);
static gboolean hyscan_gtk_waterfall_meter_mouse_button_release    (GtkWidget               *widget,
                                                                    GdkEventButton          *event,
                                                                    HyScanGtkWaterfallMeter *self);
static gboolean hyscan_gtk_waterfall_meter_mouse_motion            (GtkWidget               *widget,
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
static void     hyscan_gtk_waterfall_meter_object_constructed      (GObject                 *object);
static void     hyscan_gtk_waterfall_meter_object_finalize         (GObject                 *object);

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
    g_param_spec_object ("waterfall", "Waterfall", "GtkWaterfall object", HYSCAN_TYPE_GTK_WATERFALL_STATE,
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
    self->priv->wfall = g_value_dup_object (value);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
hyscan_gtk_waterfall_meter_object_constructed (GObject *object)
{
  GdkRGBA color_meter, color_shadow;
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (object);
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  g_signal_connect (priv->wfall, "visible-draw",         G_CALLBACK (hyscan_gtk_waterfall_meter_draw), self);
  g_signal_connect (priv->wfall, "configure-event",      G_CALLBACK (hyscan_gtk_waterfall_meter_configure), self);
  g_signal_connect (priv->wfall, "button-press-event",   G_CALLBACK (hyscan_gtk_waterfall_meter_mouse_button_press), self);
  g_signal_connect (priv->wfall, "button-release-event", G_CALLBACK (hyscan_gtk_waterfall_meter_mouse_button_release), self);
  g_signal_connect (priv->wfall, "motion-notify-event",  G_CALLBACK (hyscan_gtk_waterfall_meter_mouse_motion), self);

  g_signal_connect (HYSCAN_GTK_WATERFALL_STATE (priv->wfall), "handle",  G_CALLBACK (hyscan_gtk_waterfall_meter_handle), self);

  gdk_rgba_parse (&color_meter, "yellow");
  gdk_rgba_parse (&color_shadow, "black");

  hyscan_gtk_waterfall_meter_set_meter_color  (self, color_meter);
  hyscan_gtk_waterfall_meter_set_shadow_color (self, color_shadow);
  hyscan_gtk_waterfall_meter_set_meter_width  (self, 3);
  hyscan_gtk_waterfall_meter_set_shadow_width (self, 5);
}

static void
hyscan_gtk_waterfall_meter_object_finalize (GObject *object)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (object);
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  g_clear_object (&priv->wfall);
  g_clear_pointer (&priv->font, g_object_unref);
  G_OBJECT_CLASS (hyscan_gtk_waterfall_meter_parent_class)->finalize (object);
}

static void
hyscan_gtk_waterfall_meter_grab_input (HyScanGtkWaterfallLayer *iface)
{
  HyScanGtkWaterfallMeter *self = HYSCAN_GTK_WATERFALL_METER (iface);

  hyscan_gtk_waterfall_state_set_input_owner (HYSCAN_GTK_WATERFALL_STATE (self->priv->wfall), self);
  hyscan_gtk_waterfall_state_set_changes_allowed (HYSCAN_GTK_WATERFALL_STATE (self->priv->wfall), TRUE);
}

static const gchar*
hyscan_gtk_waterfall_meter_get_mnemonic (HyScanGtkWaterfallLayer *iface)
{
  return "waterfall-meter";
}

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

static gpointer
hyscan_gtk_waterfall_meter_handle (HyScanGtkWaterfallState *state,
                                   GdkEventButton          *event,
                                   HyScanGtkWaterfallMeter *self)
{
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;
  HyScanCoordinates mouse;
  gdouble re, rs, thres;
  GList *link;

  thres = priv->color.shadow_width;
  mouse.x = event->x;
  mouse.y = event->y;

  if (hyscan_gtk_waterfall_tools_radius (&priv->press, &mouse) > 2)
    return NULL;

  for (link = g_list_last (priv->visible); link != NULL; link = link->prev)
    {
      HyScanGtkWaterfallMeterItem *drawn = link->data;

      rs = hyscan_gtk_waterfall_tools_radius (&mouse, &drawn->px_start);
      re = hyscan_gtk_waterfall_tools_radius (&mouse, &drawn->px_end);
      if (rs < thres || re < thres)
        {
          priv->current = *drawn;
          priv->current.start = (rs > re) ? drawn->start : drawn->end;
          priv->current.end   = (rs > re) ? drawn->end : drawn->start;
          priv->mode = EDIT;
          return self;
        }
    }

  return NULL;
}

/* В эту функцию вынесена обработка нажатия. */
static gboolean
hyscan_gtk_waterfall_meter_mouse_button_processor (GtkWidget               *widget,
                                                   GdkEventButton          *event,
                                                   HyScanGtkWaterfallMeter *self)
{
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;
  /* Мы оказываемся в этой функции только когда функция
   * hyscan_gtk_waterfall_meter_mouse_button_release решила,
   * что мы имеем право обработать это воздействие.
   */
  /* Если сейчас не идет */
  if (priv->mode == EMPTY)
    {
      /* Устанавливаем режим. */
      priv->mode = CREATE;
      hyscan_gtk_waterfall_state_set_handle_grabbed (HYSCAN_GTK_WATERFALL_STATE (widget), self);

      /* Запоминаем координаты начала. */
      gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (widget), event->x, event->y,
                                             &priv->current.start.x, &priv->current.start.y);
      priv->current.end.x = priv->current.start.x;
      priv->current.end.y = priv->current.start.y;
      priv->current.id = ++priv->last_id;
    }
  else if (priv->mode == EDIT)
    {
      GList *link;
      /* Устанавливаем режим. */
      priv->mode = CREATE;

      link = g_list_find_custom (priv->drawable, &priv->current, hyscan_gtk_waterfall_meter_find_by_id);
      priv->drawable = g_list_remove_link (priv->drawable, link);
      g_list_free (link);
    }
  else if (priv->mode == CREATE)
    {
      HyScanGtkWaterfallMeterItem *new;
      /* Сбрасываем режим. */
      priv->mode = EMPTY;
      hyscan_gtk_waterfall_state_set_handle_grabbed (HYSCAN_GTK_WATERFALL_STATE (widget), NULL);

      new = g_new (HyScanGtkWaterfallMeterItem, 1);
      *new = priv->current;
      priv->drawable = g_list_append (priv->drawable, new);
      hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (priv->wfall));
    }

  return FALSE;
}

static gboolean
hyscan_gtk_waterfall_meter_mouse_button_press (GtkWidget               *widget,
                                               GdkEventButton          *event,
                                               HyScanGtkWaterfallMeter *self)
{
  /* Просто запомним координату, где нажата кнопка мыши. */
  self->priv->press.x = event->x;
  self->priv->press.y = event->y;
  return FALSE;
}

static gboolean
hyscan_gtk_waterfall_meter_mouse_button_release (GtkWidget               *widget,
                                                 GdkEventButton          *event,
                                                 HyScanGtkWaterfallMeter *self)
{
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;
  gconstpointer howner, iowner;

  /* Игнорируем нажатия всех кнопок, кроме левой. */
  if (event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  /* Слишком большое перемещение говорит о том, что пользователь двигает область.  */
  priv->release.x = event->x;
  priv->release.y = event->y;
  if (hyscan_gtk_waterfall_tools_radius (&priv->press, &priv->release) > 2)
    return FALSE;

  /* Проверяем режим (просмотр/редактирование). */
  if (!hyscan_gtk_waterfall_state_get_changes_allowed (HYSCAN_GTK_WATERFALL_STATE (widget)))
    return FALSE;

  howner = hyscan_gtk_waterfall_state_get_handle_grabbed (HYSCAN_GTK_WATERFALL_STATE (widget));
  iowner = hyscan_gtk_waterfall_state_get_input_owner (HYSCAN_GTK_WATERFALL_STATE (widget));

  /* Можно обработать это взаимодействие пользователя в следующих случаях:
   * - howner - это мы (в данный момент идет обработка)
   * - под указателем есть хэндл, за который можно схватиться
   * - хэндла нет, но мы - iowner.
   */
  if (howner == self || (howner == NULL && iowner == self))
    {
      hyscan_gtk_waterfall_meter_mouse_button_processor (widget, event, self);
      return TRUE;
    }

  return FALSE;

  /* TODO: подумать, может, сделать интерфейс классом и вынести туда всё одинаковое,
   * как, например, этот метод. */
}

static gboolean
hyscan_gtk_waterfall_meter_mouse_motion (GtkWidget               *widget,
                                         GdkEventMotion          *event,
                                         HyScanGtkWaterfallMeter *self)
{
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

  if (priv->mode == CREATE)
    {
      /* Запоминаем текущие координаты. */
      gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (widget), event->x, event->y,
                                             &priv->current.end.x, &priv->current.end.y);
    }
  return FALSE;
}

static void
hyscan_gtk_waterfall_meter_draw_helper (cairo_t           *cairo,
                                        HyScanCoordinates *start,
                                        HyScanCoordinates *end,
                                        GdkRGBA           *color,
                                        gdouble            width)
{
  cairo_set_source_rgba (cairo, color->red, color->green, color->blue, color->alpha);

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

static gboolean
hyscan_gtk_waterfall_meter_draw_task (HyScanGtkWaterfallMeter     *self,
                                      cairo_t                     *cairo,
                                      HyScanGtkWaterfallMeterItem *task)
{
  HyScanCoordinates start, end, mid;
  gboolean visible;
  gdouble angle, dist;
  gchar *text;

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

  task->px_start = start;
  task->px_end = end;

  cairo_save (cairo);

  hyscan_gtk_waterfall_meter_draw_helper (cairo, &start, &end, &priv->color.shadow, priv->color.shadow_width);
  hyscan_gtk_waterfall_meter_draw_helper (cairo, &start, &end, &priv->color.meter, priv->color.meter_width);

  angle = hyscan_gtk_waterfall_tools_angle (&task->start, &task->end);
  dist = hyscan_gtk_waterfall_tools_radius (&task->start, &task->end);
  mid = hyscan_gtk_waterfall_tools_middle (&start, &end);

  cairo_move_to (cairo, mid.x, mid.y);
  cairo_rotate (cairo, angle);

  text = g_strdup_printf ("%3.2f m.", dist);
  pango_layout_set_text (priv->font, text, -1);
  g_free (text);

  pango_cairo_show_layout (cairo, priv->font);
  cairo_restore (cairo);

  return TRUE;
}

static void
hyscan_gtk_waterfall_meter_draw (GtkWidget               *widget,
                                 cairo_t                 *cairo,
                                 HyScanGtkWaterfallMeter *self)
{
  /* TODO: переделать, чтобы сначала рисовалась все подложки, потом все линейки
   * TODO: возможно, имеет смысл обновлять только когда реально поменялись параметры отображения
   * для этого надо ловить изменение вью/скейла, а на обычный мышевоз не реагировать
   */

  GList *link;
  HyScanGtkWaterfallMeterPrivate *priv = self->priv;

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
      HyScanGtkWaterfallMeterItem *new;
      HyScanGtkWaterfallMeterItem *task = link->data;

      /* Функция вернет FALSE, если метку рисовать не надо.
       * В противном случае её надо отправить в список видимых. */
      if (!hyscan_gtk_waterfall_meter_draw_task (self, cairo, task))
        continue;

      new = g_new (HyScanGtkWaterfallMeterItem, 1);
      *new = *task;
      priv->visible = g_list_prepend (priv->visible, new);
    }
  priv->visible = g_list_reverse (priv->visible);

  if (priv->mode == CREATE)
    hyscan_gtk_waterfall_meter_draw_task (self, cairo, &priv->current);

}

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

HyScanGtkWaterfallMeter*
hyscan_gtk_waterfall_meter_new (HyScanGtkWaterfallState *waterfall)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_METER,
                       "waterfall", waterfall,
                       NULL);
}

void
hyscan_gtk_waterfall_meter_set_meter_color (HyScanGtkWaterfallMeter *self,
                                            GdkRGBA                  color)
{
   g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

   self->priv->color.meter = color;
   hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (self->priv->wfall));
}

void
hyscan_gtk_waterfall_meter_set_shadow_color (HyScanGtkWaterfallMeter *self,
                                             GdkRGBA                  color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

  self->priv->color.shadow = color;
  hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (self->priv->wfall));
}
void
hyscan_gtk_waterfall_meter_set_meter_width (HyScanGtkWaterfallMeter *self,
                                            gdouble                  width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

  self->priv->color.meter_width = width;
  hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (self->priv->wfall));
}
void
hyscan_gtk_waterfall_meter_set_shadow_width (HyScanGtkWaterfallMeter *self,
                                             gdouble                  width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_METER (self));

  self->priv->color.shadow_width = width;
  hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (self->priv->wfall));
}

static void
hyscan_gtk_waterfall_meter_interface_init (HyScanGtkWaterfallLayerInterface *iface)
{
  iface->grab_input = hyscan_gtk_waterfall_meter_grab_input;
  iface->get_mnemonic = hyscan_gtk_waterfall_meter_get_mnemonic;
}
