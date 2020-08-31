/* hyscan-gtk-mark-export.c
 *
 * Copyright 2019 Screen LLC, Andrey Zakharov <zaharov@screen-co.ru>
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
 * SECTION: hyscan-gtk-mark-export
 * @Short_description: Библиотека с функциями сохранения меток в форматах CSV и HTML,
 * и их копирования в буфер обмена.
 * @Title: HyScanGtkMarkExport
 * @See_also: #HyScanGtkMarkManager
 *
 * - hyscan_gtk_mark_export_save_as_csv () - сохранение меток в формате CSV;
 * - hyscan_gtk_mark_export_copy_to_clipboard () - копирование меток в буфер обмена;
 * - hyscan_gtk_mark_export_to_str () - получение информации о метках в виде строки;
 * - hyscan_gtk_mark_export_save_as_html () - сохранение меток в формате HTML.
 *
 * После сохранения графической и текстовой информации в формате HTML, можно
 * сконвертировать данные в различные форматы для дальнейшей обработки.
 *
 * Microsoft Word (DOC, DOCX, RTF, PDF)
 *
 * Чтобы конвертировать этот файл в doc, необходимо открыть его в Microsoft Word-е.
 * Выполните Правый клик -> Контекстное меню -> Открыть с помощью -> Microsoft Word.
 * Если Word-a в открывшемся списке нет, то нужно "Выбрать программу..." и там найти
 * Word. Тогда файл откроется. После того как файл открылся Выберете в меню Файл ->
 * Сведения -> Связаные документы -> Изменить связи с файлами. И в открывшемся окне
 * для каждого файла поставить галочку в Параметры связи -> Хранить в документе и
 * нажать OK. Затем выбрать Cохранить как и укажите нужный формат DOC, DOCX, RTF, PDF.
 * Всё файл сконвертирован.
 *
 * LibreOffice Writer (ODT, DOC, DOCX, RTF, PDF)
 *
 * Чтобы сконвертировать этот файл в odt, необходимо открыть его в LibreOffice Writer.
 * Выполните Правый клик -> Контекстное меню -> Открыть в программе -> LibreOffice
 * Writer. в меню выберите Правка -> Связи и для каждого файла нажать Разорвать связи.
 * Затем можно сохранять файл в нужном формате ODT, DOC, DOCX, RTF. Для сохранения в
 * формате PDF Выберите в меню Файл -> Экспорт в PDF. Всё файл сконвертирован.
 */

#include <hyscan-gtk-mark-export.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdk.h>
#include <sys/stat.h>
#include <hyscan-mark.h>
#include <hyscan-db-info.h>
#include <hyscan-tile-color.h>
#include <math.h>
#include <stdint.h>
#include <glib/gi18n-lib.h>

/* Структура для передачи данных в поток сохранения меток в формате HTML. */
typedef struct _DataForHTML
{
  GHashTable   *acoustic_marks, /* Акустические метки. */
               *geo_marks;      /* Гео-метки. */
  GtkWindow    *toplevel;       /* Главное окно. */
  const gchar  *project_name;   /* Название проекта. */
  HyScanDB     *db;             /* Указатель на базу данных. */
  HyScanCache  *cache;          /* Указатель на кэш.  */
  gchar        *folder;         /* Папка для экспорта. */
  GdkRGBA       color;          /* Цветовая схема. */
}DataForHTML;

/* Структура для передачи данных для сохранения тайла после генерации. */
typedef struct _package
{
  GMutex       mutex;           /* Мьютекс для защиты данных во сохранения тайла. */
  HyScanCache *cache;           /* Указатель на кэш. */
  GdkRGBA      color;           /* Цветовая схема. */
  guint        counter;         /* Счётчик тайлов. */

}Package;

static gchar hyscan_gtk_mark_export_header[] = "LAT,LON,NAME,DESCRIPTION,COMMENT,NOTES,DATE,TIME\n";

static gchar *empty = N_("Empty");

static gchar *unknown = N_("Unknown");

static gchar *link_to_site = N_("Generated: <a href=\"http://screen-co.ru/\">HyScan5</a>.");

static gchar *time_stamp = "%d.%m.%Y %H:%M:%S";

static gchar *sys_coord = "WGS 84";
/* Cкорость движения при которой генерируются тайлы в Echosounder-е, но метка
 * сохраняется в базе данных без учёта этого коэфициента масштабирования. */
static gdouble ship_speed = 10.0;

static void          hyscan_gtk_mark_export_tile_loaded         (Package             *package,
                                                                 HyScanTile          *tile,
                                                                 gfloat              *img,
                                                                 gint                 size,
                                                                 gulong               hash);

static void          hyscan_gtk_mark_export_save_tile_as_png    (HyScanTile          *tile,
                                                                 Package             *package,
                                                                 gfloat              *img,
                                                                 gint                 size,
                                                                 const gchar         *file_name,
                                                                 gboolean             echo);

static void          hyscan_gtk_mark_export_generate_tile       (HyScanMarkLocation  *location,
                                                                 HyScanTileQueue     *tile_queue,
                                                                 guint               *counter);

static void          hyscan_gtk_mark_export_save_tile           (HyScanMarkLocation  *location,
                                                                 GDateTime           *track_ctime,
                                                                 HyScanTileQueue     *tile_queue,
                                                                 const gchar         *image_folder,
                                                                 const gchar         *media,
                                                                 const gchar         *project_name,
                                                                 const gchar         *id,
                                                                 FILE                *file,
                                                                 Package             *package);

static void          hyscan_gtk_mark_export_init_tile           (HyScanTile          *tile,
                                                                 HyScanTileCacheable *tile_cacheable,
                                                                 HyScanMarkLocation  *location);

static gpointer      hyscan_gtk_mark_export_save_as_html_thread (gpointer             user_data);

static gboolean      hyscan_gtk_mark_export_set_watch_cursor    (gpointer             user_data);

static gboolean      hyscan_gtk_mark_export_set_default_cursor  (gpointer             user_data);

static gdouble       hyscan_gtk_mark_export_get_wfmark_width    (HyScanMarkLocation  *location);

