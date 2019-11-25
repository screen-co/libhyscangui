#include "hyscan-gtk-export.h"
#include <hyscan-hsx-converter.h>
#include <hyscan-core-common.h>
#include <glib/gi18n.h>

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJ,
  PROP_TRACKS
};

typedef struct 
{
  GtkWidget    *label;
  GtkWidget    *entry;
  GtkWidget    *button;
  gboolean      editable;
}HyScanGtkExportGUIElem;

typedef enum
{
  HYSCAN_NMEA_TYPE_RMC = 0,                  /* Индекс для NMEA данных типа RMC */              
  HYSCAN_NMEA_TYPE_GGA,                      /* Индекс для NMEA данных типа GGA */
  HYSCAN_NMEA_TYPE_DPT,                      /* Индекс для NMEA данных типа DPT */
  HYSCAN_NMEA_TYPE_HDT,                      /* Индекс для NMEA данных типа HDT */
  HYSCAN_NMEA_TYPE_LAST
} NmeaType;

typedef struct
{
  GtkWidget                   *combo;
  gchar                       *label_name;
} HyScanGtkExportGUICombo;

typedef struct 
{
  GtkWidget                   *label_top;

  HyScanGtkExportGUIElem      *project_elem;
  HyScanGtkExportGUICombo      track_elem;
  HyScanGtkExportGUIElem      *out_path_elem;

  HyScanGtkExportGUICombo     nmea_elem[HYSCAN_NMEA_TYPE_LAST];
  
  GtkWidget                   *label_execute;
  GtkWidget                   *progress_bar;

  GtkWidget                   *button_run;
  GtkWidget                   *button_stop;

  GtkWidget                   *label_bottom;
}HyScanGtkExportGUI;

struct _HyScanGtkExportPrivate
{
  HyScanHSXConverter          *converter;

  HyScanDB                    *db;
  gchar                       *project_name;
  gchar                      **track_names;
  GQueue                       track_queue;
  gchar                       *out_path;

  guint                        updater;           /* Флаг работ обновлений GUI */
  guint                        period;            /* Период обновления GUI */
  guint                        current_percent;   /* Процент выполнения */
  guint                        prev_percent;      /* Процент выполнения */
  HyScanGtkExportGUI          *ui;                /* GUI */
  gchar                       *message;
};

static void        hyscan_gtk_export_set_property             (GObject               *object,
                                                               guint                  prop_id,
                                                               const GValue          *value,
                                                               GParamSpec            *pspec);
static void        hyscan_gtk_export_object_constructed       (GObject               *object);
static void        hyscan_gtk_export_object_finalize          (GObject               *object);

static gboolean    hyscan_gtk_export_update                   (gpointer               data);
static gboolean    hyscan_gtk_export_run                      (gpointer               data);

HyScanNmeaDataType hyscan_gtk_export_nmeadt2hyscan_nmeadt     (NmeaType               type);

static void        hyscan_gtk_export_gui_nmea_select_make     (HyScanGtkExport       *self);

static HyScanGtkExportGUIElem*
                   hyscan_gtk_export_gui_elem_make            (HyScanGtkExport       *self,
                                                               gboolean               editable,
                                                               const gchar           *label_title,
                                                               const gchar           *entry_text);

static gint        hyscan_gtk_export_gui_tracks_compare       (gconstpointer          a,
                                                               gconstpointer          b,
                                                               gpointer               udata);

static void        hyscan_gtk_export_gui_tracks_make          (HyScanGtkExport       *self);
static void        hyscan_gtk_export_gui_make                 (HyScanGtkExport       *self);

static void        hyscan_gtk_export_msg_set                  (HyScanGtkExport       *self,
                                                               const gchar           *message,
                                                               ...);

static void        hyscan_gtk_export_on_select                (GtkWidget             *sender,
                                                               HyScanGtkExport       *self);

static void        hyscan_gtk_export_on_select_path           (GtkWidget             *sender,
                                                               HyScanGtkExport       *self);

static void        hyscan_gtk_export_on_select_track          (GtkComboBox           *box,
                                                               HyScanGtkExport       *self);

static void        hyscan_gtk_export_track_to_queue           (HyScanGtkExport       *self);

static void        hyscan_gtk_export_path_to_converter        (HyScanGtkExport       *self);

static void        hyscan_gtk_export_nmea_to_converter        (HyScanGtkExport       *self);

