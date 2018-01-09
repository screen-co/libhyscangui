/**
 * \file hyscan-gtk-waterfall-player.h
 *
 * \brief Плеер для водопада
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfallPlayer HyScanGtkWaterfallPlayer - плеер для водопада
 *
 * Данный слой реализует простой плеер для водопада. Он прокручивает изображение
 * с постоянной скоростью и эмиттирует сигнал "player-stop" когда прокручивать
 * изображение больше некуда.
 * Коллбэк сигнала:
 * \code
 * void player_stop_cb (HyScanGtkWaterfallPlayer *player,
 *                      gpointer                  user_data);
 * \endcode
 * В обработчике сигнала нельзя вызывать методы класса.
 *
 * Новый объект создается функцией #hyscan_gtk_waterfall_player_new,
 * настройка выполняется функциями #hyscan_gtk_waterfall_player_set_fps и
 * #hyscan_gtk_waterfall_player_set_speed.
 *
 * Класс не потокобезопасен и должен использоваться только из главного потока.
 *
 */

#ifndef __HYSCAN_GTK_WATERFALL_PLAYER_H__
#define __HYSCAN_GTK_WATERFALL_PLAYER_H__

#include <hyscan-gtk-waterfall-layer.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_PLAYER             (hyscan_gtk_waterfall_player_get_type ())
#define HYSCAN_GTK_WATERFALL_PLAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_PLAYER, HyScanGtkWaterfallPlayer))
#define HYSCAN_IS_GTK_WATERFALL_PLAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_PLAYER))
#define HYSCAN_GTK_WATERFALL_PLAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_PLAYER, HyScanGtkWaterfallPlayerClass))
#define HYSCAN_IS_GTK_WATERFALL_PLAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_PLAYER))
#define HYSCAN_GTK_WATERFALL_PLAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_PLAYER, HyScanGtkWaterfallPlayerClass))

typedef struct _HyScanGtkWaterfallPlayer HyScanGtkWaterfallPlayer;
typedef struct _HyScanGtkWaterfallPlayerPrivate HyScanGtkWaterfallPlayerPrivate;
typedef struct _HyScanGtkWaterfallPlayerClass HyScanGtkWaterfallPlayerClass;

struct _HyScanGtkWaterfallPlayer
{
  GObject parent_instance;

  HyScanGtkWaterfallPlayerPrivate *priv;
};

struct _HyScanGtkWaterfallPlayerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType           hyscan_gtk_waterfall_player_get_type    (void);

/**
 *
 * Функция создает новый объект \link HyScanGtkWaterfallPlayer \endlink
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 *
 * \return новый объект \link HyScanGtkWaterfallPlayer \endlink
 */
HYSCAN_API
HyScanGtkWaterfallPlayer* hyscan_gtk_waterfall_player_new (HyScanGtkWaterfall *waterfall);

/**
 *
 * Функция задает количество кадров в секунду
 *
 * Каждую секунду будет сделано именно столько сдвижек. Это позволяет настроить
 * плавность проигрывания. Реальное количество кадров в секунду может отличаться
 * от запрошенного.
 *
 * \param player указатель на объект \link HyScanGtkWaterfallPlayer \endlink
 * \param fps количество кадров в секунду
 */
HYSCAN_API
void            hyscan_gtk_waterfall_player_set_fps     (HyScanGtkWaterfallPlayer *player,
                                                         guint                     fps);

/**
 *
 * Функция задает скорость сдвижки.
 *
 * \param player указатель на объект \link HyScanGtkWaterfallPlayer \endlink
 * \param speed скорость сдвижки в м/с.
 */
HYSCAN_API
void            hyscan_gtk_waterfall_player_set_speed   (HyScanGtkWaterfallPlayer *player,
                                                         gdouble                   speed);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_PLAYER_H__ */
