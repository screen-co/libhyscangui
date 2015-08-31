/*!
 * \file hyscan-gtk-area.c
 *
 * \brief Исходный файл виджета рабочей области
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
*/

#include "hyscan-gtk-area.h"


typedef struct HyScanGtkAreaPriv {               // Внутренние данные объекта.

  GtkWidget                 *central_area;       // Виджет рабочей области по центру.
  GtkWidget                 *left_expander;      // Кнопка скрытия / показа рабочей области слева.
  GtkWidget                 *left_area;          // Виджет скрытия / показа рабочей области слева.
  GtkWidget                 *right_area;         // Виджет скрытия / показа рабочей области справа.
  GtkWidget                 *right_expander;     // Кнопка скрытия / показа рабочей области справа.
  GtkWidget                 *top_expander;       // Кнопка скрытия / показа рабочей области сверху.
  GtkWidget                 *top_area;           // Виджет скрытия / показа рабочей области сверху.
  GtkWidget                 *bottom_expander;    // Кнопка скрытия / показа рабочей области снизу.
  GtkWidget                 *bottom_area;        // Виджет скрытия / показа рабочей области снизу.

} HyScanGtkAreaPriv;


#define HYSCAN_GTK_AREA_GET_PRIVATE( obj ) ( G_TYPE_INSTANCE_GET_PRIVATE( ( obj ), HYSCAN_TYPE_GTK_AREA, HyScanGtkAreaPriv ) )


static GObject* hyscan_gtk_area_object_constructor( GType g_type, guint n_construct_properties, GObjectConstructParam *construct_properties );

static GtkWidget* hyscan_gtk_area_left_arrow_image_new( void );
static GtkWidget* hyscan_gtk_area_right_arrow_image_new( void );
static GtkWidget* hyscan_gtk_area_up_arrow_image_new( void );
static GtkWidget* hyscan_gtk_area_down_arrow_image_new( void );

static gboolean hyscan_gtk_area_switch_left( GtkWidget *widget, GdkEvent *event, HyScanGtkAreaPriv *priv );
static gboolean hyscan_gtk_area_switch_right( GtkWidget *widget, GdkEvent *event, HyScanGtkAreaPriv *priv );
static gboolean hyscan_gtk_area_switch_top( GtkWidget *widget, GdkEvent *event, HyScanGtkAreaPriv *priv );
static gboolean hyscan_gtk_area_switch_bottom( GtkWidget *widget, GdkEvent *event, HyScanGtkAreaPriv *priv );


G_DEFINE_TYPE( HyScanGtkArea, hyscan_gtk_area, GTK_TYPE_GRID );


static void hyscan_gtk_area_class_init( HyScanGtkAreaClass *klass )
{

  GObjectClass *this_class = G_OBJECT_CLASS( klass );

  this_class->constructor = (gpointer)hyscan_gtk_area_object_constructor;

  g_type_class_add_private( klass, sizeof( HyScanGtkAreaPriv ) );

}


static void hyscan_gtk_area_init( HyScanGtkArea *area )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  priv->central_area = NULL;
  priv->left_expander = NULL;
  priv->left_area = NULL;
  priv->right_area = NULL;
  priv->right_expander = NULL;
  priv->top_expander = NULL;
  priv->top_area = NULL;
  priv->bottom_expander = NULL;
  priv->bottom_area = NULL;

}


static GObject* hyscan_gtk_area_object_constructor( GType g_type, guint n_construct_properties, GObjectConstructParam *construct_properties )
{

  GObject *area = G_OBJECT_CLASS( hyscan_gtk_area_parent_class )->constructor( g_type, n_construct_properties, construct_properties );

  hyscan_gtk_area_set_central( HYSCAN_GTK_AREA( area ), NULL );

  return area;

}


