/* hyscan-gtk-area.с
 *
 * Copyright 2015 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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
 * SECTION: hyscan-gtk-area
 * @Title HyScanGtkArea
 * @Short_description: виджет рабочей области
 *
 * Виджет представляет собой контейнер с пятью рабочими областями: центральной, левой,
 * правой, верхней и нижней. В каждую рабочую область можно разместить один дочерний виджет.
 * Для размещения нескольких виджетов необходимо использовать #GtkContainer, например #GtkBox или #GtkGrid.
 * Размещение дочерних виджетов осуществляется с помощью функций: hyscan_gtk_area_set_central(),
 * hyscan_gtk_area_set_left(), hyscan_gtk_area_set_right(), hyscan_gtk_area_set_top() и hyscan_gtk_area_set_bottom().
 *
 * Рабочие области вокруг центральной могут быть скрыты пользователем через графический интерфейс,
 * нажатием на соответствующие элементы управления (стрелки) или программно, с помощью функций:
 * hyscan_gtk_area_set_left_visible(), hyscan_gtk_area_set_right_visible(), hyscan_gtk_area_set_top_visible(),
 * hyscan_gtk_area_set_bottom_visible() и hyscan_gtk_area_set_all_visible().
 *
 * Определить отображаются рабочие области или нет можно с помощью функций: hyscan_gtk_area_is_left_visible(),
 * hyscan_gtk_area_is_right_visible(), hyscan_gtk_area_is_top_visible() и hyscan_gtk_area_is_bottom_visible().
 *
 */
#include "hyscan-gtk-area.h"

/* Внутренние данные объекта. */
struct _HyScanGtkArea
{
  GtkGrid              parent_instance;

  GtkWidget           *central_area;           /* Виджет рабочей области по центру. */
  GtkWidget           *left_expander;          /* Кнопка скрытия / показа рабочей области слева. */
  GtkWidget           *left_area;              /* Виджет скрытия / показа рабочей области слева. */
  GtkWidget           *right_area;             /* Виджет скрытия / показа рабочей области справа. */
  GtkWidget           *right_expander;         /* Кнопка скрытия / показа рабочей области справа. */
  GtkWidget           *top_expander;           /* Кнопка скрытия / показа рабочей области сверху. */
  GtkWidget           *top_area;               /* Виджет скрытия / показа рабочей области сверху. */
  GtkWidget           *bottom_expander;        /* Кнопка скрытия / показа рабочей области снизу. */
  GtkWidget           *bottom_area;            /* Виджет скрытия / показа рабочей области снизу. */
};


static void            hyscan_gtk_area_object_constructed      (GObject       *object);

static GtkWidget      *hyscan_gtk_area_left_arrow_image_new    (void);
static GtkWidget      *hyscan_gtk_area_right_arrow_image_new   (void);
static GtkWidget      *hyscan_gtk_area_up_arrow_image_new      (void);
static GtkWidget      *hyscan_gtk_area_down_arrow_image_new    (void);

static gboolean        hyscan_gtk_area_switch_left             (GtkWidget             *widget,
                                                                GdkEvent              *event,
                                                                HyScanGtkArea         *area);
static gboolean        hyscan_gtk_area_switch_right            (GtkWidget             *widget,
                                                                GdkEvent              *event,
                                                                HyScanGtkArea         *area);
static gboolean        hyscan_gtk_area_switch_top              (GtkWidget             *widget,
                                                                GdkEvent              *event,
                                                                HyScanGtkArea         *area);
static gboolean        hyscan_gtk_area_switch_bottom           (GtkWidget             *widget,
                                                                GdkEvent              *event,
                                                                HyScanGtkArea         *area);

G_DEFINE_TYPE (HyScanGtkArea, hyscan_gtk_area, GTK_TYPE_GRID);

static void
hyscan_gtk_area_class_init (HyScanGtkAreaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );

  object_class->constructed = hyscan_gtk_area_object_constructed;
}