/* Функция генерации строки. */
static gchar *
hyscan_gtk_mark_export_formatter (gdouble      lat,
                                  gdouble      lon,
                                  gint64       unixtime,
                                  const gchar *name,
                                  const gchar *description,
                                  const gchar *comment,
                                  const gchar *notes)
{
  gchar *str, *date, *time;
  gchar lat_str[128];
  gchar lon_str[128];
  GDateTime *dt = NULL;

  dt = g_date_time_new_from_unix_local (1e-6 * unixtime);

  if (dt == NULL)
    return NULL;

  date = g_date_time_format (dt, "%m/%d/%Y");
  time = g_date_time_format (dt, "%H:%M:%S");

  g_ascii_dtostr (lat_str, sizeof (lat_str), lat);
  g_ascii_dtostr (lon_str, sizeof (lon_str), lon);

  (name == NULL) ? name = "" : 0;
  (description == NULL) ? description = "" : 0;
  (comment == NULL) ? comment = "" : 0;
  (notes == NULL) ? notes = "" : 0;

  /* LAT,LON,NAME,DESCRIPTION,COMMENT,NOTES,DATE,TIME\n */
  str = g_strdup_printf ("%s,%s,%s,%s,%s,%s,%s,%s\n",
                         lat_str, lon_str,
                         name, description, comment, notes,
                         date, time);
  g_date_time_unref (dt);

  return str;
}

/* Функция печати всех меток. */
static gchar *
hyscan_gtk_mark_export_print_marks (GHashTable *wf_marks,
                                    GHashTable *geo_marks)
{
  GHashTableIter iter;
  gchar *mark_id = NULL;
  GString *str = g_string_new (NULL);

  /* Водопадные метки. */
  if (wf_marks != NULL)
    {
      HyScanMarkLocation *location;

      g_hash_table_iter_init (&iter, wf_marks);
      while (g_hash_table_iter_next (&iter, (gpointer *) &mark_id, (gpointer *) &location))
        {
          gchar *line;

          if (location->mark->type != HYSCAN_TYPE_MARK_WATERFALL)
            continue;

          line = hyscan_gtk_mark_export_formatter (location->mark_geo.lat,
                                                   location->mark_geo.lon,
                                                   location->mark->ctime,
                                                   location->mark->name,
                                                   location->mark->description,
                                                   NULL, NULL);

          g_string_append (str, line);
          g_free (line);
        }
    }
  /* Гео-метки. */
  if (geo_marks != NULL)
    {
      HyScanMarkGeo *geo_mark;

      g_hash_table_iter_init (&iter, geo_marks);
      while (g_hash_table_iter_next (&iter, (gpointer *) &mark_id, (gpointer *) &geo_mark))
        {
          gchar *line;

          if (geo_mark->type != HYSCAN_TYPE_MARK_GEO)
            continue;

          line = hyscan_gtk_mark_export_formatter (geo_mark->center.lat,
                                                   geo_mark->center.lon,
                                                   geo_mark->ctime,
                                                   geo_mark->name,
                                                   geo_mark->description,
                                                   NULL, NULL);

          g_string_append (str, line);
          g_free (line);
        }
    }

  /* Возвращаю строку в чистом виде. */
  return g_string_free (str, FALSE);
}

/**
 * hyscan_gtk_mark_export_to_str:
 * @ml_model: указатель на модель водопадных меток с данными о положении в пространстве
 * @mark_geo_model: указатель на модель геометок
 * @project_name: указатель на назване проекта
 *
 * информация о метках в виде строки
 */
gchar*
hyscan_gtk_mark_export_to_str (HyScanMarkLocModel *ml_model,
                               HyScanObjectModel  *mark_geo_model,
                               gchar              *project_name)
{
  GHashTable *wf_marks, *geo_marks;
  GDateTime *local;
  gchar *str, *marks, *date;

  wf_marks = hyscan_mark_loc_model_get (ml_model);
  geo_marks = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (mark_geo_model), HYSCAN_TYPE_MARK_GEO);

  if (wf_marks == NULL || geo_marks == NULL)
    return NULL;

  local = g_date_time_new_now_local ();
  date = g_date_time_format (local, "%A %B %e %T %Y");

  g_date_time_unref (local);

  marks = hyscan_gtk_mark_export_print_marks (wf_marks, geo_marks);
  str = g_strdup_printf (_("%s\nProject: %s\n%s%s"),
                         date,
                         project_name,
                         hyscan_gtk_mark_export_header,
                         marks);

  g_free (date);

  g_hash_table_unref (wf_marks);
  g_hash_table_unref (geo_marks);

  g_free (marks);

  return str;
}

/* Функция-обработчик завершения гененрации тайла.
 * По завершению генерации уменьшает счётчик герируемых тайлов.
 * */
void
hyscan_gtk_mark_export_tile_loaded (Package    *package, /* Пакет дополнительных данных. */
                                    HyScanTile *tile,    /* Тайл. */
                                    gfloat     *img,     /* Акустическое изображение. */
                                    gint        size,    /* Размер акустического изображения в байтах. */
                                    gulong      hash)    /* Хэш состояния тайла. */
{
  package->counter--;
}

/*
 * функция ищет тайл в кэше и если не находит, то запускает процесс генерации тайла.
 * */
void
hyscan_gtk_mark_export_generate_tile (HyScanMarkLocation *location,   /* Метка. */
                                      HyScanTileQueue    *tile_queue, /* Очередь для работы с акустическими
                                                                       * изображениями. */
                                      guint              *counter)    /* Счётчик генерируемых тайлов. */
{
  if (location->mark->type == HYSCAN_TYPE_MARK_WATERFALL)
    {
      HyScanTile *tile = NULL;
      HyScanTileCacheable tile_cacheable;

      tile = hyscan_tile_new (location->track_name);
      tile->info.source = hyscan_source_get_type_by_id (location->mark->source);
      if (tile->info.source != HYSCAN_SOURCE_INVALID)
        {
          hyscan_gtk_mark_export_init_tile (tile, &tile_cacheable, location);

          if (hyscan_tile_queue_check (tile_queue, tile, &tile_cacheable, NULL) == FALSE)
            {
              HyScanCancellable *cancellable;
              cancellable = hyscan_cancellable_new ();
              /* Добавляем тайл в очередь на генерацию. */
              hyscan_tile_queue_add (tile_queue, tile, cancellable);
              /* Увеличиваем счётчик тайлов. */
              (*counter)++;
              g_object_unref (cancellable);
            }
        }
      g_object_unref (tile);
    }
}

/*
 * функция сохраняет метку в файл и добавляет запись в файл index.html.
 * */