// Функция возвращает изображение со стрелкой влево.
static GtkWidget* hyscan_gtk_area_left_arrow_image_new( void )
{

  return gtk_image_new_from_icon_name( "pan-start-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR );

}


// Функция возвращает изображение со стрелкой вправо.
static GtkWidget* hyscan_gtk_area_right_arrow_image_new( void )
{

  return gtk_image_new_from_icon_name( "pan-end-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR );

}


// Функция возвращает изображение со стрелкой вверх.
static GtkWidget* hyscan_gtk_area_up_arrow_image_new( void )
{

  return gtk_image_new_from_icon_name( "pan-up-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR );

}


// Функция возвращает изображение со стрелкой вниз.
static GtkWidget* hyscan_gtk_area_down_arrow_image_new( void )
{

  return gtk_image_new_from_icon_name( "pan-down-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR );

}


// Функция переключает видимость рабочей области слева.
static gboolean hyscan_gtk_area_switch_left( GtkWidget *widget, GdkEvent *event, HyScanGtkAreaPriv *priv )
{

  gboolean revealed = gtk_revealer_get_reveal_child( GTK_REVEALER( priv->left_area ) );

  gtk_revealer_set_reveal_child( GTK_REVEALER( priv->left_area ), !revealed );

  gtk_container_remove( GTK_CONTAINER( priv->left_expander ), gtk_bin_get_child( GTK_BIN( priv->left_expander ) ) );
  if( revealed ) gtk_container_add( GTK_CONTAINER( priv->left_expander ), hyscan_gtk_area_right_arrow_image_new() );
  else gtk_container_add( GTK_CONTAINER( priv->left_expander ), hyscan_gtk_area_left_arrow_image_new() );
  gtk_widget_show_all( GTK_WIDGET( priv->left_expander ) );

  return TRUE;

}


// Функция переключает видимость рабочей области справа.
static gboolean hyscan_gtk_area_switch_right( GtkWidget *widget, GdkEvent *event, HyScanGtkAreaPriv *priv )
{

  gboolean revealed = gtk_revealer_get_reveal_child( GTK_REVEALER( priv->right_area ) );

  gtk_revealer_set_reveal_child( GTK_REVEALER( priv->right_area ), !revealed );

  gtk_container_remove( GTK_CONTAINER( priv->right_expander ), gtk_bin_get_child( GTK_BIN( priv->right_expander ) ) );
  if( revealed ) gtk_container_add( GTK_CONTAINER( priv->right_expander ), hyscan_gtk_area_left_arrow_image_new() );
  else gtk_container_add( GTK_CONTAINER( priv->right_expander ), hyscan_gtk_area_right_arrow_image_new() );
  gtk_widget_show_all( GTK_WIDGET( priv->right_expander ) );

  return TRUE;

}


// Функция переключает видимость рабочей области сверху.
static gboolean hyscan_gtk_area_switch_top( GtkWidget *widget, GdkEvent *event, HyScanGtkAreaPriv *priv )
{

  gboolean revealed = gtk_revealer_get_reveal_child( GTK_REVEALER( priv->top_area ) );

  gtk_revealer_set_reveal_child( GTK_REVEALER( priv->top_area ), !revealed );

  gtk_container_remove( GTK_CONTAINER( priv->top_expander ), gtk_bin_get_child( GTK_BIN( priv->top_expander ) ) );
  if( revealed ) gtk_container_add( GTK_CONTAINER( priv->top_expander ), hyscan_gtk_area_down_arrow_image_new() );
  else gtk_container_add( GTK_CONTAINER( priv->top_expander ), hyscan_gtk_area_up_arrow_image_new() );
  gtk_widget_show_all( GTK_WIDGET( priv->top_expander ) );

  return TRUE;

}


// Функция переключает видимость рабочей области снизу.
static gboolean hyscan_gtk_area_switch_bottom( GtkWidget *widget, GdkEvent *event, HyScanGtkAreaPriv *priv )
{

  gboolean revealed = gtk_revealer_get_reveal_child( GTK_REVEALER( priv->bottom_area ) );

  gtk_revealer_set_reveal_child( GTK_REVEALER( priv->bottom_area ), !revealed );

  gtk_container_remove( GTK_CONTAINER( priv->bottom_expander ), gtk_bin_get_child( GTK_BIN( priv->bottom_expander ) ) );
  if( revealed ) gtk_container_add( GTK_CONTAINER( priv->bottom_expander ), hyscan_gtk_area_up_arrow_image_new() );
  else gtk_container_add( GTK_CONTAINER( priv->bottom_expander ), hyscan_gtk_area_down_arrow_image_new() );
  gtk_widget_show_all( GTK_WIDGET( priv->bottom_expander ) );

  return TRUE;

}


GtkWidget *hyscan_gtk_area_new( void )
{

  return g_object_new( HYSCAN_TYPE_GTK_AREA, NULL );

}


// Функция устанавливает виджет рабочей области по центру.
void hyscan_gtk_area_set_central( HyScanGtkArea *area, GtkWidget *child )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  // Удаляем виджет из рабочей области по центру, если он установлен.
  if( priv->central_area != NULL )
    gtk_container_remove( GTK_CONTAINER( area ), priv->central_area );

  // Если дочерний виджет равен NULL, устанавливаем заглушку.
  if( child == NULL ) child = gtk_drawing_area_new();

  // Виджет рабочей области по центру.
  gtk_widget_set_vexpand( child, TRUE );
  gtk_widget_set_hexpand( child, TRUE );
  gtk_widget_set_valign( child, GTK_ALIGN_FILL );
  gtk_widget_set_halign( child, GTK_ALIGN_FILL );
  gtk_grid_attach( GTK_GRID( area ), child, 2, 2, 1, 1 );

}


// Функция устанавливает виджет рабочей области слева.
void hyscan_gtk_area_set_left( HyScanGtkArea *area, GtkWidget *child )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  // Удаляем виджет из рабочей области слева, если он установлен.
  if( priv->left_area != NULL )
    {
    gtk_container_remove( GTK_CONTAINER( area ), priv->left_area );
    gtk_container_remove( GTK_CONTAINER( area ), priv->left_expander );
    }

  // Если дочерний виджет равен NULL, выходим.
  if( child == NULL ) return;

  // Кнопка скрытия / показа рабочей области слева.
  priv->left_expander = gtk_event_box_new();
  gtk_container_add( GTK_CONTAINER( priv->left_expander ), hyscan_gtk_area_left_arrow_image_new() );
  gtk_widget_set_valign( priv->left_expander, GTK_ALIGN_FILL );
  gtk_widget_set_halign( priv->left_expander, GTK_ALIGN_FILL );
  gtk_grid_attach( GTK_GRID( area ), priv->left_expander, 0, 0, 1, 5 );

  /// Виджет скрытия / показа рабочей области слева.
  priv->left_area = gtk_revealer_new();
  gtk_revealer_set_transition_type( GTK_REVEALER( priv->left_area ), GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT );
  gtk_revealer_set_reveal_child( GTK_REVEALER( priv->left_area ), TRUE );
  gtk_container_add( GTK_CONTAINER( priv->left_area ), child );
  gtk_grid_attach( GTK_GRID( area ), priv->left_area, 1, 0, 1, 5 );

  // Обработчик нажатия кнопки скрытия / показа рабочей области слева.
  g_signal_connect( priv->left_expander, "button-press-event", G_CALLBACK( hyscan_gtk_area_switch_left ), priv );

}


// Функция устанавливает виджет рабочей области справа.
void hyscan_gtk_area_set_right( HyScanGtkArea *area, GtkWidget *child )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  // Удаляем виджет из рабочей области справа, если он установлен.
  if( priv->right_area != NULL )
    {
    gtk_container_remove( GTK_CONTAINER( area ), priv->right_area );
    gtk_container_remove( GTK_CONTAINER( area ), priv->right_expander );
    }

  // Если дочерний виджет равен NULL, выходим.
  if( child == NULL ) return;

  // Кнопка скрытия / показа рабочей области справа.
  priv->right_expander = gtk_event_box_new();
  gtk_container_add( GTK_CONTAINER( priv->right_expander ), hyscan_gtk_area_right_arrow_image_new() );
  gtk_widget_set_valign( priv->right_expander, GTK_ALIGN_FILL );
  gtk_widget_set_halign( priv->right_expander, GTK_ALIGN_FILL );
  gtk_grid_attach( GTK_GRID( area ), priv->right_expander, 4, 0, 1, 5 );

  /// Виджет скрытия / показа рабочей области справа.
  priv->right_area = gtk_revealer_new();
  gtk_revealer_set_transition_type( GTK_REVEALER( priv->right_area ), GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT );
  gtk_revealer_set_reveal_child( GTK_REVEALER( priv->right_area ), TRUE );
  gtk_container_add( GTK_CONTAINER( priv->right_area ), child );
  gtk_grid_attach( GTK_GRID( area ), priv->right_area, 3, 0, 1, 5 );

  // Обработчик нажатия кнопки скрытия / показа рабочей области справа.
  g_signal_connect( priv->right_expander, "button-press-event", G_CALLBACK( hyscan_gtk_area_switch_right ), priv );

}


// Функция устанавливает виджет рабочей области сверху.
void hyscan_gtk_area_set_top( HyScanGtkArea *area, GtkWidget *child )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  // Удаляем виджет из рабочей области сверху, если он установлен.
  if( priv->top_area != NULL )
    {
    gtk_container_remove( GTK_CONTAINER( area ), priv->top_area );
    gtk_container_remove( GTK_CONTAINER( area ), priv->top_expander );
    }

  // Если дочерний виджет равен NULL, выходим.
  if( child == NULL ) return;

  // Кнопка скрытия / показа рабочей области сверху.
  priv->top_expander = gtk_event_box_new();
  gtk_container_add( GTK_CONTAINER( priv->top_expander ), hyscan_gtk_area_up_arrow_image_new() );
  gtk_widget_set_valign( priv->top_expander, GTK_ALIGN_FILL );
  gtk_widget_set_halign( priv->top_expander, GTK_ALIGN_FILL );
  gtk_grid_attach( GTK_GRID( area ), priv->top_expander, 2, 0, 1, 1 );

  /// Виджет скрытия / показа рабочей области сверху.
  priv->top_area = gtk_revealer_new();
  gtk_revealer_set_transition_type( GTK_REVEALER( priv->top_area ), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP );
  gtk_revealer_set_reveal_child( GTK_REVEALER( priv->top_area ), TRUE );
  gtk_container_add( GTK_CONTAINER( priv->top_area ), child );
  gtk_grid_attach( GTK_GRID( area ), priv->top_area, 2, 1, 1, 1 );

  // Обработчик нажатия кнопки скрытия / показа рабочей области сверху.
  g_signal_connect( priv->top_expander, "button-press-event", G_CALLBACK( hyscan_gtk_area_switch_top ), priv );

}


