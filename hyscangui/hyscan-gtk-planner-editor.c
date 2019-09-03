#include <glib/gi18n-lib.h>
#include "hyscan-gtk-planner-editor.h"

enum
{
  PROP_O,
  PROP_MODEL,
  PROP_SELECTION,
};

typedef struct
{
  guint    n_items;

  gdouble  start_lat;
  gdouble  start_lon;
  gdouble  end_lat;
  gdouble  end_lon;

  gboolean start_lat_diffs;
  gboolean start_lon_diffs;
  gboolean end_lat_diffs;
  gboolean end_lon_diffs;
} HyScanGtkPlannerEditorValue;

struct _HyScanGtkPlannerEditorPrivate
{
  HyScanObjectModel *model;
  GListModel        *selection;
  gchar             *object_id;
  gboolean           block_value_change;
  guint              n_items;
  
  GHashTable        *objects;

  GtkWidget         *label;
  GtkWidget         *start_lat;
  GtkWidget         *start_lat_btn;
  GtkWidget         *start_lon;
  GtkWidget         *start_lon_btn;
  GtkWidget         *end_lat;
  GtkWidget         *end_lat_btn;
  GtkWidget         *end_lon;
  GtkWidget         *end_lon_btn;
};

static void    hyscan_gtk_planner_editor_set_property             (GObject                *object,
                                                                   guint                   prop_id,
                                                                   const GValue           *value,
                                                                   GParamSpec             *pspec);
static void    hyscan_gtk_planner_editor_object_constructed       (GObject                *object);
static void    hyscan_gtk_planner_editor_object_finalize          (GObject                *object);
static void    hyscan_gtk_planner_editor_selection_changed        (HyScanGtkPlannerEditor *editor,
                                                                   guint                   position,
                                                                   guint                   removed,
                                                                   guint                   added);
static void    hyscan_gtk_planner_editor_model_changed            (HyScanGtkPlannerEditor *editor);
static void    hyscan_gtk_planner_editor_value_changed            (HyScanGtkPlannerEditor *editor);
static void    hyscan_gtk_planner_editor_update_view              (HyScanGtkPlannerEditor *editor);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkPlannerEditor, hyscan_gtk_planner_editor, GTK_TYPE_GRID)

