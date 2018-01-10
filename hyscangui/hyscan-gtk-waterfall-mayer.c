#include "hyscan-gtk-waterfall-mayer.h"
#include "hyscan-gtk-waterfall-mayer-private.h"

enum
{
  PROP_WATERFALL = 1
};

static void    hyscan_gtk_waterfall_mayer_set_property             (GObject               *object,
                                                                    guint                  prop_id,
                                                                    const GValue          *value,
                                                                    GParamSpec            *pspec);
static void    hyscan_gtk_waterfall_mayer_object_constructed       (GObject               *object);
static void    hyscan_gtk_waterfall_mayer_object_finalize          (GObject               *object);

static gpointer hyscan_gtk_waterfall_mayer_thread                  (gpointer               data);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (HyScanGtkWaterfallMayer, hyscan_gtk_waterfall_mayer, G_TYPE_OBJECT);

static void
hyscan_gtk_waterfall_mayer_class_init (HyScanGtkWaterfallMayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_waterfall_mayer_set_property;

  object_class->constructed = hyscan_gtk_waterfall_mayer_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_mayer_object_finalize;

  g_object_class_install_property (object_class, PROP_WATERFALL,
    g_param_spec_object ("waterfall", "Waterfall", "Waterfall widget",
                         HYSCAN_TYPE_GTK_WATERFALL_STATE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_waterfall_mayer_init (HyScanGtkWaterfallMayer *self)
{
  self->priv = hyscan_gtk_waterfall_mayer_get_instance_private (self);
}

static void
hyscan_gtk_waterfall_mayer_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanGtkWaterfallMayer *self = HYSCAN_GTK_WATERFALL_MAYER (object);
  HyScanGtkWaterfallMayerPrivate *priv = self->priv;

  if (prop_id == PROP_WATERFALL)
    {
      priv->wfall = g_value_dup_object (value);
      priv->wf_state = HYSCAN_GTK_WATERFALL_STATE (priv->wfall);
      priv->carea = GTK_CIFRO_AREA (priv->wfall);
    }
  else
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hyscan_gtk_waterfall_mayer_object_constructed (GObject *object)
{
  // HyScanGtkWaterfallMayer *self = HYSCAN_GTK_WATERFALL_MAYER (object);
  // HyScanGtkWaterfallMayerPrivate *priv = self->priv;
}

static void
hyscan_gtk_waterfall_mayer_object_finalize (GObject *object)
{
  HyScanGtkWaterfallMayer *self = HYSCAN_GTK_WATERFALL_MAYER (object);
  HyScanGtkWaterfallMayerPrivate *priv = self->priv;

  g_object_unref (priv->wfall);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_mayer_parent_class)->finalize (object);
}

void
hyscan_gtk_waterfall_mayer_grab_input (HyScanGtkWaterfallMayer *self)
{
  HyScanGtkWaterfallMayerClass *klass;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MAYER (self));

  klass = HYSCAN_GTK_WATERFALL_MAYER_GET_CLASS (self);

  if (klass->grab_input != NULL)
    (* klass->grab_input) (self);

}

void
hyscan_gtk_waterfall_mayer_set_visible (HyScanGtkWaterfallMayer *self,
                                        gboolean                 visible)
{
  HyScanGtkWaterfallMayerClass *klass;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MAYER (self));

  klass = HYSCAN_GTK_WATERFALL_MAYER_GET_CLASS (self);

  if (klass->set_visible != NULL)
    (* klass->set_visible) (self, visible);

}

const gchar*
hyscan_gtk_waterfall_mayer_get_mnemonic (HyScanGtkWaterfallMayer *self)
{
  HyScanGtkWaterfallMayerClass *klass;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MAYER (self));

  klass = HYSCAN_GTK_WATERFALL_MAYER_GET_CLASS (self);

  if (klass->get_mnemonic != NULL)
    return (* klass->get_mnemonic) (self);

   return NULL;
}