// Функция устанавливает виджет рабочей области снизу.
void hyscan_gtk_area_set_bottom( HyScanGtkArea *area, GtkWidget *child )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  // Удаляем виджет из рабочей области снизу, если он установлен.
  if( priv->bottom_area != NULL )
    {
    gtk_container_remove( GTK_CONTAINER( area ), priv->bottom_area );
    gtk_container_remove( GTK_CONTAINER( area ), priv->bottom_expander );
    }

  // Если дочерний виджет равен NULL, выходим.
  if( child == NULL ) return;

  // Кнопка скрытия / показа рабочей области снизу.
  priv->bottom_expander = gtk_event_box_new();
  gtk_container_add( GTK_CONTAINER( priv->bottom_expander ), hyscan_gtk_area_down_arrow_image_new() );
  gtk_widget_set_valign( priv->bottom_expander, GTK_ALIGN_FILL );
  gtk_widget_set_halign( priv->bottom_expander, GTK_ALIGN_FILL );
  gtk_grid_attach( GTK_GRID( area ), priv->bottom_expander, 2, 4, 1, 1 );

  /// Виджет скрытия / показа рабочей области снизу.
  priv->bottom_area = gtk_revealer_new();
  gtk_revealer_set_transition_type( GTK_REVEALER( priv->bottom_area ), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN );
  gtk_revealer_set_reveal_child( GTK_REVEALER( priv->bottom_area ), TRUE );
  gtk_container_add( GTK_CONTAINER( priv->bottom_area ), child );
  gtk_grid_attach( GTK_GRID( area ), priv->bottom_area, 2, 3, 1, 1 );

  // Обработчик нажатия кнопки скрытия / показа рабочей области снизу.
  g_signal_connect( priv->bottom_expander, "button-press-event", G_CALLBACK( hyscan_gtk_area_switch_bottom ), priv );

}


