#include "hyscan-gtk-export.h"
#include <hyscan-hsx-converter.h>
#include <glib/gi18n.h>
enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJ
};

typedef struct 
{
  GtkWidget    *label;
  GtkWidget    *entry;
  GtkWidget    *button;
  gboolean      editable;
}HyScanGtkExportGUIElem;

typedef struct 
{
  GtkWidget                   *label_top;

  HyScanGtkExportGUIElem      *project_elem;
  HyScanGtkExportGUIElem      *track_elem;
  HyScanGtkExportGUIElem      *out_path_elem;

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
static void        hyscan_gtk_export_gui_make                 (HyScanGtkExport       *self);
static gboolean    hyscan_gtk_export_gui_clear                (HyScanGtkExport       *self);

static void        hyscan_gtk_export_msg_set                  (HyScanGtkExport       *self,
                                                               const gchar           *message,
                                                               ...);

static void        hyscan_gtk_export_on_select                (GtkWidget             *sender,
                                                               HyScanGtkExport       *self);

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

  priv->out_path = g_strdup (".");
  g_debug ("Before make GUI\n");
  hyscan_gtk_export_gui_make (gtk_export);
  g_debug ("After make GUI\n");

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
  g_free (priv->out_path);

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
  /*self->priv->prev_percent = 0;*/
  hyscan_gtk_export_update (self);
  /*g_main_loop_quit (main_loop);*/
}

/* Обновление процентного выполнения */
void 
hyscan_gtk_export_current_percent (HyScanGtkExport *self,
                                   gint             percent)
{
  g_printf ("%d%%\r", percent);
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

  g_debug ("Convert track %s\n", current_track);

  return G_SOURCE_CONTINUE;

remove:
  return G_SOURCE_REMOVE;
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
  if (editable)
    {
      el->button = gtk_button_new_with_label (_("Select"));
      g_signal_connect (el->button, "clicked", 
                        G_CALLBACK (hyscan_gtk_export_on_select), self);
    }
  return el;
}

static void
hyscan_gtk_export_gui_make (HyScanGtkExport *self)
{
  HyScanGtkExportPrivate *priv;
  HyScanGtkExportGUI *ui = NULL;
  gchar *track_str = NULL;
  gint row = 0;

  priv = self->priv;
  //track_str = g_strjoinv (",", priv->track_names);
  track_str = g_strdup ("");

  ui = priv->ui = g_malloc0 (sizeof (HyScanGtkExportGUI));

  ui->label_top     = gtk_label_new (_("HSX Converter"));
  ui->project_elem  = hyscan_gtk_export_gui_elem_make (self, FALSE, "Project: ", priv->project_name);
  ui->track_elem    = hyscan_gtk_export_gui_elem_make (self, TRUE, "Tracks: ", track_str);
  ui->out_path_elem = hyscan_gtk_export_gui_elem_make (self, TRUE, "Out path: ", priv->out_path);
  ui->label_execute = gtk_label_new (_("Execute: "));
  ui->progress_bar  = gtk_progress_bar_new ();
  ui->button_run    = gtk_button_new_with_label (_("Convert"));
  g_signal_connect (ui->button_run, "clicked", G_CALLBACK (hyscan_gtk_export_on_run), self);
  ui->button_stop   = gtk_button_new_with_label (_("Abort"));
  g_signal_connect (ui->button_stop, "clicked", G_CALLBACK (hyscan_gtk_export_on_stop), self);
  ui->label_bottom  = gtk_label_new ("");

  gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (ui->progress_bar), TRUE);
 
  g_free (track_str);

  gtk_grid_attach (GTK_GRID (self), ui->label_top, 0, row++, 3, 1);

  gtk_grid_attach (GTK_GRID (self), ui->project_elem->label, 0, row, 1, 1);
  gtk_grid_attach (GTK_GRID (self), ui->project_elem->entry, 1, row++, 2, 1);

  gtk_grid_attach (GTK_GRID (self), ui->track_elem->label, 0, row, 1, 1);
  gtk_grid_attach (GTK_GRID (self), ui->track_elem->entry, 1, row, 1, 1);
  gtk_grid_attach (GTK_GRID (self), ui->track_elem->button, 2, row++, 1, 1);

  gtk_grid_attach (GTK_GRID (self), ui->out_path_elem->label, 0, row, 1, 1);
  gtk_grid_attach (GTK_GRID (self), ui->out_path_elem->entry, 1, row, 1, 1);
  gtk_grid_attach (GTK_GRID (self), ui->out_path_elem->button, 2, row++, 1, 1);

  gtk_grid_attach (GTK_GRID (self), ui->label_execute, 0, row++, 3, 1);
  gtk_grid_attach (GTK_GRID (self), ui->progress_bar, 0, row++, 3, 1);
  gtk_grid_attach (GTK_GRID (self), ui->label_bottom, 0, row++, 3, 1);
  gtk_grid_attach (GTK_GRID (self), ui->button_run, 0, row, 1, 1);
  gtk_grid_attach (GTK_GRID (self), ui->button_stop, 2, row++, 1, 1);
}