void
hyscan_gtk_mark_export_save_tile (HyScanMarkLocation *location,     /* Метка. */
                                  GDateTime          *track_ctime,  /* Время создания галса. */
                                  HyScanTileQueue    *tile_queue,   /* Очередь для работы с акустическими изображениями. */
                                  const gchar        *image_folder, /* Полный путь до папки с изображениями. */
                                  const gchar        *media,        /* Папка для сохранения изображений. */
                                  const gchar        *project_name, /* Название проекта. */
                                  const gchar        *id,           /* Идентификатор метки. */
                                  FILE               *file,         /* Дескриптор файла для записи данных в index.html. */
                                  Package            *package)      /* Пакет дополнительных данных. */
{
  if (location->mark->type == HYSCAN_TYPE_MARK_WATERFALL)
    {
      HyScanTile *tile = NULL;
      HyScanTileCacheable tile_cacheable;

      tile = hyscan_tile_new (location->track_name);
      tile->info.source = hyscan_source_get_type_by_id (location->mark->source);
      if (tile->info.source != HYSCAN_SOURCE_INVALID)
        {
          hyscan_gtk_mark_export_init_tile (tile, &tile_cacheable, location);

          if (hyscan_tile_queue_check (tile_queue, tile, &tile_cacheable, NULL))
            {
              gfloat *image = NULL;
              guint32 size = 0;
              if (hyscan_tile_queue_get (tile_queue, tile, &tile_cacheable, &image, &size))
                {
                  gboolean echo = (location->direction == HYSCAN_MARK_LOCATION_BOTTOM)? TRUE : FALSE;
                  gdouble width = hyscan_gtk_mark_export_get_wfmark_width (location);
                  GDateTime *local = NULL;
                  gchar *lat, *lon, *name, *description, *comment, *notes,
                        *date, *time, *content, *file_name, *board, *format,
                        *track_time = (track_ctime == NULL)? g_strdup (_(empty)) : g_date_time_format (track_ctime, time_stamp);
                  if (echo)
                    {
                      format = g_strdup (_("\t\t\t<p><a name=\"%s\"><strong>%s</strong></a></p>\n"
                                           "\t\t\t\t<img src=\"%s/%s.png\" alt=\"%s\" title=\"%s\">\n"
                                           "\t\t\t\t<p>Date: %s<br>\n"
                                           "\t\t\t\tTime: %s<br>\n"
                                           "\t\t\t\tLocation: %s, %s (%s)<br>\n"
                                           "\t\t\t\tDescription: %s<br>\n"
                                           "\t\t\t\tComment: %s<br>\n"
                                           "\t\t\t\tNotes: %s<br>\n"
                                           "\t\t\t\tTrack: %s<br>\n"
                                           "\t\t\t\tTrack creation date: %s<br>\n"
                                           "\t\t\t\tBoard: %s<br>\n"
                                           "\t\t\t\tDepth: %.2f m<br>\n"
                                           "\t\t\t\tProject: %s<br>\n"
                                           "\t\t\t\t%s</p>\n"
                                           "\t\t\t<br style=\"page-break-before: always\"/>\n"));
                    }
                  else
                    {
                      format = g_strdup (_("\t\t\t<p><a name=\"%s\"><strong>%s</strong></a></p>\n"
                                           "\t\t\t\t<img src=\"%s/%s.png\" alt=\"%s\" title=\"%s\">\n"
                                           "\t\t\t\t<p>Date: %s<br>\n"
                                           "\t\t\t\tTime: %s<br>\n"
                                           "\t\t\t\tLocation: %s, %s (%s)<br>\n"
                                           "\t\t\t\tDescription: %s<br>\n"
                                           "\t\t\t\tComment: %s<br>\n"
                                           "\t\t\t\tNotes: %s<br>\n"
                                           "\t\t\t\tTrack: %s<br>\n"
                                           "\t\t\t\tTrack creation date: %s<br>\n"
                                           "\t\t\t\tBoard: %s<br>\n"
                                           "\t\t\t\tDepth: %.2f m<br>\n"
                                           "\t\t\t\tWidth: %.2f m<br>\n"
                                           "\t\t\t\tSlant range: %.2f m<br>\n"
                                           "\t\t\t\tProject: %s<br>\n"
                                           "\t\t\t\t%s</p>\n"
                                           "\t\t\t<br style=\"page-break-before: always\"/>\n"));
                    }
                  lat = g_strdup_printf ("%.6f°", location->mark_geo.lat);
                  lon = g_strdup_printf ("%.6f°", location->mark_geo.lon);

                  local  = g_date_time_new_from_unix_local (1e-6 * location->mark->ctime);
                  if (local == NULL)
                    return;

                  date = g_date_time_format (local, "%m/%d/%Y");
                  time = g_date_time_format (local, "%H:%M:%S");

                  name = (location->mark->name == NULL ||
                         0 == g_strcmp0 (location->mark->name, "")) ?
                         g_strdup (_(unknown)) : g_strdup (location->mark->name);
                  description = (location->mark->description == NULL ||
                                 0 == g_strcmp0 (location->mark->description, "")) ?
                                 g_strdup (_(empty)) : g_strdup (location->mark->description);

                  comment = g_strdup (_(empty));
                  notes   = g_strdup (_(empty));

                  switch (location->direction)
                    {
                    case HYSCAN_MARK_LOCATION_PORT:
                      board = g_strdup (_("Port"));
                      break;
                    case HYSCAN_MARK_LOCATION_STARBOARD:
                      board = g_strdup (_("Starboard"));
                      break;
                    case HYSCAN_MARK_LOCATION_BOTTOM:
                      board = g_strdup (_("Bottom"));
                      break;
                    default:
                      board = g_strdup (_(unknown));
                      break;
                    }

                  if (echo)
                    {
                      /* location->across - глубина до ценра тайла.*/
                      /* location->depth - расстояние от поверхности
                       * до линии дна по центральной вертикальной линии тайла. */
                      content = g_strdup_printf (format, id, name, media, id, name, name,
                                                 date, time, lat, lon, sys_coord, description,
                                                 comment, notes, location->track_name,
                                                 track_time, board, location->depth,
                                                 project_name, _(link_to_site));
                    }
                  else
                    {
                      content = g_strdup_printf (format, id, name, media, id, name, name,
                                                 date, time, lat, lon, sys_coord, description,
                                                 comment, notes, location->track_name,
                                                 track_time, board,location->depth, width,
                                                 location->across, project_name, _(link_to_site));
                    }

                  fwrite (content, sizeof (gchar), strlen (content), file);

                  g_free (format);
                  g_free (content);
                  g_free (lat);
                  g_free (lon);
                  g_free (name);
                  g_free (description);
                  g_free (comment);
                  g_free (notes);
                  g_free (track_time);
                  g_free (date);
                  g_free (time);
                  g_free (board);

                  lat = lon = name = description = comment = notes = date = time = NULL;

                  g_date_time_unref (local);

                  tile->cacheable = tile_cacheable;
                  file_name = g_strdup_printf ("%s/%s.png", image_folder, id);

                  hyscan_gtk_mark_export_save_tile_as_png (tile,
                                                           package,
                                                           image,
                                                           size,
                                                           file_name,
                                                           echo);
                  g_free (file_name);
                }
            }
        }
      g_object_unref (tile);
    }
}