// Функция изменяет видимость рабочей области слева.
void hyscan_gtk_area_set_left_visible( HyScanGtkArea *area, gboolean visible )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  // Если виджет рабочей области слева не установлен, выходим.
  if( priv->left_area == NULL ) return;

  // Проверяем совпадает состояние отображения рабочей области слева или нет.
  if( gtk_revealer_get_reveal_child( GTK_REVEALER( priv->left_area ) ) == visible ) return;

  // Переключаем состояние отображения рабочей области слева.
  hyscan_gtk_area_switch_left( NULL, NULL, priv );

}


// Функция возвращает состояние видимости рабочей области слева.
gboolean hyscan_gtk_area_is_left_visible( HyScanGtkArea *area )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  // Если виджет рабочей области слева не установлен, выходим.
  if( priv->left_area == NULL ) return FALSE;

  return gtk_revealer_get_reveal_child( GTK_REVEALER( priv->left_area ) );

}


// Функция изменяет видимость рабочей области справа.
void hyscan_gtk_area_set_right_visible( HyScanGtkArea *area, gboolean visible )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  // Если виджет рабочей области справа не установлен, выходим.
  if( priv->right_area == NULL ) return;

  // Проверяем совпадает состояние отображения рабочей области справа или нет.
  if( gtk_revealer_get_reveal_child( GTK_REVEALER( priv->right_area ) ) == visible ) return;

  // Переключаем состояние отображения рабочей области справа.
  hyscan_gtk_area_switch_right( NULL, NULL, priv );

}