static void
hyscan_gtk_planner_editor_class_init (HyScanGtkPlannerEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_planner_editor_set_property;
  object_class->constructed = hyscan_gtk_planner_editor_object_constructed;
  object_class->finalize = hyscan_gtk_planner_editor_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "HyScanObjectModel", "HyScanObjectModel with planner data",
                         HYSCAN_TYPE_OBJECT_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SELECTION,
    g_param_spec_object ("selection", "GListModel", "GListModel with C-string ID of objects",
                         G_TYPE_LIST_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_planner_editor_init (HyScanGtkPlannerEditor *editor)
{
  editor->priv = hyscan_gtk_planner_editor_get_instance_private (editor);
}

static void
hyscan_gtk_planner_editor_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  HyScanGtkPlannerEditor *editor = HYSCAN_GTK_PLANNER_EDITOR (object);
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->model = g_value_dup_object (value);
      break;

    case PROP_SELECTION:
      priv->selection = g_value_dup_object (value);
      if (g_list_model_get_item_type (priv->selection) != G_TYPE_STRING)
        {
          g_warning ("HyScanGtkPlannerEditor: GListModel must contain items of type G_TYPE_STRING");
          g_clear_object (&priv->selection);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_planner_editor_object_constructed (GObject *object)
{
  HyScanGtkPlannerEditor *editor = HYSCAN_GTK_PLANNER_EDITOR (object);
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  gsize i = 0;

  GtkGrid *grid = GTK_GRID (object);
  GtkWidget *spin_btns[4];
  GtkWidget *check_btns[4];

  G_OBJECT_CLASS (hyscan_gtk_planner_editor_parent_class)->constructed (object);

  gtk_grid_set_row_spacing (grid, 3);
  gtk_grid_set_column_spacing (grid, 3);

  priv->label = gtk_label_new (NULL);
  priv->start_lat = gtk_spin_button_new_with_range (-90.0, 90.0, 0.0001);
  priv->start_lon = gtk_spin_button_new_with_range (-180.0, 180.0, 0.0001);
  priv->end_lat = gtk_spin_button_new_with_range (-90.0, 90.0, 0.0001);
  priv->end_lon = gtk_spin_button_new_with_range (-180.0, 180.0, 0.0001);
  priv->start_lat_btn = gtk_check_button_new ();
  priv->start_lon_btn = gtk_check_button_new ();
  priv->end_lat_btn = gtk_check_button_new ();
  priv->end_lon_btn = gtk_check_button_new ();
  gtk_grid_attach (grid, priv->label, 0, ++i, 2, 1);
  gtk_grid_attach (grid, gtk_label_new ("Start lat"), 0, ++i, 1, 1);
  gtk_grid_attach (grid, priv->start_lat,             1,   i, 1, 1);
  gtk_grid_attach (grid, priv->start_lat_btn,         2,   i, 1, 1);
  gtk_grid_attach (grid, gtk_label_new ("Start lon"), 0, ++i, 1, 1);
  gtk_grid_attach (grid, priv->start_lon,             1,   i, 1, 1);
  gtk_grid_attach (grid, priv->start_lon_btn,         2,   i, 1, 1);
  gtk_grid_attach (grid, gtk_label_new ("End lat"),   0, ++i, 1, 1);
  gtk_grid_attach (grid, priv->end_lat,               1,   i, 1, 1);
  gtk_grid_attach (grid, priv->end_lat_btn,           2,   i, 1, 1);
  gtk_grid_attach (grid, gtk_label_new ("End lon"),   0, ++i, 1, 1);
  gtk_grid_attach (grid, priv->end_lon,               1,   i, 1, 1);
  gtk_grid_attach (grid, priv->end_lon_btn,           2,   i, 1, 1);

  spin_btns[0] = priv->start_lat;
  spin_btns[1] = priv->start_lon;
  spin_btns[2] = priv->end_lat;
  spin_btns[3] = priv->end_lon;
  check_btns[0] = priv->start_lat_btn;
  check_btns[1] = priv->start_lon_btn;
  check_btns[2] = priv->end_lat_btn;
  check_btns[3] = priv->end_lon_btn;

  for (i = 0; i < G_N_ELEMENTS (spin_btns); ++i)
    {
      gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin_btns[i]), FALSE);
      g_object_bind_property (check_btns[i], "active",
                              spin_btns[i], "sensitive", G_BINDING_SYNC_CREATE);
      g_signal_connect_swapped (spin_btns[i], "value-changed",
                                G_CALLBACK (hyscan_gtk_planner_editor_value_changed), editor);
    }

  g_signal_connect_swapped (priv->selection, "items-changed", G_CALLBACK (hyscan_gtk_planner_editor_selection_changed),
                            editor);
  g_signal_connect_swapped (priv->model, "changed", G_CALLBACK (hyscan_gtk_planner_editor_model_changed), editor);

  hyscan_gtk_planner_editor_update_view (editor);
}

static void
hyscan_gtk_planner_editor_object_finalize (GObject *object)
{
  HyScanGtkPlannerEditor *editor = HYSCAN_GTK_PLANNER_EDITOR (object);
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;

  g_signal_handlers_disconnect_by_data (priv->selection, editor);
  g_signal_handlers_disconnect_by_data (priv->model, editor);
  g_clear_pointer (&priv->objects, g_hash_table_destroy);
  g_clear_object (&priv->model);
  g_clear_object (&priv->selection);
  g_free (priv->object_id);

  G_OBJECT_CLASS (hyscan_gtk_planner_editor_parent_class)->finalize (object);
}

static HyScanGtkPlannerEditorValue
hyscan_gtk_planner_editor_get_value (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorValue value;

  guint i;

  memset (&value, 0, sizeof (value));

  if (priv->objects == NULL)
    return value;

  for (i = 0; i < priv->n_items; ++i)
    {
      HyScanPlannerObject *object;
      HyScanPlannerTrack *track;
      gchar *id;

      id = g_list_model_get_item (priv->selection, i);
      object = g_hash_table_lookup (priv->objects, id);
      if (object == NULL || object->type != HYSCAN_PLANNER_TRACK)
        {
          g_free (id);
          continue;
        }

      track = &object->track;

      if (value.n_items == 0)
        {
          value.start_lat = track->start.lat;
          value.start_lon = track->start.lon;
          value.end_lat = track->end.lat;
          value.end_lon = track->end.lon;
        }
      else
        {
          if (value.start_lat != track->start.lat)
            value.start_lat_diffs = TRUE;

          if (value.start_lon != track->start.lon)
            value.start_lon_diffs = TRUE;

          if (value.end_lat != track->end.lat)
            value.end_lat_diffs = TRUE;

          if (value.end_lon != track->end.lon)
            value.end_lon_diffs = TRUE;
        }

      value.n_items++;
      g_free (id);
    }

  return value;
}

static void
hyscan_gtk_planner_editor_update_view (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorValue value;
  gboolean value_set;

  gchar *label_text;

  priv->n_items = g_list_model_get_n_items (priv->selection);
  value = hyscan_gtk_planner_editor_get_value (editor);

  priv->block_value_change = TRUE;

  value_set = (value.n_items > 0);

  if (value_set)
    {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->start_lat), value.start_lat);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->start_lon), value.start_lon);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->end_lat),   value.end_lat);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->end_lon),   value.end_lon);
    }
  else
    {
      gtk_entry_set_text (GTK_ENTRY (priv->start_lat), "");
      gtk_entry_set_text (GTK_ENTRY (priv->start_lon), "");
      gtk_entry_set_text (GTK_ENTRY (priv->end_lat), "");
      gtk_entry_set_text (GTK_ENTRY (priv->end_lon), "");
    }

  gtk_widget_set_sensitive (priv->start_lat_btn, value_set);
  gtk_widget_set_sensitive (priv->start_lon_btn, value_set);
  gtk_widget_set_sensitive (priv->end_lat_btn, value_set);
  gtk_widget_set_sensitive (priv->end_lon_btn, value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->start_lat_btn), !value.start_lat_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->start_lon_btn), !value.start_lon_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->end_lat_btn), !value.end_lat_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->end_lon_btn), !value.end_lon_diffs && value_set);

  priv->block_value_change = FALSE;

  label_text = g_strdup_printf (_("%d items selected"), value.n_items);
  gtk_label_set_text (GTK_LABEL (priv->label), label_text);

  g_free (label_text);
}