/* Функция сохраняет тайл в формате PNG. */
void
hyscan_gtk_mark_export_save_tile_as_png (HyScanTile  *tile,       /* Тайл. */
                                         Package     *package,    /* Пакет дополнительных данных. */
                                         gfloat      *img,        /* Акустическое изображение. */
                                         gint         size,       /* Размер акустического изображения в байтах. */
                                         const gchar *file_name,  /* Имя файла. */
                                         gboolean     echo)       /* Флаг "эхолотной" метки, которую нужно
                                                                   * повернуть на 90 градусов и отразить
                                                                   * по горизонтали. */
{
  cairo_surface_t   *surface    = NULL;
  HyScanTileSurface  tile_surface;
  HyScanTileColor   *tile_color = NULL;
  guint32            background,
                     colors[2],
                    *colormap   = NULL;
  guint              cmap_len;

  if (file_name != NULL)
    {
      tile_color = hyscan_tile_color_new (package->cache);

      background = hyscan_tile_color_converter_d2i (0.15, 0.15, 0.15, 1.0);
      colors[0]  = hyscan_tile_color_converter_d2i (0.0, 0.0, 0.0, 1.0);
      colors[1]  = hyscan_tile_color_converter_d2i (package->color.red,
                                                    package->color.green,
                                                    package->color.blue,
                                                    1.0);

      colormap   = hyscan_tile_color_compose_colormap (colors, 2, &cmap_len);

      hyscan_tile_color_set_colormap_for_all (tile_color, colormap, cmap_len, background);

      /* 1.0 / 2.2 = 0.454545... */
      hyscan_tile_color_set_levels (tile_color, tile->info.source, 0.0, 0.454545, 1.0);

      tile_surface.width  = tile->cacheable.w;
      tile_surface.height = tile->cacheable.h;
      tile_surface.stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32,
                                                           tile_surface.width);
      tile_surface.data   = g_malloc0 (tile_surface.height * tile_surface.stride);

      hyscan_tile_color_add (tile_color, tile, img, size, &tile_surface);

      if (hyscan_tile_color_check (tile_color, tile, &tile->cacheable))
        {
          gpointer buffer = NULL;

          if (echo == TRUE)
            {
              gint   i, j,
                     height = tile_surface.width,
                     width  = tile_surface.height,
                     stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);
              guint *dest,
                    *src;

              buffer = g_malloc0 (tile_surface.height * tile_surface.stride);

              hyscan_tile_color_get (tile_color, tile, &tile->cacheable, &tile_surface);

              dest = (guint*)buffer,
              src  = (guint*)tile_surface.data;
              /* Поворот на 90 градусов */
              for (j = 0; j < tile_surface.height; j++)
                {
                  for (i = 0; i < tile_surface.width; i++)
                    {
                      dest[i * tile_surface.height + j] = src[j * tile_surface.width + i];
                    }
                }
              /* Отражение по горизонтали. */
              for (j = 0; j <= (height - 1); j++)
                {
                  for (i = 0; i <= (width >> 1); i++)
                    {
                      gint  from = j * width + i,
                            to   = j * width + width - 1 - i;
                      guint tmp  = dest[from];

                      dest[from] = dest[to];
                      dest[to]   = tmp;
                   }
                }

              surface = cairo_image_surface_create_for_data ( (guchar*)buffer,
                                                              CAIRO_FORMAT_ARGB32,
                                                              width,
                                                              height,
                                                              stride);
            }
          else
            {
              hyscan_tile_color_get (tile_color, tile, &tile->cacheable, &tile_surface);
              surface = cairo_image_surface_create_for_data ( (guchar*)tile_surface.data,
                                                              CAIRO_FORMAT_ARGB32,
                                                              tile_surface.width,
                                                              tile_surface.height,
                                                              tile_surface.stride);
            }
          cairo_surface_write_to_png (surface, file_name);
          if (buffer != NULL)
            g_free (buffer);
        }
      if (tile_surface.data != NULL)
        g_free (tile_surface.data);
      if (tile_color != NULL)
        g_object_unref (tile_color);
      if (surface != NULL)
        cairo_surface_destroy (surface);
    }
}

/*
 * Функция инициализирует структуры HyScanTile и HyScanTileCacheable
 * используя данные метки из location.
 * */
void
hyscan_gtk_mark_export_init_tile (HyScanTile          *tile,
                                  HyScanTileCacheable *tile_cacheable,
                                  HyScanMarkLocation  *location)
{
  gfloat  ppi    = 1.0f;
  gdouble width  = location->mark->width,
          height = location->mark->height;

  if (location->direction == HYSCAN_MARK_LOCATION_BOTTOM)
    {
      /* Если метка "эхолотная", то умножаем её высоту на ship_speed.
       * И меняем ширину и высоту местами, т.к. у Echosounder-а другая система координат.*/
      width  = ship_speed * location->mark->height;
      height = location->mark->width;
    }

  /* Для левого борта тайл надо отразить по оси X. */
  if (location->direction == HYSCAN_MARK_LOCATION_PORT)
    {
      /* Поэтому значения отрицательные, и start и end меняются местами. */
      tile->info.across_start = -round (
            (location->across + width) * 1000.0);
      tile->info.across_end   = -round (
            (location->across - width) * 1000.0);
      if (tile->info.across_start < 0 && tile->info.across_end > 0)
        {
          tile->info.across_end = 0;
        }
    }
  else if (location->direction == HYSCAN_MARK_LOCATION_BOTTOM)
    {
      tile->info.across_start = round (
            (location->across - height) * 1000.0);
      tile->info.across_end   = round (
            (location->across + height) * 1000.0);
    }
  else
    {
      tile->info.across_start = round (
            (location->across - width) * 1000.0);
      tile->info.across_end   = round (
            (location->across + width) * 1000.0);
      if (tile->info.across_start < 0 && tile->info.across_end > 0)
        {
          tile->info.across_start = 0;
        }
    }

  if (location->direction == HYSCAN_MARK_LOCATION_BOTTOM)
    {
      tile->info.along_start = round (
            (location->along - width) * 1000.0);
      tile->info.along_end   = round (
            (location->along + width) * 1000.0);
    }
  else
    {
      tile->info.along_start = round (
            (location->along - height) * 1000.0);
      tile->info.along_end   = round (
            (location->along + height) * 1000.0);
    }

  /* Нормировка тайлов по ширине в 600 пикселей. */
  ppi = 600.0 / ( (2.0 * 100.0 * width) / 2.54);

  tile->info.scale    = 1.0f;
  tile->info.ppi      = ppi;
  tile->info.upsample = 2;
  tile->info.rotate   = FALSE;
  tile->info.flags    = HYSCAN_TILE_GROUND;

  if (tile->info.source == HYSCAN_SOURCE_ECHOSOUNDER)
    {
      tile->info.rotate   = FALSE;
      tile->info.flags    = HYSCAN_TILE_PROFILER;
    }

  tile_cacheable->w =                  /* Будет заполнено генератором. */
  tile_cacheable->h = 0;               /* Будет заполнено генератором. */
  tile_cacheable->finalized = FALSE;   /* Будет заполнено генератором. */
}