static void        hyscan_gtk_export_on_run                   (GtkWidget             *sender,
                                                               HyScanGtkExport       *self);

static void        hyscan_gtk_export_on_stop                  (GtkWidget             *sender,
                                                               HyScanGtkExport       *self);

void               hyscan_gtk_export_current_percent          (HyScanGtkExport      *self,
                                                               gint                  percent);

void               hyscan_gtk_export_done                     (HyScanGtkExport      *self);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkExport, hyscan_gtk_export, GTK_TYPE_GRID)

static void
hyscan_gtk_export_class_init (HyScanGtkExportClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_export_set_property;

  object_class->constructed = hyscan_gtk_export_object_constructed;
  object_class->finalize = hyscan_gtk_export_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB object", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROJ,
  g_param_spec_string ("project", "Project", "Project name", NULL,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACKS,
  g_param_spec_string ("tracks", "Tracks", "Tracks", NULL,
                        G_PARAM_WRITABLE));
}

static void
hyscan_gtk_export_init (HyScanGtkExport *gtk_export)
{
  gtk_export->priv = hyscan_gtk_export_get_instance_private (gtk_export);
}

static void
hyscan_gtk_export_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanGtkExport *gtk_export = HYSCAN_GTK_EXPORT (object);
  HyScanGtkExportPrivate *priv = gtk_export->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;
    case PROP_PROJ:
      priv->project_name = g_value_dup_string (value);
      break;
    case PROP_TRACKS:
      {
        gchar *tracks;
        if ((tracks = g_value_dup_string (value)) != NULL)
          priv->track_names = g_strsplit (tracks, ",", -1);
        g_clear_pointer (&tracks, g_free);
        break;

      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_export_object_constructed (GObject *object)
{
  HyScanGtkExport *gtk_export = HYSCAN_GTK_EXPORT (object);
  HyScanGtkExportPrivate *priv = gtk_export->priv;

  G_OBJECT_CLASS (hyscan_gtk_export_parent_class)->constructed (object);

  gtk_grid_set_column_spacing (GTK_GRID (gtk_export), 5);
  gtk_grid_set_row_spacing (GTK_GRID (gtk_export), 5);
  gtk_container_set_border_width (GTK_CONTAINER (gtk_export), 5);

  priv->out_path = g_strdup (".");

  g_queue_init (&priv->track_queue);

  priv->converter = hyscan_hsx_converter_new (priv->out_path);
  if (priv->converter == NULL)
    hyscan_gtk_export_msg_set (gtk_export, "Can't create converter object");

  if (!hyscan_hsx_converter_init_crs (priv->converter, NULL, NULL))
    hyscan_gtk_export_msg_set (gtk_export, "Can't init proj4 ctx");


  g_signal_connect_swapped ((gpointer)priv->converter, "exec",
                            G_CALLBACK (hyscan_gtk_export_current_percent), 
                            gtk_export);


  g_signal_connect_swapped ((gpointer)priv->converter, "done",
                            G_CALLBACK (hyscan_gtk_export_done),
                            gtk_export);
}

static void
hyscan_gtk_export_object_finalize (GObject *object)
{
  HyScanGtkExport *gtk_export = HYSCAN_GTK_EXPORT (object);
  HyScanGtkExportPrivate *priv = gtk_export->priv;

  if (priv->updater != 0)
    g_source_remove (priv->updater);

  g_object_unref (priv->db);
  g_free (priv->project_name);
  g_strfreev (priv->track_names);
  g_queue_clear (&priv->track_queue);
  g_clear_pointer (&priv->out_path, g_free);

  g_clear_pointer (&priv->message, g_free);
  g_clear_pointer (&priv->ui->track_elem.label_name, g_free);
  g_clear_pointer (&priv->ui->nmea_elem[HYSCAN_NMEA_TYPE_RMC].label_name, g_free);
  g_clear_pointer (&priv->ui->nmea_elem[HYSCAN_NMEA_TYPE_GGA].label_name, g_free);
  g_clear_pointer (&priv->ui->nmea_elem[HYSCAN_NMEA_TYPE_DPT].label_name, g_free);
  g_clear_pointer (&priv->ui->nmea_elem[HYSCAN_NMEA_TYPE_HDT].label_name, g_free);
  g_object_unref (priv->converter);

  G_OBJECT_CLASS (hyscan_gtk_export_parent_class)->finalize (object);
}

/* Завершении конвертации */
void 
hyscan_gtk_export_done (HyScanGtkExport *self)
{
  g_print ("\nConverter done");
  g_print ("\n");
  self->priv->current_percent = 0;
  hyscan_gtk_export_update (self);
  hyscan_gtk_export_msg_set (self, "Convert done");
  /*g_main_loop_quit (main_loop);*/
}

/* Обновление процентного выполнения */
void 
hyscan_gtk_export_current_percent (HyScanGtkExport *self,
                                   gint             percent)
{
  g_print ("%d%%\r", percent);
  self->priv->current_percent = percent;
}

static gboolean
hyscan_gtk_export_update (gpointer data)
{
  HyScanGtkExportPrivate *priv;
  HyScanGtkExport *self = HYSCAN_GTK_EXPORT (data);
  priv = self->priv;
  if (priv->current_percent != priv->prev_percent)
    {
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->ui->progress_bar),
                                     priv->current_percent / 100.);

      priv->prev_percent = priv->current_percent;
    }
  return G_SOURCE_CONTINUE;
}