static gboolean
hyscan_gtk_export_gui_clear (HyScanGtkExport *self)
{
  HyScanGtkExportGUI *ui = NULL;
  ui = self->priv->ui;
  
  if (ui == NULL)
    return FALSE;

  /* TODO What do I need free here? */
}

static void
hyscan_gtk_export_msg_set (HyScanGtkExport *self,
                           const gchar     *message,
                           ...)
{
  va_list args;
  va_start (args, message);

  g_clear_pointer (&self->priv->message, g_free);

  self->priv->message = g_strdup_vprintf (message, args);
  gtk_label_set_text (GTK_LABEL (self->priv->ui->label_bottom), message);
  va_end (args);
}

static void
hyscan_gtk_export_on_select (GtkWidget       *sender,
                             HyScanGtkExport *self)
{
  HyScanGtkExportGUI *ui = NULL;
  HyScanGtkExportPrivate *priv = self->priv;
  ui = priv->ui;

  if (ui->track_elem->button == sender)
    {
      gint i = 0;
      g_debug ("Select track names\n");
      g_strfreev (priv->track_names);
      /* PUT track names to entry */;
      priv->track_names = g_strsplit (gtk_entry_get_text (GTK_ENTRY (ui->track_elem->entry)), ",", -1);

      while (priv->track_names[i] != NULL)
        g_queue_push_head (&priv->track_queue, priv->track_names[i++]);
    }
  else if (ui->out_path_elem->button == sender)
    {
      g_debug ("Select out_path\n");
      hyscan_hsx_converter_set_out_path (priv->converter, 
                                         gtk_entry_get_text (GTK_ENTRY (ui->out_path_elem->entry)));
    }
      
}

static void
hyscan_gtk_export_on_run (GtkWidget       *sender,
                          HyScanGtkExport *self)
{
  g_timeout_add (1000, hyscan_gtk_export_run, self);
}

static void
hyscan_gtk_export_on_stop (GtkWidget       *sender,
                           HyScanGtkExport *self)
{
  HyScanGtkExportPrivate *priv = self->priv;

  if (!hyscan_hsx_converter_stop (priv->converter))
    hyscan_gtk_export_msg_set (self, "Can't stop converting");
  else
    hyscan_gtk_export_msg_set (self, "Convert was stopped");

  g_debug ("Convert stopped by user\n");
}

GtkWidget*
hyscan_gtk_export_new (HyScanDB    *db,
                       const gchar *project_name)
{
  HyScanGtkExport *export = NULL;
  if (db == NULL || project_name == NULL)
    return NULL;

  export =  g_object_new (HYSCAN_TYPE_GTK_EXPORT, 
                          "db", db,
                          "project", project_name,
                          NULL);

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