/* Потоковая функция сохранения меток в формате HTML. */
gpointer
hyscan_gtk_mark_export_save_as_html_thread (gpointer user_data)
{
  DataForHTML *data           = (DataForHTML*)user_data;
  FILE        *file           = NULL;    /* Дескриптор файла. */
  gchar       *current_folder = NULL,    /* Полный путь до папки с проектом. */
              *media          = "media", /* Папка для сохранения изображений. */
              *image_folder   = NULL,    /* Полный путь до папки с изображениями. */
              *file_name      = NULL;    /* Полный путь до файла index.html */
  /* Создаём папку с названием проекта. */
  current_folder = g_strconcat (data->folder, "/", data->project_name, (gchar*) NULL);
  g_mkdir (current_folder, 0777);
  /* HTML-файл. */
  file_name = g_strdup_printf ("%s/index.html", current_folder);
  /* Создаём папку для сохранения тайлов. */
  image_folder = g_strdup_printf ("%s/%s", current_folder, media);
  g_mkdir (image_folder, 0777);

#ifdef G_OS_WIN32
  fopen_s (&file, file_name, "w");
#else
  file = fopen (file_name, "w");
#endif
  if (file != NULL)
    {
      HyScanFactoryAmplitude  *factory_amp;  /* Фабрика объектов акустических данных. */
      HyScanFactoryDepth      *factory_dpt;  /* Фабрика объектов глубины. */
      HyScanTileQueue         *tile_queue;   /* Очередь для работы с акустическими изображениями. */
      HyScanProjectInfo       *project_info = hyscan_db_info_get_project_info (data->db,
                                                                               data->project_name);
      Package                  package;
      guint wf_mark_size  = g_hash_table_size (data->acoustic_marks),
            geo_mark_size = g_hash_table_size (data->geo_marks);
      GDateTime *local = g_date_time_new_now_local ();
      gchar *header    = "<!DOCTYPE html>\n"
                         "<html lang=\"ru\">\n"
                         "\t<head>\n"
                         "\t\t<meta charset=\"utf-8\">\n"
                         "\t\t<meta name=\"generator\" content=\"HyScan5\">\n"
                         "\t\t<meta name=\"description\" content=\"%s\">\n"
                         "\t\t<meta name=\"author\" content=\"operator_name\">\n"
                         "\t\t<meta name=\"document-state\" content=\"static\">\n"
                         "\t\t<title>%s</title>\n"
                         "\t</head>\n"
                         "\t<body>\n"
                         "\t\t<p>%s</p>\n"
                         "\t\t<p>%s<br>\n\t\t%s</p>\n"
                         "%s",
            *title     = _("Marks report"),
            *project   = g_strdup_printf (_("Project: %s"), data->project_name),
            *prj_desc  = g_strdup_printf (_("Project description: %s"),
                                          (project_info->description == NULL ||
                                           0 == g_strcmp0 (project_info->description, "")) ?
                                           _(empty) : project_info->description),
            *crtime    = g_strdup_printf (_("Project creation date: %s"),
                                         (project_info->ctime == NULL)? _(empty) :
                                                g_date_time_format (project_info->ctime, time_stamp)),
            *date      = g_date_time_format (local, time_stamp),
            *gntime    = g_strdup_printf (_("Report creation date: %s"), date),
            *footer    = "\t</body>\n"
                         "</html>",
            *str       = NULL,
            *txt_title = NULL,
            *list      = "\t\t<br style=\"page-break-before: always\"/>\n";

      g_date_time_unref (local);
      g_free (date);

      title = g_strdup_printf (title, data->project_name);
      if (wf_mark_size > 0 || geo_mark_size > 0)
        {
          gchar *tmp = _("%s<br>\n"
                       "\t\t%s<br>\n"
                       "\t\t%s<br>\n"
                       "\t\t%s</p>\n"
                       "\t\t<p>Statistics<br>\n"
                       "\t\tGeo marks: %i<br>\n"
                       "\t\tAcoustic marks: %i<br>\n"
                       "\t\tTotal: %i");
          txt_title = g_strdup_printf (tmp,
                                       title, project, crtime, prj_desc,
                                       geo_mark_size, wf_mark_size,
                                       geo_mark_size + wf_mark_size);
        }
      else
        {
          txt_title = g_strdup_printf ("%s\n"
                                       "\t\t%s<br>\n"
                                       "\t\t%s<br>\n"
                                       "\t\t%s",
                                       title, project, crtime, prj_desc);
        }
      title = g_strdup_printf ("%s. %s. %s. %s.", title, project, crtime, gntime);

      if (data->geo_marks != NULL)
        {
          HyScanMarkGeo  *geo_mark   = NULL;
          GHashTableIter  hash_iter;
          gchar          *mark_id    = NULL;

          list = g_strconcat (list, _("\t\t<a href=\"#geo\"><strong>Geo marks</strong></a><br>\n"), (gchar*) NULL);

          g_hash_table_iter_init (&hash_iter, data->geo_marks);

          while (g_hash_table_iter_next (&hash_iter, (gpointer *) &mark_id, (gpointer *) &geo_mark))
            {
              gchar *link_to_mark = g_strdup_printf ("\t\t\t<a href=\"#%s\">%s</a><br>\n",
                                                     mark_id,
                                                     (geo_mark->name == NULL ||
                                                      0 == g_strcmp0 (geo_mark->name, "") ?
                                                      _(unknown) : geo_mark->name));
              list = g_strconcat (list, link_to_mark, (gchar*) NULL);
              g_free (link_to_mark);
            }
          list = g_strconcat (list, "\t\t<br style=\"page-break-before: always\"/>\n", (gchar*) NULL);
        }

      if (data->acoustic_marks != NULL)
        {
          HyScanMarkLocation *location   = NULL;
          GHashTableIter      hash_iter;
          gchar              *mark_id    = NULL;

          list = g_strconcat (list, _("\t\t<a href=\"#wf\"><strong>Acoustic marks</strong></a><br>\n"), (gchar*) NULL);

          g_hash_table_iter_init (&hash_iter, data->acoustic_marks);

          while (g_hash_table_iter_next (&hash_iter, (gpointer *) &mark_id, (gpointer *) &location))
            {
              gchar *link_to_mark = g_strdup_printf ("\t\t\t<a href=\"#%s\">%s</a><br>\n",
                                                     mark_id,
                                                     (location->mark->name == NULL ||
                                                      0 == g_strcmp0 (location->mark->name, "") ?
                                                      _(unknown) : location->mark->name));
              list = g_strconcat (list, link_to_mark, (gchar*) NULL);
              g_free (link_to_mark);
            }
          list = g_strconcat (list, "\t\t<br style=\"page-break-before: always\"/>\n", (gchar*) NULL);
        }

      str   = g_strdup_printf (header, title, title, txt_title, _(link_to_site), gntime, list);
      fwrite (str, sizeof (gchar), strlen (str), file);

      g_free (str);
      g_free (txt_title);
      g_free (title);
      g_free (project);
      g_free (prj_desc);
      g_free (crtime);
      g_free (gntime);
      g_free (list);
      /* Создаём фабрику объектов доступа к данным амплитуд. */
      factory_amp = hyscan_factory_amplitude_new (data->cache);
      hyscan_factory_amplitude_set_project (factory_amp,
                                            data->db,
                                            data->project_name);
      /* Создаём фабрику объектов доступа к данным глубины. */
      factory_dpt = hyscan_factory_depth_new (data->cache);
      hyscan_factory_depth_set_project (factory_dpt,
                                        data->db,
                                        data->project_name);
      /* Создаём очередь для генерации тайлов. */
      tile_queue = hyscan_tile_queue_new (g_get_num_processors (),
                                          data->cache,
                                          factory_amp,
                                          factory_dpt);
      g_mutex_init (&package.mutex);
      package.cache   = data->cache;
      package.color   = data->color;
      package.counter = 0;
      /* Соединяем сигнал готовности тайла с функцией-обработчиком. */
      g_signal_connect_swapped (G_OBJECT (tile_queue), "tile-queue-image",
                                G_CALLBACK (hyscan_gtk_mark_export_tile_loaded), &package);

      /* Акустические метки. */
      if (data->acoustic_marks != NULL)
        {
          HyScanMarkLocation *location   = NULL; /*  */
          GHashTableIter      hash_iter;
          gchar              *mark_id    = NULL; /* Идентификатор метки. */

          g_hash_table_iter_init (&hash_iter, data->acoustic_marks);

          while (g_hash_table_iter_next (&hash_iter, (gpointer *) &mark_id, (gpointer *) &location))
            {
              hyscan_gtk_mark_export_generate_tile (location, tile_queue, &package.counter);
            }
        }
      if (data->geo_marks != NULL)
        {
          HyScanMarkGeo  *geo_mark   = NULL; /*  */
          GHashTableIter  hash_iter;
          gchar          *mark_id    = NULL, /* Идентификатор метки. */
                         *category   = _("\t\t<p><a name=\"geo\"><strong>Geo marks</strong></a></p>\n");

          fwrite (category, sizeof (gchar), strlen (category), file);

          g_hash_table_iter_init (&hash_iter, data->geo_marks);

          while (g_hash_table_iter_next (&hash_iter, (gpointer *) &mark_id, (gpointer *) &geo_mark))
            {
              GDateTime *local;
              gchar *lat, *lon, *name, *description, *comment, *notes, *date, *time;

              lat = g_strdup_printf ("%.6f°", geo_mark->center.lat);
              lon = g_strdup_printf ("%.6f°", geo_mark->center.lon);

              local  = g_date_time_new_from_unix_local (1e-6 * geo_mark->ctime);
              if (local == NULL)
                {
                  date = g_strdup (_(empty));
                  time = g_strdup (_(empty));
                }
              else
                {
                  date = g_date_time_format (local, "%m/%d/%Y");
                  time = g_date_time_format (local, "%H:%M:%S");
                }

              name = (geo_mark->name == NULL ||
                      0 == g_strcmp0 (geo_mark->name, "")) ?
                      g_strdup (_(unknown)) : g_strdup (geo_mark->name);

              description = (geo_mark->description != NULL ||
                             0 == g_strcmp0 (geo_mark->description, "")) ?
                             g_strdup (_(empty)) : g_strdup (geo_mark->description);

              comment = g_strdup (_(empty));
              notes = g_strdup (_(empty));

              if (geo_mark->type == HYSCAN_TYPE_MARK_GEO)
                {
                   gchar *format  = _("\t\t\t<p><a name=\"%s\"><strong>%s</strong></a></p>\n"
                                    "\t\t\t\t<p>Date: %s<br>\n"
                                    "\t\t\t\tTime: %s<br>\n"
                                    "\t\t\t\tLocation: %s, %s (%s)<br>\n"
                                    "\t\t\t\tDescription: %s<br>\n"
                                    "\t\t\t\tComment: %s<br>\n"
                                    "\t\t\t\tNotes: %s<br>\n"
                                    "\t\t\t\tProject: %s<br>\n"
                                    "\t\t\t\t%s</p>\n"
                                    "\t\t\t<br style=\"page-break-before: always\"/>\n");
                   gchar *content = g_strdup_printf (format, mark_id, name, date, time, lat, lon,
                                                     sys_coord, description, comment, notes,
                                                     data->project_name, _(link_to_site));
                   fwrite (content, sizeof (gchar), strlen (content), file);
                   g_free (content);
                }

              g_free (lat);
              g_free (lon);
              g_free (name);
              g_free (description);
              g_free (comment);
              g_free (notes);
              g_free (date);
              g_free (time);

              lat = lon = name = description = comment = notes = date = time = NULL;

              g_date_time_unref (local);
            }
        }

      if (package.counter)
        {
          GRand *rand = g_rand_new ();  /* Идентификатор для TileQueue. */
          /* Запуск генерации тайлов. */
          hyscan_tile_queue_add_finished (tile_queue, g_rand_int_range (rand, 0, INT32_MAX));
          /* Устанавливаем курсор-часы. */
          g_timeout_add (0, hyscan_gtk_mark_export_set_watch_cursor, data->toplevel);
          g_free (rand);
        }
      while (package.counter > 0)
        {
          /* Ждём пока сгенерируются и сохранятся все тайлы. */
          /* g_usleep (1000); */
          g_print ("Waiting...\n");
        }
      g_print ("Done.\n");
      if (data->acoustic_marks != NULL)
        {
          HyScanMarkLocation *location   = NULL;
          GHashTableIter      hash_iter;
          gchar              *mark_id    = NULL,
                             *category   = _("\t\t<p><a name=\"wf\"><strong>Acoustic marks</strong></a></p>\n");

          fwrite (category, sizeof (gchar), strlen (category), file);

          g_hash_table_iter_init (&hash_iter, data->acoustic_marks);

          while (g_hash_table_iter_next (&hash_iter, (gpointer *) &mark_id, (gpointer *) &location))
            {
              HyScanTrackInfo *track_info;
              gint32 project_id = hyscan_db_project_open (data->db, data->project_name);

              if (project_id <= 0)
                continue;

              track_info = hyscan_db_info_get_track_info (data->db,
                                                          project_id,
                                                          location->track_name);
              hyscan_db_close (data->db, project_id);
              hyscan_gtk_mark_export_save_tile (location,
                                                track_info->ctime,
                                                tile_queue,
                                                image_folder,
                                                media,
                                                data->project_name,
                                                mark_id,
                                                file,
                                                &package);
            }
        }

      g_mutex_clear (&package.mutex);

      g_object_unref (tile_queue);
      g_object_unref (factory_dpt);
      g_object_unref (factory_amp);

      fwrite (footer, sizeof (gchar), strlen (footer), file);
      fclose (file);
    }

  g_hash_table_unref (data->acoustic_marks);
  g_hash_table_unref (data->geo_marks);
  g_object_unref (data->db);
  g_object_unref (data->cache);
  data->project_name = NULL;
  g_free (file_name);
  g_timeout_add (0, hyscan_gtk_mark_export_set_default_cursor, data->toplevel);
  g_free (data);
  return NULL;
}