static void
hyscan_gtk_planner_editor_selection_changed (HyScanGtkPlannerEditor *editor,
                                             guint                   position,
                                             guint                   removed,
                                             guint                   added)
{
  hyscan_gtk_planner_editor_update_view (editor);
}

static void
hyscan_gtk_planner_editor_value_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  guint i;

  if (priv->block_value_change || priv->objects == NULL)
    return;

  for (i = 0; i < priv->n_items; ++i)
    {
      HyScanPlannerTrack *track;
      gchar *id;

      id = g_list_model_get_item (priv->selection, i);
      track = g_hash_table_lookup (priv->objects, id);
      if (track == NULL || track->type != HYSCAN_PLANNER_TRACK)
        {
          g_free (id);
          continue;
        }

      if (gtk_widget_get_sensitive (priv->start_lat))
        track->start.lat = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->start_lat));
      if (gtk_widget_get_sensitive (priv->start_lon))
        track->start.lon = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->start_lon));
      if (gtk_widget_get_sensitive (priv->end_lat))
        track->end.lat = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->end_lat));
      if (gtk_widget_get_sensitive (priv->end_lon))
        track->end.lon = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->end_lon));

      hyscan_object_model_modify_object (priv->model, id, track);
    }
}

static void
hyscan_gtk_planner_editor_model_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;

  g_clear_pointer (&priv->objects, g_hash_table_destroy);
  priv->objects = hyscan_object_model_get (priv->model);

  hyscan_gtk_planner_editor_update_view (editor);
}

GtkWidget *
hyscan_gtk_planner_editor_new (HyScanObjectModel *model,
                               GListModel        *selection)
{
  return g_object_new (HYSCAN_TYPE_GTK_PLANNER_EDITOR,
                       "model", model,
                       "selection", selection,
                       NULL);
}