// Функция возвращает состояние видимости рабочей области справа.
gboolean hyscan_gtk_area_is_right_visible( HyScanGtkArea *area )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  // Если виджет рабочей области справа не установлен, выходим.
  if( priv->right_area == NULL ) return FALSE;

  return gtk_revealer_get_reveal_child( GTK_REVEALER( priv->right_area ) );

}


// Функция изменяет видимость рабочей области сверху.
void hyscan_gtk_area_set_top_visible( HyScanGtkArea *area, gboolean visible )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  // Если виджет рабочей области сверху не установлен, выходим.
  if( priv->top_area == NULL ) return;

  // Проверяем совпадает состояние отображения рабочей области сверху или нет.
  if( gtk_revealer_get_reveal_child( GTK_REVEALER( priv->top_area ) ) == visible ) return;

  // Переключаем состояние отображения рабочей области сверху.
  hyscan_gtk_area_switch_top( NULL, NULL, priv );

}


// Функция возвращает состояние видимости рабочей области сверху.
gboolean hyscan_gtk_area_is_top_visible( HyScanGtkArea *area )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  // Если виджет рабочей области сверху не установлен, выходим.
  if( priv->top_area == NULL ) return FALSE;

  return gtk_revealer_get_reveal_child( GTK_REVEALER( priv->top_area ) );

}


// Функция изменяет видимость рабочей области снизу.
void hyscan_gtk_area_set_bottom_visible( HyScanGtkArea *area, gboolean visible )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  // Если виджет рабочей области снизу не установлен, выходим.
  if( priv->bottom_area == NULL ) return;

  // Проверяем совпадает состояние отображения рабочей области снизу или нет.
  if( gtk_revealer_get_reveal_child( GTK_REVEALER( priv->bottom_area ) ) == visible ) return;

  // Переключаем состояние отображения рабочей области снизу.
  hyscan_gtk_area_switch_bottom( NULL, NULL, priv );

}


// Функция возвращает состояние видимости рабочей области снизу.
gboolean hyscan_gtk_area_is_bottom_visible( HyScanGtkArea *area )
{

  HyScanGtkAreaPriv *priv = HYSCAN_GTK_AREA_GET_PRIVATE( area );

  // Если виджет рабочей области снизу не установлен, выходим.
  if( priv->bottom_area == NULL ) return FALSE;

  return gtk_revealer_get_reveal_child( GTK_REVEALER( priv->bottom_area ) );

}


// Функция изменяет видимость всех рабочих областей, кроме центральной.
void hyscan_gtk_area_set_all_visible( HyScanGtkArea *area, gboolean visible )
{

  hyscan_gtk_area_set_left_visible( area, visible );
  hyscan_gtk_area_set_right_visible( area, visible );
  hyscan_gtk_area_set_top_visible( area, visible );
  hyscan_gtk_area_set_bottom_visible( area, visible );

}