static gboolean
hyscan_gtk_export_run (gpointer data)
{
  HyScanGtkExportPrivate *priv;
  HyScanGtkExport *self = HYSCAN_GTK_EXPORT (data);
  priv = self->priv;
  gchar *current_track = NULL;

  if (hyscan_hsx_converter_is_run (priv->converter))
    return G_SOURCE_CONTINUE;

  if ((current_track = g_queue_pop_tail (&priv->track_queue)) == NULL)
    goto remove;

  if (!hyscan_hsx_converter_set_track (priv->converter, 
                                       priv->db,
                                       priv->project_name,
                                       current_track))
    {
      hyscan_gtk_export_msg_set (self, "Can't set track %s", current_track);
      goto remove;      
    }  

  if (!hyscan_hsx_converter_run (priv->converter))
    {
      hyscan_gtk_export_msg_set (self, "Can't run convert");
      goto remove;
    }

  hyscan_gtk_export_msg_set (self, "Convert track %s", current_track);

  g_debug ("Convert track %s\n", current_track);

  return G_SOURCE_CONTINUE;

remove:
  return G_SOURCE_REMOVE;
}

HyScanNmeaDataType
hyscan_gtk_export_nmeadt2hyscan_nmeadt (NmeaType type)
{
  switch (type)
    {
    case HYSCAN_NMEA_TYPE_RMC:
      return HYSCAN_NMEA_DATA_RMC;
      break;
    case HYSCAN_NMEA_TYPE_GGA:
      return HYSCAN_NMEA_DATA_GGA;
      break;
    case HYSCAN_NMEA_TYPE_HDT:
      return HYSCAN_NMEA_DATA_HDT;
      break;
    case HYSCAN_NMEA_TYPE_DPT:
      return HYSCAN_NMEA_DATA_DPT;
      break;

    case HYSCAN_NMEA_TYPE_LAST:
    default:
      return HYSCAN_NMEA_DATA_INVALID;
      break;
    }
}
static void
hyscan_gtk_export_gui_nmea_select_make (HyScanGtkExport *self)
{
  HyScanGtkExportPrivate *priv = self->priv;
  HyScanGtkExportGUI *ui = priv->ui;

  gint32 project_id = -1, track_id = -1;
  gchar **channels = NULL;
  guint i;

  /* Получаем из БД информацию о каналах галса. */
  project_id = hyscan_db_project_open (priv->db, priv->project_name);
  if (project_id < 0)
    goto exit;
  g_debug ("proj id '%d'", project_id);

  if (priv->track_names == NULL)
    goto exit;

  track_id = hyscan_db_track_open (priv->db, project_id, priv->track_names[0]);
  if (track_id < 0)
    goto exit;
  g_debug ("track id '%d'\n", track_id);

  channels = hyscan_db_channel_list (priv->db, track_id);
  if (channels == NULL)
    goto exit;

  for (i = 0; i < HYSCAN_NMEA_TYPE_LAST; ++i)
    gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (ui->nmea_elem[i].combo));

  for (i = 0; channels[i] != NULL; ++i)
    {
      gchar *sensor_name;
      gint32 channel_id, param_id;
      gchar *id;

      HyScanSourceType source;
      HyScanChannelType type;
      guint channel_num;

      hyscan_channel_get_types_by_id (channels[i], &source, &type, &channel_num);
      if (source != HYSCAN_SOURCE_NMEA)
        continue;

      channel_id = hyscan_db_channel_open (priv->db, track_id, channels[i]);
      if (channel_id < 0)
        continue;

      param_id = hyscan_db_channel_param_open (priv->db, channel_id);
      hyscan_db_close (priv->db, channel_id);

      if (param_id < 0)
        continue;

      sensor_name = hyscan_core_params_load_sensor_info (priv->db, param_id);
      hyscan_db_close (priv->db, param_id);
      g_debug ("sensor_name '%s'\n", sensor_name);

      /* Добавляем источник в комбобокс */
      id = g_strdup_printf ("%d", channel_num);
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (ui->nmea_elem[HYSCAN_NMEA_TYPE_RMC].combo),
                                                     id, sensor_name);
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (ui->nmea_elem[HYSCAN_NMEA_TYPE_GGA].combo),
                                                     id, sensor_name);
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (ui->nmea_elem[HYSCAN_NMEA_TYPE_HDT].combo),
                                                     id, sensor_name);
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (ui->nmea_elem[HYSCAN_NMEA_TYPE_DPT].combo),
                                                     id, sensor_name);
      g_free (sensor_name);
      g_free (id);
    }