/* Функция безопасно устанавливает курсор "Часы". */
gboolean
hyscan_gtk_mark_export_set_watch_cursor (gpointer user_data)
{
  GtkWidget  *window;
  GdkCursor  *cursor_watch;
  GdkDisplay *display;

  g_return_val_if_fail (GTK_IS_WIDGET (user_data), FALSE);

  window       = GTK_WIDGET (user_data);
  display      = gdk_display_get_default ();
  cursor_watch = gdk_cursor_new_for_display (display, GDK_WATCH);
  gdk_window_set_cursor (gtk_widget_get_window (window), cursor_watch);
  return FALSE;
}

/* Функция безопасно устанавливает курсор по умолчанию. */
gboolean
hyscan_gtk_mark_export_set_default_cursor (gpointer user_data)
{
  GtkWidget *window;

  g_return_val_if_fail (GTK_IS_WIDGET (user_data), FALSE);

  window = GTK_WIDGET (user_data);
  gdk_window_set_cursor (gtk_widget_get_window (window), NULL);
  return FALSE;
}

/* функция возвращает ширину метки с учётом выхода за пределы "борта". */
gdouble
hyscan_gtk_mark_export_get_wfmark_width (HyScanMarkLocation  *location)
{
  gdouble width = 2.0 * location->mark->width;
  /* "Эхолотная" метка. */
  if (location->direction == HYSCAN_MARK_LOCATION_BOTTOM)
    {
      /* Если метка "эхолотная", то умножаем её габариты на ship_speed.
       * И меняем ширину и высоту местами, т.к. у Echosounder-а другая система координат.*/
      width =  ship_speed * 2.0 * location->mark->height;
    }
  /* Левоый борт. */
  else if (location->direction == HYSCAN_MARK_LOCATION_PORT)
    {
      /* Поэтому значения отрицательные, и start и end меняются местами. */
      gdouble across_start = -location->mark->width - location->across;
      gdouble across_end   =  location->mark->width - location->across;
      if (across_start < 0 && across_end > 0)
        {
          width = -across_start;
        }
    }
  /* Правый борт. */
  else
    {
      gdouble across_start = location->across - location->mark->width;
      gdouble across_end   = location->across + location->mark->width;
      if (across_start < 0 && across_end > 0)
        {
          width = across_end;
        }
    }
  return width;
}
/**
 * hyscan_gtk_mark_export_save_as_csv:
 * @window: указатель на окно, которое вызывает диалог выбора файла для сохранения данных
 * @ml_model: указатель на модель водопадных меток с данными о положении в пространстве
 * @mark_geo_model: указатель на модель геометок
 * @projrct_name: указатель на название проекта. Используется как имя файла по умолчанию
 *
 * сохраняет список меток в формате CSV.
 */