static void
hyscan_gtk_area_init (HyScanGtkArea *area)
{
  area->central_area = NULL;
  area->left_expander = NULL;
  area->left_area = NULL;
  area->right_area = NULL;
  area->right_expander = NULL;
  area->top_expander = NULL;
  area->top_area = NULL;
  area->bottom_expander = NULL;
  area->bottom_area = NULL;
}

static void
hyscan_gtk_area_object_constructed (GObject *object)
{
  HyScanGtkArea *area = HYSCAN_GTK_AREA (object);

  hyscan_gtk_area_set_central (area, NULL);
}

/* Функция возвращает изображение со стрелкой влево. */
static GtkWidget *
hyscan_gtk_area_left_arrow_image_new (void)
{
  return gtk_image_new_from_icon_name ("pan-start-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
}

/* Функция возвращает изображение со стрелкой вправо. */
static GtkWidget *
hyscan_gtk_area_right_arrow_image_new (void)
{
  return gtk_image_new_from_icon_name ("pan-end-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
}

/* Функция возвращает изображение со стрелкой вверх. */
static GtkWidget *
hyscan_gtk_area_up_arrow_image_new (void)
{
  return gtk_image_new_from_icon_name ("pan-up-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
}

/* Функция возвращает изображение со стрелкой вниз. */
static GtkWidget *
hyscan_gtk_area_down_arrow_image_new (void)
{
  return gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
}

/* Функция переключает видимость рабочей области слева. */
static gboolean
hyscan_gtk_area_switch_left (GtkWidget     *widget,
                             GdkEvent      *event,
                             HyScanGtkArea *area)
{
  gboolean revealed = gtk_revealer_get_reveal_child (GTK_REVEALER (area->left_area));

  gtk_revealer_set_reveal_child (GTK_REVEALER (area->left_area), !revealed);

  gtk_container_remove (GTK_CONTAINER( area->left_expander ),
                        gtk_bin_get_child (GTK_BIN( area->left_expander )));

  if (revealed)
    {
      gtk_container_add (GTK_CONTAINER( area->left_expander ),
                         hyscan_gtk_area_right_arrow_image_new ());
    }
  else
    {
      gtk_container_add (GTK_CONTAINER( area->left_expander ),
                         hyscan_gtk_area_left_arrow_image_new ());
    }

  gtk_widget_show_all (GTK_WIDGET( area->left_expander ));

  return TRUE;
}

/* Функция переключает видимость рабочей области справа. */
static gboolean
hyscan_gtk_area_switch_right (GtkWidget     *widget,
                              GdkEvent      *event,
                              HyScanGtkArea *area)
{
  gboolean revealed = gtk_revealer_get_reveal_child (GTK_REVEALER (area->right_area));

  gtk_revealer_set_reveal_child (GTK_REVEALER (area->right_area), !revealed);

  gtk_container_remove (GTK_CONTAINER( area->right_expander ),
                        gtk_bin_get_child (GTK_BIN( area->right_expander )));

  if (revealed)
    {
      gtk_container_add (GTK_CONTAINER( area->right_expander ),
                         hyscan_gtk_area_left_arrow_image_new ());
    }
  else
    {
      gtk_container_add (GTK_CONTAINER( area->right_expander ),
                         hyscan_gtk_area_right_arrow_image_new ());
    }

  gtk_widget_show_all (GTK_WIDGET( area->right_expander ));

  return TRUE;
}

/* Функция переключает видимость рабочей области сверху. */
static gboolean
hyscan_gtk_area_switch_top (GtkWidget     *widget,
                            GdkEvent      *event,
                            HyScanGtkArea *area)
{
  gboolean revealed = gtk_revealer_get_reveal_child (GTK_REVEALER (area->top_area));

  gtk_revealer_set_reveal_child (GTK_REVEALER (area->top_area), !revealed);

  gtk_container_remove (GTK_CONTAINER( area->top_expander ),
                        gtk_bin_get_child (GTK_BIN( area->top_expander )));

  if (revealed)
    {
      gtk_container_add (GTK_CONTAINER( area->top_expander ),
                         hyscan_gtk_area_down_arrow_image_new ());
    }
  else
    {
      gtk_container_add (GTK_CONTAINER( area->top_expander ),
                         hyscan_gtk_area_up_arrow_image_new ());
    }

  gtk_widget_show_all (GTK_WIDGET( area->top_expander ));

  return TRUE;
}

/* Функция переключает видимость рабочей области снизу. */
static gboolean
hyscan_gtk_area_switch_bottom (GtkWidget     *widget,
                               GdkEvent      *event,
                               HyScanGtkArea *area)
{
  gboolean revealed = gtk_revealer_get_reveal_child (GTK_REVEALER (area->bottom_area));

  gtk_revealer_set_reveal_child (GTK_REVEALER (area->bottom_area), !revealed);

  gtk_container_remove (GTK_CONTAINER( area->bottom_expander ),
                        gtk_bin_get_child (GTK_BIN( area->bottom_expander )));

  if (revealed)
    {
      gtk_container_add (GTK_CONTAINER( area->bottom_expander ),
                         hyscan_gtk_area_up_arrow_image_new ());
    }
  else
    {
      gtk_container_add (GTK_CONTAINER( area->bottom_expander ),
                         hyscan_gtk_area_down_arrow_image_new ());
    }

  gtk_widget_show_all (GTK_WIDGET( area->bottom_expander ));

  return TRUE;
}

/**
 * hyscan_gtk_area_new:
 * Функция создаёт новый виджет #HyScanGtkArea
 *
 * Returns: Указатель на новый виджет #HyScanGtkArea
 */
GtkWidget *
hyscan_gtk_area_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_AREA, NULL);
}

/**
 * hyscan_gtk_area_set_central:
 * @area: #HyScanGtkArea
 * @child: дочерний виджет.
 *
 * Функция задаёт дочерний виджет центральной рабочей области.
 */
void
hyscan_gtk_area_set_central (HyScanGtkArea *area,
                             GtkWidget     *child)
{
  /* Удаляем виджет из рабочей области по центру, если он установлен. */
  if (area->central_area != NULL)
    gtk_container_remove (GTK_CONTAINER( area ), area->central_area);

  /* Если дочерний виджет равен NULL, устанавливаем заглушку. */
  if (child == NULL)
    child = gtk_drawing_area_new ();

  /* Виджет рабочей области по центру. */
  gtk_widget_set_vexpand (child, TRUE);
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_valign (child, GTK_ALIGN_FILL);
  gtk_widget_set_halign (child, GTK_ALIGN_FILL);
  gtk_grid_attach (GTK_GRID (area), child, 2, 2, 1, 1);
  area->central_area = child;
}

/**
 * hyscan_gtk_area_set_left:
 * @area: #HyScanGtkArea
 * @child: дочерний виджет.
 *
 * Функция задаёт дочерний виджет рабочей области слева.
 */
void
hyscan_gtk_area_set_left (HyScanGtkArea *area,
                          GtkWidget     *child)
{
  /* Удаляем виджет из рабочей области слева, если он установлен. */
  if (area->left_area != NULL)
    {
      gtk_container_remove (GTK_CONTAINER( area ), area->left_area);
      gtk_container_remove (GTK_CONTAINER( area ), area->left_expander);
    }

  if (child == NULL)
    return;

  /* Кнопка скрытия / показа рабочей области слева. */
  area->left_expander = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER( area->left_expander ),
                     hyscan_gtk_area_left_arrow_image_new ());
  gtk_widget_set_valign (area->left_expander, GTK_ALIGN_FILL);
  gtk_widget_set_halign (area->left_expander, GTK_ALIGN_FILL);
  gtk_grid_attach (GTK_GRID (area), area->left_expander, 0, 0, 1, 5);

  /* Виджет скрытия / показа рабочей области слева. */
  area->left_area = gtk_revealer_new ();
  gtk_revealer_set_transition_type (GTK_REVEALER (area->left_area),
                                    GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
  gtk_revealer_set_reveal_child (GTK_REVEALER (area->left_area), TRUE);
  gtk_container_add (GTK_CONTAINER( area->left_area ), child);
  gtk_grid_attach (GTK_GRID (area), area->left_area, 1, 0, 1, 5);

  /* Обработчик нажатия кнопки скрытия / показа рабочей области слева. */
  g_signal_connect (area->left_expander, "button-press-event",
                    G_CALLBACK( hyscan_gtk_area_switch_left ), area);
}

/**
 * hyscan_gtk_area_set_right:
 * @area: #HyScanGtkArea
 * @child: дочерний виджет.
 *
 * Функция задаёт дочерний виджет рабочей области справа.
 */
void
hyscan_gtk_area_set_right (HyScanGtkArea *area,
                           GtkWidget     *child)
{
  /* Удаляем виджет из рабочей области справа, если он установлен. */
  if (area->right_area != NULL)
    {
      gtk_container_remove (GTK_CONTAINER( area ), area->right_area);
      gtk_container_remove (GTK_CONTAINER( area ), area->right_expander);
    }

  if (child == NULL)
    return;

  /* Кнопка скрытия / показа рабочей области справа. */
  area->right_expander = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER( area->right_expander ),
                     hyscan_gtk_area_right_arrow_image_new ());
  gtk_widget_set_valign (area->right_expander, GTK_ALIGN_FILL);
  gtk_widget_set_halign (area->right_expander, GTK_ALIGN_FILL);
  gtk_grid_attach (GTK_GRID (area), area->right_expander, 4, 0, 1, 5);

  /* Виджет скрытия / показа рабочей области справа. */
  area->right_area = gtk_revealer_new ();
  gtk_revealer_set_transition_type (GTK_REVEALER (area->right_area),
                                    GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
  gtk_revealer_set_reveal_child (GTK_REVEALER (area->right_area), TRUE);
  gtk_container_add (GTK_CONTAINER( area->right_area ), child);
  gtk_grid_attach (GTK_GRID (area), area->right_area, 3, 0, 1, 5);

  /* Обработчик нажатия кнопки скрытия / показа рабочей области справа. */
  g_signal_connect (area->right_expander, "button-press-event",
                    G_CALLBACK( hyscan_gtk_area_switch_right ), area);

}

/**
 * hyscan_gtk_area_set_top:
 * @area: #HyScanGtkArea
 * @child: дочерний виджет.
 *
 * Функция задаёт дочерний виджет рабочей области сверху.
 */
void
hyscan_gtk_area_set_top (HyScanGtkArea *area,
                         GtkWidget     *child)
{
  /* Удаляем виджет из рабочей области сверху, если он установлен. */
  if (area->top_area != NULL)
    {
      gtk_container_remove (GTK_CONTAINER( area ), area->top_area);
      gtk_container_remove (GTK_CONTAINER( area ), area->top_expander);
    }

  if (child == NULL)
    return;

  /* Кнопка скрытия / показа рабочей области сверху. */
  area->top_expander = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER( area->top_expander ),
                     hyscan_gtk_area_up_arrow_image_new ());
  gtk_widget_set_valign (area->top_expander, GTK_ALIGN_FILL);
  gtk_widget_set_halign (area->top_expander, GTK_ALIGN_FILL);
  gtk_grid_attach (GTK_GRID (area), area->top_expander, 2, 0, 1, 1);

  /* Виджет скрытия / показа рабочей области сверху. */
  area->top_area = gtk_revealer_new ();
  gtk_revealer_set_transition_type (GTK_REVEALER (area->top_area),
                                    GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
  gtk_revealer_set_reveal_child (GTK_REVEALER (area->top_area), TRUE);
  gtk_container_add (GTK_CONTAINER( area->top_area ), child);
  gtk_grid_attach (GTK_GRID (area), area->top_area, 2, 1, 1, 1);

  /* Обработчик нажатия кнопки скрытия / показа рабочей области сверху. */
  g_signal_connect (area->top_expander, "button-press-event",
                    G_CALLBACK( hyscan_gtk_area_switch_top ), area);
}

/**
 * hyscan_gtk_area_set_bottom:
 * @area: #HyScanGtkArea
 * @child: дочерний виджет.
 *
 * Функция задаёт дочерний виджет рабочей области снизу.
 */
void
hyscan_gtk_area_set_bottom (HyScanGtkArea *area,
                            GtkWidget     *child)
{
  /* Удаляем виджет из рабочей области снизу, если он установлен. */
  if (area->bottom_area != NULL)
    {
      gtk_container_remove (GTK_CONTAINER( area ), area->bottom_area);
      gtk_container_remove (GTK_CONTAINER( area ), area->bottom_expander);
    }

  if (child == NULL)
    return;

  /* Кнопка скрытия / показа рабочей области снизу. */
  area->bottom_expander = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER( area->bottom_expander ),
                     hyscan_gtk_area_down_arrow_image_new ());
  gtk_widget_set_valign (area->bottom_expander, GTK_ALIGN_FILL);
  gtk_widget_set_halign (area->bottom_expander, GTK_ALIGN_FILL);
  gtk_grid_attach (GTK_GRID (area), area->bottom_expander, 2, 4, 1, 1);

  /* Виджет скрытия / показа рабочей области снизу. */
  area->bottom_area = gtk_revealer_new ();
  gtk_revealer_set_transition_type (GTK_REVEALER (area->bottom_area),
                                    GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
  gtk_revealer_set_reveal_child (GTK_REVEALER (area->bottom_area), TRUE);
  gtk_container_add (GTK_CONTAINER( area->bottom_area ), child);
  gtk_grid_attach (GTK_GRID (area), area->bottom_area, 2, 3, 1, 1);

  /* Обработчик нажатия кнопки скрытия / показа рабочей области снизу. */
  g_signal_connect (area->bottom_expander, "button-press-event",
                    G_CALLBACK( hyscan_gtk_area_switch_bottom ), area);
}

/**
 * hyscan_gtk_area_set_left_visible:
 * @area: #HyScanGtkArea
 * @visible: показывать или нет рабочую область слева.
 *
 * Функция устанавливает видимость рабочей области слева.
 */
void
hyscan_gtk_area_set_left_visible (HyScanGtkArea *area,
                                  gboolean       visible)
{
  if (area->left_area == NULL)
    return;

  /* Проверяем совпадает состояние отображения рабочей области слева или нет. */
  if (gtk_revealer_get_reveal_child (GTK_REVEALER (area->left_area)) == visible)
    return;

  /* Переключаем состояние отображения рабочей области слева. */
  hyscan_gtk_area_switch_left (NULL, NULL, area);
}

/**
 * hyscan_gtk_area_is_left_visible:
 * @area: #HyScanGtkArea
 *
 * Функция возвращает состояние видимости рабочей области слева.
 *
 * Returns: Состояние видимости рабочей области слева.
 */
gboolean
hyscan_gtk_area_is_left_visible (HyScanGtkArea *area)
{
  if (area->left_area == NULL)
    return FALSE;

  return gtk_revealer_get_reveal_child (GTK_REVEALER (area->left_area));
}

/**
 * hyscan_gtk_area_set_right_visible:
 * @area: #HyScanGtkArea
 * @visible: показывать или нет рабочую область справа.
 *
 * Функция устанавливает видимость рабочей области справа.
 */
void
hyscan_gtk_area_set_right_visible (HyScanGtkArea *area, gboolean visible)
{
  if (area->right_area == NULL)
    return;

  /* Проверяем совпадает состояние отображения рабочей области справа или нет. */
  if (gtk_revealer_get_reveal_child (GTK_REVEALER (area->right_area)) == visible)
    return;

  /* Переключаем состояние отображения рабочей области справа. */
  hyscan_gtk_area_switch_right (NULL, NULL, area);
}

/**
 * hyscan_gtk_area_is_right_visible:
 * @area: #HyScanGtkArea
 *
 * Функция возвращает состояние видимости рабочей области справа.
 *
 * Returns: Состояние видимости рабочей области справа.
 */
gboolean
hyscan_gtk_area_is_right_visible (HyScanGtkArea *area)
{
  if (area->right_area == NULL)
    return FALSE;

  return gtk_revealer_get_reveal_child (GTK_REVEALER (area->right_area));
}

/**
 * hyscan_gtk_area_set_top_visible:
 * @area: #HyScanGtkArea
 * @visible: показывать или нет рабочую область сверху.
 *
 * Функция устанавливает видимость рабочей области сверху.
 */
void
hyscan_gtk_area_set_top_visible (HyScanGtkArea *area,
                                 gboolean       visible)
{
  if (area->top_area == NULL)
    return;

  /* Проверяем совпадает состояние отображения рабочей области сверху или нет. */
  if (gtk_revealer_get_reveal_child (GTK_REVEALER (area->top_area)) == visible)
    return;

  /* Переключаем состояние отображения рабочей области сверху. */
  hyscan_gtk_area_switch_top (NULL, NULL, area);
}

/**
 * hyscan_gtk_area_is_top_visible:
 * @area: #HyScanGtkArea
 *
 * Функция возвращает состояние видимости рабочей области сверху.
 *
 * Returns: Состояние видимости рабочей области сверху.
 */
gboolean
hyscan_gtk_area_is_top_visible (HyScanGtkArea *area)
{
  if (area->top_area == NULL)
    return FALSE;

  return gtk_revealer_get_reveal_child (GTK_REVEALER (area->top_area));
}

/**
 * hyscan_gtk_area_set_bottom_visible:
 * @area: #HyScanGtkArea
 * @visible: показывать или нет рабочую область снизу.
 *
 * Функция устанавливает видимость рабочей области снизу.
 */
void
hyscan_gtk_area_set_bottom_visible (HyScanGtkArea *area,
                                    gboolean       visible)
{
  if (area->bottom_area == NULL)
    return;

  /* Проверяем совпадает состояние отображения рабочей области снизу или нет. */
  if (gtk_revealer_get_reveal_child (GTK_REVEALER (area->bottom_area)) == visible)
    return;

  /* Переключаем состояние отображения рабочей области снизу. */
  hyscan_gtk_area_switch_bottom (NULL, NULL, area);
}

/**
 * hyscan_gtk_area_is_bottom_visible:
 * @area: #HyScanGtkArea
 *
 * Функция возвращает состояние видимости рабочей области снизу.
 *
 * Returns: Состояние видимости рабочей области снизу.
 */
gboolean
hyscan_gtk_area_is_bottom_visible (HyScanGtkArea *area)
{
  if (area->bottom_area == NULL)
    return FALSE;

  return gtk_revealer_get_reveal_child (GTK_REVEALER (area->bottom_area));
}

/**
 * hyscan_gtk_area_set_all_visible:
 * @area: #HyScanGtkArea
 * @visible: показывать или нет рабочие области.
 *
 * Функция устанавливает видимость всех рабочих областей, кроме центральной.
 */
void
hyscan_gtk_area_set_all_visible (HyScanGtkArea *area,
                                 gboolean       visible)
{
  hyscan_gtk_area_set_left_visible (area, visible);
  hyscan_gtk_area_set_right_visible (area, visible);
  hyscan_gtk_area_set_top_visible (area, visible);
  hyscan_gtk_area_set_bottom_visible (area, visible);
}
