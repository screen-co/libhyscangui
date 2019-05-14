#include <hyscan-map-tile-loader.h>

static HyScanGtkMapTileGrid *grid;          /* Сетка тайлов. */
static guint xnums[] = { 1, 2, 4, 10 };     /* Масштабы сетки. */

static gdouble loader_fraction;

static int filled_count   = 0;
static int expected_count = 121;            /* Количество тайлов на всех масштабах: 1*1 + 2*2 + 4*4 + 10 * 10. */

typedef struct
{
  GObject parent_instance;
} DummyTileSource;

typedef struct
{
  GObjectClass parent_class;
} DummyTileSourceClass;

static gboolean
dummy_tile_source_fill_tile (HyScanGtkMapTileSource *source,
                             HyScanGtkMapTile       *tile,
                             GCancellable           *cancellable)
{
  g_atomic_int_inc (&filled_count);

  return TRUE;
}

static HyScanGtkMapTileGrid *
dummy_tile_source_get_grid (HyScanGtkMapTileSource *source)
{
  return g_object_ref (grid);
}

static void
dummy_tile_source_interface_init (HyScanGtkMapTileSourceInterface *iface)
{
  iface->fill_tile = dummy_tile_source_fill_tile;
  iface->get_grid = dummy_tile_source_get_grid;
}

static void
dummy_tile_source_class_init (DummyTileSourceClass *klass)
{

}

static void
dummy_tile_source_init (DummyTileSource *source)
{

}

G_DEFINE_TYPE_WITH_CODE (DummyTileSource, dummy_tile_source, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_MAP_TILE_SOURCE, dummy_tile_source_interface_init))


static void
loader_progress (HyScanMapTileLoader *loader,
                 gdouble              fraction)
{
  loader_fraction = fraction;
}

int
main (int argc,
      char **argv)
{
  HyScanMapTileLoader *loader;
  HyScanGtkMapTileSource *source;
  GThread *thread;

  /* Создаем dummy-источник тайлов. */
  grid = hyscan_gtk_map_tile_grid_new (-1.0, 1.0, -1.0, 1.0, 0, 256);
  hyscan_gtk_map_tile_grid_set_xnums (grid, xnums, G_N_ELEMENTS (xnums));
  source = g_object_new (dummy_tile_source_get_type (), NULL);

  /* Создаем загрузчик. */
  loader = hyscan_map_tile_loader_new ();
  g_signal_connect (loader, "progress", G_CALLBACK (loader_progress), NULL);

  /* Запускаем загрузку. */
  thread = hyscan_map_tile_loader_start (loader, source, -0.99, 0.99, -0.99, 0.99);

  g_object_unref (source);
  g_object_unref (loader);

  g_thread_join (thread);

  /* Проверяем, что все тайлы загрузились. */
  g_assert_cmpint (filled_count, ==, expected_count);
  g_assert_cmpfloat (loader_fraction, ==, 1.0);

  g_message ("Test done successfully!");

  return 0;
}