exit:
  g_clear_pointer (&channels, g_strfreev);

  if (track_id > 0)
    hyscan_db_close (priv->db, track_id);

  if (project_id > 0)
    hyscan_db_close (priv->db, project_id);
}

static HyScanGtkExportGUIElem*
hyscan_gtk_export_gui_elem_make (HyScanGtkExport *self,
                                 gboolean         editable,
                                 const gchar     *label_title,
                                 const gchar     *entry_text)
{
  HyScanGtkExportGUIElem *el = NULL;
  el = g_malloc0 (sizeof (HyScanGtkExportGUIElem));

  el->editable = editable;
  el->label    = gtk_label_new (_(label_title));
  el->entry    = gtk_entry_new ();
  
  gtk_entry_set_text (GTK_ENTRY (el->entry), _(entry_text));
  gtk_widget_set_sensitive (el->entry, editable);

  gtk_widget_set_halign (el->label, GTK_ALIGN_END);
  gtk_widget_set_hexpand (el->label, FALSE);
  gtk_widget_set_halign (el->entry, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (el->entry, TRUE);

  if (editable)
    {
      el->button = gtk_button_new_with_label (_("Select"));
      gtk_widget_set_halign (el->button, GTK_ALIGN_END);
      gtk_widget_set_hexpand (el->button, FALSE);
      g_signal_connect (el->button, "clicked", 
                        G_CALLBACK (hyscan_gtk_export_on_select), self);
    }
  return el;
}

static gint
hyscan_gtk_export_gui_tracks_compare (gconstpointer a,
                                      gconstpointer b,
                                      gpointer      udata)
{
  return g_strcmp0 ((const char*)a, (const char*)b);
}

static void
hyscan_gtk_export_gui_tracks_make (HyScanGtkExport *self)
{
  HyScanGtkExportPrivate *priv = self->priv;
  HyScanGtkExportGUI *ui = priv->ui;
  
  gint i = 0;
  gint32 pid = 0;
  gchar **tracks = NULL;

  ui->track_elem.label_name = g_strdup (_("Tracks: "));
  
  ui->track_elem.combo      = gtk_combo_box_text_new ();
  gtk_widget_set_halign (ui->track_elem.combo, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (ui->track_elem.combo, TRUE);

  pid = hyscan_db_project_open (priv->db, priv->project_name);
  if (pid < 0)
    goto exit;

  tracks = hyscan_db_track_list (priv->db, pid);
  if (tracks == NULL)
    goto exit;

  g_qsort_with_data (tracks, i, sizeof (gchar*),
                     (GCompareDataFunc)hyscan_gtk_export_gui_tracks_compare,
                     NULL);

  i = 0;
  while (tracks[i] != NULL)
    {
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (ui->track_elem.combo), tracks[i], tracks[i]);
      i++;
    }

  if (priv->track_names != NULL)
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (ui->track_elem.combo), priv->track_names[0]);

  gtk_widget_set_tooltip_text (GTK_WIDGET (ui->track_elem.combo), _("Select track"));
  g_signal_connect (ui->track_elem.combo, "changed", G_CALLBACK (hyscan_gtk_export_on_select_track), self);

exit:
  hyscan_db_close (priv->db, pid);
}