void
hyscan_gtk_mark_export_save_as_csv (GtkWindow          *window,
                                    HyScanMarkLocModel *ml_model,
                                    HyScanObjectModel  *mark_geo_model,
                                    gchar              *project_name)
{
  FILE       *file     = NULL;
  GtkWidget  *dialog;
  GHashTable *wf_marks,
             *geo_marks;
  gint        res;
  gchar      *filename = NULL,
             *marks;

  wf_marks  = hyscan_mark_loc_model_get (ml_model);
  geo_marks = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (mark_geo_model), HYSCAN_TYPE_MARK_GEO);

  if (wf_marks == NULL || geo_marks == NULL)
    return;

  dialog = gtk_file_chooser_dialog_new (_("Save file"),
                                        window,
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("Cancel"),
                                        GTK_RESPONSE_CANCEL,
                                        _("Save"),
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  filename = g_strdup_printf ("%s.csv", project_name);

  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), filename);

  res = gtk_dialog_run (GTK_DIALOG (dialog));

  if (res != GTK_RESPONSE_ACCEPT)
    {
      gtk_widget_destroy (dialog);
      return;
    }

  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
#ifdef G_OS_WIN32
  fopen_s (&file, filename, "w");
#else
  file = fopen (filename, "w");
#endif
  if (file != NULL)
    {
      fwrite (hyscan_gtk_mark_export_header, sizeof (gchar), g_utf8_strlen (hyscan_gtk_mark_export_header, -1), file);

      marks = hyscan_gtk_mark_export_print_marks (wf_marks, geo_marks);
      fwrite (marks, sizeof (gchar), g_utf8_strlen (marks, -1), file);

      fclose (file);
    }

  g_hash_table_unref (wf_marks);
  g_hash_table_unref (geo_marks);
  g_free (filename);
  gtk_widget_destroy (dialog);
}

/**
 * hyscan_gtk_mark_export_copy_to_clipboard:
 * @ml_model: указатель на модель водопадных меток с данными о положении в пространстве
 * @mark_geo_model: указатель на модель геометок
 * @project_name: указатель на назване проекта. Добавляется в буфер обмена как описание содержимого
 *
 * копирует список меток в буфер обмена
 */
void
hyscan_gtk_mark_export_copy_to_clipboard (HyScanMarkLocModel *ml_model,
                                          HyScanObjectModel  *mark_geo_model,
                                          gchar              *project_name)
{
  GtkClipboard *clipboard;
  GHashTable   *wf_marks,
               *geo_marks;
  GDateTime    *local;
  gchar        *str, *marks, *date;

  wf_marks  = hyscan_mark_loc_model_get (ml_model);
  geo_marks = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (mark_geo_model), HYSCAN_TYPE_MARK_GEO);

  if (wf_marks == NULL || geo_marks == NULL)
    return;

  local = g_date_time_new_now_local ();
  date = g_date_time_format (local, "%A %B %e %T %Y");

  g_date_time_unref (local);

  marks = hyscan_gtk_mark_export_print_marks (wf_marks, geo_marks);
  str   = g_strdup_printf (_("%s\nProject: %s\n%s%s"),
                           date,
                           project_name,
                           hyscan_gtk_mark_export_header,
                           marks);

  g_free (date);

  g_hash_table_unref (wf_marks);
  g_hash_table_unref (geo_marks);

  clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, str, -1);

  g_free (str);
  g_free (marks);
}

/**
 * hyscan_gtk_mark_export_save_as_html:
 * @model_manager: указатель на указатель на Менеджер Моделей
 * @toplevel: главное окно
 * @toggled: TRUE - только выбранные объекты, FALSE - все
 *
 * Сохраняет метки в формате HTML.
 */
void
hyscan_gtk_mark_export_save_as_html (HyScanGtkModelManager *model_manager,
                                     GtkWindow             *toplevel,
                                     gboolean               toggled)
{
  HyScanMarkLocModel *acoustic_mark_model = hyscan_gtk_model_manager_get_acoustic_mark_loc_model (model_manager);
  HyScanObjectModel  *geo_mark_model = hyscan_gtk_model_manager_get_geo_mark_model (model_manager);
  GtkWidget   *dialog = NULL;   /* Диалог выбора директории. */
  GThread     *thread = NULL;   /* Поток. */
  DataForHTML *data   = NULL;   /* Данные для потока. */
  gint         res;             /* Результат диалога. */

  dialog = gtk_file_chooser_dialog_new (_("Choose directory"),
                                        toplevel,
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        _("Cancel"),
                                        GTK_RESPONSE_CANCEL,
                                        _("Choose"),
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                       hyscan_gtk_model_manager_get_export_folder (model_manager));

  res = gtk_dialog_run (GTK_DIALOG (dialog));

  if (res != GTK_RESPONSE_ACCEPT)
    {
      gtk_widget_destroy (dialog);
      return;
    }

  data = g_malloc0 (sizeof (DataForHTML));
  if (toggled)
    {
      gchar **geo_mark_list = hyscan_gtk_model_manager_get_toggled_items (model_manager, GEO_MARK),
            **acoustic_mark_list  = hyscan_gtk_model_manager_get_toggled_items (model_manager, ACOUSTIC_MARK);
      gint i;

      if (geo_mark_list != NULL)
        {
          data->geo_marks = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   g_free,
                                                   (GDestroyNotify) hyscan_mark_geo_free);
          for (i = 0; geo_mark_list[i] != NULL; i++)
            {
              HyScanMarkGeo *geo_mark = (HyScanMarkGeo*) hyscan_object_store_get (HYSCAN_OBJECT_STORE (geo_mark_model),
                                                                                  HYSCAN_TYPE_MARK_GEO,
                                                                                  geo_mark_list[i]);
              if (geo_mark != NULL)
                {
                  g_hash_table_insert (data->geo_marks,
                                       g_strdup (geo_mark_list[i]),
                                       hyscan_mark_geo_copy (geo_mark));
                  hyscan_mark_geo_free (geo_mark);
                }
            }
        }

      if (acoustic_mark_list != NULL)
        {
          GHashTable *table = hyscan_mark_loc_model_get (acoustic_mark_model);
          data->acoustic_marks = g_hash_table_new_full (g_str_hash,
                                                        g_str_equal,
                                                        g_free,
                                                        (GDestroyNotify) hyscan_mark_location_free);
          for (i = 0; acoustic_mark_list[i] != NULL; i++)
            {
              HyScanMarkLocation *location = g_hash_table_lookup (table, acoustic_mark_list[i]);

              if (location != NULL)
                {
                  g_hash_table_insert (data->acoustic_marks,
                                       g_strdup (acoustic_mark_list[i]),
                                       hyscan_mark_location_copy (location));
                }
            }
          g_hash_table_unref (table);
        }
      g_strfreev (geo_mark_list);
      g_strfreev (acoustic_mark_list);
    }
  else
    {
      data->geo_marks = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (geo_mark_model), HYSCAN_TYPE_MARK_GEO);
      data->acoustic_marks = hyscan_mark_loc_model_get (acoustic_mark_model);
    }

  g_object_unref (geo_mark_model);
  g_object_unref (acoustic_mark_model);

  if (data->acoustic_marks == NULL || data->geo_marks == NULL)
    {
      gtk_widget_destroy (dialog);
      return;
    }

  data->project_name = hyscan_gtk_model_manager_get_project_name (model_manager);
  data->db = hyscan_gtk_model_manager_get_db (model_manager);
  data->cache = hyscan_gtk_model_manager_get_cache (model_manager);
  data->toplevel = toplevel;
  data->folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
  gdk_rgba_parse (&data->color, "#FFFF00");

  thread = g_thread_new ("save_as_html", hyscan_gtk_mark_export_save_as_html_thread, (gpointer)data);

  g_thread_unref (thread);
  gtk_widget_destroy (dialog);
}