static void
hyscan_gtk_export_gui_make (HyScanGtkExport *self)
{
  HyScanGtkExportPrivate *priv;
  HyScanGtkExportGUI *ui = NULL;
  GtkWidget *label = NULL;
  gint row = 0, i = 0;
  priv = self->priv;

  ui = priv->ui = g_malloc0 (sizeof (HyScanGtkExportGUI));
  ui->label_top     = gtk_label_new (_("HSX Converter"));
  gtk_widget_set_valign (ui->label_top, GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand (ui->label_top, TRUE);

  ui->project_elem  = hyscan_gtk_export_gui_elem_make (self, FALSE, "Project: ", priv->project_name);
  ui->out_path_elem = hyscan_gtk_export_gui_elem_make (self, TRUE, "Out path: ", priv->out_path);

  ui->label_execute = gtk_label_new (_("Execute: "));
  gtk_widget_set_halign (ui->label_execute, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (ui->label_execute, FALSE);

  ui->progress_bar  = gtk_progress_bar_new ();
  gtk_widget_set_halign (ui->progress_bar, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (ui->progress_bar, TRUE);

  ui->button_run    = gtk_button_new_with_label (_("Convert"));
  gtk_widget_set_halign (ui->button_run, GTK_ALIGN_START);
  gtk_widget_set_hexpand (ui->button_run, FALSE);
  g_signal_connect (ui->button_run, "clicked", G_CALLBACK (hyscan_gtk_export_on_run), self);

  ui->button_stop   = gtk_button_new_with_label (_("Abort"));
  gtk_widget_set_halign (ui->button_stop, GTK_ALIGN_END);
  gtk_widget_set_hexpand (ui->button_stop, FALSE);
  g_signal_connect (ui->button_stop, "clicked", G_CALLBACK (hyscan_gtk_export_on_stop), self);

  ui->label_bottom  = gtk_label_new ("");
  gtk_widget_set_halign (ui->label_bottom, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (ui->label_bottom, TRUE);

  hyscan_gtk_export_gui_tracks_make (self);

  gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (ui->progress_bar), TRUE);

  gtk_grid_attach (GTK_GRID (self), ui->label_top, 0, row++, 3, 1);

  gtk_grid_attach (GTK_GRID (self), ui->project_elem->label, 0, row, 1, 1);
  gtk_grid_attach (GTK_GRID (self), ui->project_elem->entry, 1, row++, 2, 1);

  label = gtk_label_new (ui->track_elem.label_name);
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_set_hexpand (label, FALSE);

  gtk_grid_attach (GTK_GRID (self), label, 0, row, 1, 1);
  gtk_grid_attach (GTK_GRID (self), ui->track_elem.combo, 1, row++, 2, 1);

  gtk_grid_attach (GTK_GRID (self), ui->out_path_elem->label, 0, row, 1, 1);
  gtk_grid_attach (GTK_GRID (self), ui->out_path_elem->entry, 1, row, 1, 1);
  gtk_grid_attach (GTK_GRID (self), ui->out_path_elem->button, 2, row++, 1, 1);

  ui->nmea_elem[HYSCAN_NMEA_TYPE_RMC].label_name = g_strdup ("RMC:");
  ui->nmea_elem[HYSCAN_NMEA_TYPE_GGA].label_name = g_strdup ("GGA:");
  ui->nmea_elem[HYSCAN_NMEA_TYPE_HDT].label_name = g_strdup ("HDT:");
  ui->nmea_elem[HYSCAN_NMEA_TYPE_DPT].label_name = g_strdup ("DPT:");

  i = 0;
  while (i < HYSCAN_NMEA_TYPE_LAST)
    {
      GtkWidget *label = gtk_label_new (ui->nmea_elem[i].label_name);
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      gtk_widget_set_hexpand (label, FALSE);

      gtk_grid_attach (GTK_GRID (self), label, 0, row, 1, 1);
      ui->nmea_elem[i].combo = gtk_combo_box_text_new ();
      gtk_widget_set_halign (ui->nmea_elem[i].combo, GTK_ALIGN_FILL);
      gtk_widget_set_hexpand (ui->nmea_elem[i].combo, TRUE);
      gtk_widget_set_tooltip_text (ui->nmea_elem[i].combo, _("Select NMEA source"));
      gtk_grid_attach (GTK_GRID (self), ui->nmea_elem[i++].combo, 1, row++, 2, 1);
    }

  gtk_grid_attach (GTK_GRID (self), ui->label_execute, 0, row++, 3, 1);
  gtk_grid_attach (GTK_GRID (self), ui->progress_bar, 0, row++, 3, 1);
  gtk_grid_attach (GTK_GRID (self), ui->label_bottom, 0, row++, 3, 1);
  gtk_grid_attach (GTK_GRID (self), ui->button_run, 0, row, 1, 1);
  gtk_grid_attach (GTK_GRID (self), ui->button_stop, 2, row++, 1, 1);

  hyscan_gtk_export_gui_nmea_select_make (self);
}

/* Состояние в UI label */
static void
hyscan_gtk_export_msg_set (HyScanGtkExport *self,
                           const gchar     *message,
                           ...)
{
  va_list args;
  va_start (args, message);

  g_clear_pointer (&self->priv->message, g_free);

  self->priv->message = g_strdup_vprintf (message, args);
  gtk_label_set_text (GTK_LABEL (self->priv->ui->label_bottom), self->priv->message);
  va_end (args);
}

/* Нажатие кнопки на HyScanGtkExportGUIElem */
static void
hyscan_gtk_export_on_select (GtkWidget       *sender,
                             HyScanGtkExport *self)
{ 
  hyscan_gtk_export_on_select_path (sender, self);
}

/* Выбор конечного пути для файлов */
static void
hyscan_gtk_export_on_select_path (GtkWidget       *sender,
                                  HyScanGtkExport *self)
{
  gchar *filename = NULL;
  HyScanGtkExportGUI *ui = NULL;
  GtkWidget *dialog;
  HyScanGtkExportPrivate *priv = self->priv;
  GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
  ui = priv->ui;

  g_debug ("Select out_path");

  dialog = gtk_file_chooser_dialog_new ("Select Folder",
                                        NULL,
                                        action,
                                        _("_Cancel"),
                                        GTK_RESPONSE_CANCEL,
                                        _("_Open"),
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

  if (GTK_RESPONSE_ACCEPT == gtk_dialog_run (GTK_DIALOG (dialog)))
    {
      
      GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
      filename = gtk_file_chooser_get_filename (chooser);
    }
  else
    {
      filename = g_strdup (".");
    }

  gtk_entry_set_text (GTK_ENTRY (ui->out_path_elem->entry), filename);
  gtk_widget_destroy (dialog); 
  g_free (filename);
}

/* Получение из UI списка галсов и отправка их в очередь конвертации */
static void
hyscan_gtk_export_on_select_track (GtkComboBox     *box,
                                   HyScanGtkExport *self)
{
  /* TODO Make select tracks & put result to entry */
  g_debug ("On select track names");
  
  gchar *track = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (box));
  if (track == NULL)
    return;

  g_strfreev (self->priv->track_names);
  self->priv->track_names = g_strsplit (track, ",", -1);

  hyscan_gtk_export_gui_nmea_select_make (self);

}

static void
hyscan_gtk_export_track_to_queue (HyScanGtkExport *self)
{
  // HyScanGtkExportGUI *ui = NULL;
  HyScanGtkExportPrivate *priv = self->priv;
  // ui = priv->ui;
  gint i = 0;

/*  g_strfreev (priv->track_names);
  priv->track_names = g_strsplit (gtk_combo_box_text_get_active_text (
                                  GTK_COMBO_BOX_TEXT (ui->track_elem.combo)),",", -1);
*/
  if (priv->track_names == NULL)
    return;
  
  g_debug ("Add tracks to convert queue");
  while (priv->track_names[i] != NULL)
    g_queue_push_head (&priv->track_queue, priv->track_names[i++]);
}

static void
hyscan_gtk_export_path_to_converter (HyScanGtkExport *self)
{
  HyScanGtkExportGUI *ui = NULL;
  HyScanGtkExportPrivate *priv = self->priv;
  ui = priv->ui;
  /* Сообщим конвертеру новый путь для файлов */
  hyscan_hsx_converter_set_out_path (priv->converter, 
                                     gtk_entry_get_text (GTK_ENTRY (ui->out_path_elem->entry)));
}

static void
hyscan_gtk_export_nmea_to_converter (HyScanGtkExport *self)
{
  HyScanGtkExportGUI *ui = NULL;
  HyScanGtkExportPrivate *priv = self->priv;
  gint i = 0;
  ui = priv->ui;

  /* Каналы для выбранных источников NMEA отправляются в конвертер */
  for (i = 0; i < HYSCAN_NMEA_TYPE_LAST; ++i)
    {
      gint channel;
      HyScanNmeaDataType type = hyscan_gtk_export_nmeadt2hyscan_nmeadt (i);
      const gchar *nmea_channel = gtk_combo_box_get_active_id (GTK_COMBO_BOX (ui->nmea_elem[i].combo));

      if (nmea_channel == NULL)
        continue;

      channel = g_strtod (nmea_channel, NULL);
      hyscan_hsx_converter_set_nmea_channel (priv->converter, type, channel);

      g_debug ("set NMEA channel %d for type %d", channel, type);
    }
}

/* Пуск конвертации */
static void
hyscan_gtk_export_on_run (GtkWidget       *sender,
                          HyScanGtkExport *self)
{
  /* Параметры конвертации из GUI */
  hyscan_gtk_export_track_to_queue (self);
  hyscan_gtk_export_path_to_converter (self);
  hyscan_gtk_export_nmea_to_converter (self);
  /* Запуск конвертера */
  g_timeout_add (1000, hyscan_gtk_export_run, self);
}

/* Прерывание конвертации */
static void
hyscan_gtk_export_on_stop (GtkWidget       *sender,
                           HyScanGtkExport *self)
{
  HyScanGtkExportPrivate *priv = self->priv;

  if (!hyscan_hsx_converter_stop (priv->converter))
    {
      hyscan_gtk_export_msg_set (self, "Can't stop converting");
    }
  else
    {
      self->priv->current_percent = 0;
      hyscan_gtk_export_update (self);
      g_queue_clear (&priv->track_queue);
      hyscan_gtk_export_msg_set (self, "Convert was stopped");
    }

  g_debug ("Convert stopped by user\n");
}

GtkWidget*
hyscan_gtk_export_new (HyScanDB    *db,
                       const gchar *project_name,
                       const gchar *tracks)
{
  HyScanGtkExport *export = NULL;
  if (db == NULL || project_name == NULL)
    return NULL;

  export =  g_object_new (HYSCAN_TYPE_GTK_EXPORT, 
                          "db", db,
                          "project", project_name,
                          "tracks", tracks,
                          NULL);

  g_debug ("Before make GUI\n");
  hyscan_gtk_export_gui_make (export);
  g_debug ("After make GUI\n");

  return GTK_WIDGET(export);
}

void
hyscan_gtk_export_set_watch_period (HyScanGtkExport *self,
                                    guint            period)
{
  HyScanGtkExportPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_EXPORT (self));
  priv = self->priv;

  if (priv->updater != 0)
    {
      g_source_remove (priv->updater);
      priv->updater = 0;
    }

  priv->period = period;

  if (period == 0)
    return;

  priv->updater = g_timeout_add (period, hyscan_gtk_export_update, self);
}

void
hyscan_gtk_export_set_max_ampl (HyScanGtkExport *self,
                                guint            ampl_val)
{
  g_return_if_fail (HYSCAN_IS_GTK_EXPORT (self));
  hyscan_hsx_converter_set_max_ampl (self->priv->converter, ampl_val);
}

void
hyscan_gtk_export_set_image_prm (HyScanGtkExport *self,
                                 gfloat           black,
                                 gfloat           white,
                                 gfloat           gamma)
{
  g_return_if_fail (HYSCAN_IS_GTK_EXPORT (self));
  hyscan_hsx_converter_set_image_prm (self->priv->converter, black, white, gamma);
}

void
hyscan_gtk_export_set_velosity (HyScanGtkExport *self,
                                gfloat           velosity)
{
  g_return_if_fail (HYSCAN_IS_GTK_EXPORT (self));
  hyscan_hsx_converter_set_velosity (self->priv->converter, velosity);
}

gboolean
hyscan_gtk_export_init_crs (HyScanGtkExport *self,
                            const gchar     *src_projection_id,
                            const gchar     *src_datum_id)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_EXPORT (self), FALSE);
  return hyscan_hsx_converter_init_crs (self->priv->converter, src_projection_id, src_datum_id);
}
