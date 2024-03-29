/* hyscan-gtk-gliko-grid.с
 *
 * Copyright 2020-2021 Screen LLC, Vladimir Sharov <sharovv@mail.ru>
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

#include <epoxy/gl.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hyscan-gtk-gliko-grid.h"

/* Properties enum */
enum
{
  P_RESERVED,
  P_CX,     // координата X центра
  P_CY,     // координата Y центра
  P_SCALE,  // масштаб
  P_RADIUS, // радиус развертки, м
  P_STEP,   // текущий шаг разметки по дальности, м
  P_ALPHA,  // угол поворота, градусы
  P_COLOR,
  P_COLOR_ALPHA,
  N_PROPERTIES
};

#define MAX_CIRCLES 64

typedef struct _HyScanGtkGlikoGridPrivate HyScanGtkGlikoGridPrivate;

struct _HyScanGtkGlikoGrid
{
  GObject parent;
  HyScanGtkGlikoGridPrivate *priv;
};

struct _HyScanGtkGlikoGridClass
{
  GObjectClass parent_class;
};

struct _HyScanGtkGlikoGridPrivate
{
  float cx;
  float cy;
  float scale;
  float scale0;
  float radius;
  float radius0;
  float step;
  float alpha;
  int w, h;
  int bold;         // количество малых делений разметки по дальности
  int program;      // shader program
  unsigned int vao; // vertex array object
  unsigned int vbo; // vertex buffer object
  float vertices[360 * 3 + 24 * 3];
  float circles[MAX_CIRCLES];
  int num_circles; // количество окружностей в сетке
  float color[4];
};

#define glerr()                                                                   \
  {                                                                               \
    GLenum e;                                                                     \
    if ((e = glGetError ()) != GL_NO_ERROR)                                       \
      {                                                                           \
        fprintf (stderr, "%s(%d): GL error 0x%X\n", __FILE__, __LINE__, (int) e); \
        fflush (stderr);                                                          \
      }                                                                           \
  }

static void layer_interface_init (HyScanGtkGlikoLayerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkGlikoGrid, hyscan_gtk_gliko_grid, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_GLIKO_LAYER, layer_interface_init))

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL };

/* Internal API */
static void set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static void update_circles (HyScanGtkGlikoGridPrivate *p);

static const char *vertexShaderSource =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform float cx;\n"
    "uniform float cy;\n"
    "uniform float scale;\n"
    "uniform float width;\n"
    "uniform float height;\n"
    "uniform float radius;\n"
    "uniform float sin_alpha;\n"
    "uniform float cos_alpha;\n"
    "void main()\n"
    "{\n"
    "   float x = cos_alpha * aPos.x - sin_alpha * aPos.y;\n"
    "   float y = sin_alpha * aPos.x + cos_alpha * aPos.y;\n"
    "   gl_Position = vec4((2.0 * cx + x * radius) * scale * height / width, (2.0 * cy + y * radius) * scale, aPos.z, 1.0);\n"
    "}\n";

static const char *fragmentShaderSource =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec4 color;\n"
    "void main()\n"
    "{\n"
    "   FragColor = color;\n"
    "}\n";

static void
set_uniform1f (const unsigned int prog, const char *name, const float val)
{
  int loc = glGetUniformLocation (prog, name);
  if (loc == -1)
    {
      glerr ();
      return;
    }
  glUniform1f (loc, val);
}

static void
set_uniform4fv (const unsigned int prog, const char *name, const float *a)
{
  int loc = glGetUniformLocation (prog, name);
  if (loc == -1)
    {
      glerr ();
      return;
    }
  glUniform4fv (loc, 1, a);
}

static int
create_shader_program ()
{
  // build and compile our shader program
  // ------------------------------------
  // vertex shader
  int vertexShader = glCreateShader (GL_VERTEX_SHADER);
  glShaderSource (vertexShader, 1, &vertexShaderSource, NULL);
  glCompileShader (vertexShader);
  // check for shader compile errors
  int success;
  char infoLog[512];
  glGetShaderiv (vertexShader, GL_COMPILE_STATUS, &success);
  if (!success)
    {
      glGetShaderInfoLog (vertexShader, sizeof (infoLog), NULL, infoLog);
      fprintf (stderr, "Vertex shader compile error: %s\n", infoLog);
      return -1;
    }
  // fragment shader
  int fragmentShader = glCreateShader (GL_FRAGMENT_SHADER);
  glShaderSource (fragmentShader, 1, &fragmentShaderSource, NULL);
  glCompileShader (fragmentShader);
  // check for shader compile errors
  glGetShaderiv (fragmentShader, GL_COMPILE_STATUS, &success);
  if (!success)
    {
      glGetShaderInfoLog (fragmentShader, sizeof (infoLog), NULL, infoLog);
      fprintf (stderr, "Fragment compile error: %s\n", infoLog);
      return -1;
    }
  // link shaders
  int shaderProgram = glCreateProgram ();
  glAttachShader (shaderProgram, vertexShader);
  glAttachShader (shaderProgram, fragmentShader);
  glLinkProgram (shaderProgram);
  // check for linking errors
  glGetProgramiv (shaderProgram, GL_LINK_STATUS, &success);
  if (!success)
    {
      glGetProgramInfoLog (shaderProgram, sizeof (infoLog), NULL, infoLog);
      fprintf (stderr, "Link program error: %s\n", infoLog);
      return -1;
    }
  glDeleteShader (vertexShader);
  glDeleteShader (fragmentShader);
  return shaderProgram;
}

// VAO - vertex array object
static void
create_model (HyScanGtkGlikoGridPrivate *p)
{
  // создаем объекты массива вершин и буфера вершин
  glGenVertexArrays (1, &p->vao);
  glGenBuffers (1, &p->vbo);

  // выбираем объект массива вершин
  glBindVertexArray (p->vao);
  // привязываем к нему буфер данных
  glBindBuffer (GL_ARRAY_BUFFER, p->vbo);

  // задаем размер буфера
  glBufferData (GL_ARRAY_BUFFER, sizeof (p->vertices), p->vertices, GL_DYNAMIC_DRAW);
  // одна вершина хранится как вектор из 3х чисел с плавающей точкой
  glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof (float), (void *) 0);

  // отменяем выбор объектов
  glEnableVertexAttribArray (0);
  glBindBuffer (GL_ARRAY_BUFFER, 0);
  glBindVertexArray (0);
}

static void
layer_resize (HyScanGtkGlikoLayer *layer, gint width, gint height)
{
  HyScanGtkGlikoGridPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (layer, HYSCAN_TYPE_GTK_GLIKO_GRID, HyScanGtkGlikoGridPrivate);

  p->w = width;
  p->h = height;
}

static void
layer_render (HyScanGtkGlikoLayer *layer, GdkGLContext *context)
{
  HyScanGtkGlikoGridPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (layer, HYSCAN_TYPE_GTK_GLIKO_GRID, HyScanGtkGlikoGridPrivate);
  int i, j;
  float a;
  float c1[4];
  float c2[4];

  if (p->program == -1)
    return;

  c1[0] = p->color[0];
  c1[1] = p->color[1];
  c1[2] = p->color[2];
  c1[3] = p->color[3];

  c2[0] = c1[0];
  c2[1] = c1[1];
  c2[2] = c1[2];
  c2[3] = 0.5f * c1[3];

  update_circles (p);

  // не требуется для рисования линиями
  //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

  // вся сетка состоит из линий, задаем параметры линий
  glEnable (GL_LINE_SMOOTH);
  glLineWidth (2.0f);

  // выбираем программу шейдера
  glUseProgram (p->program);

  // задаем параметры программы шейдера
  set_uniform1f (p->program, "cx", p->cx);
  set_uniform1f (p->program, "cy", p->cy);
  set_uniform1f (p->program, "scale", 1.0f / p->scale);
  set_uniform1f (p->program, "width", (float) p->w);
  set_uniform1f (p->program, "height", (float) p->h);

  // угол вращения относительно центра
  a = -p->alpha * G_PI / 180.0;
  set_uniform1f (p->program, "sin_alpha", sinf (a));
  set_uniform1f (p->program, "cos_alpha", cosf (a));

  // выбираем буфер с координатами вершин
  glBindBuffer (GL_ARRAY_BUFFER, p->vbo);
  glBindVertexArray (p->vao);

  glBufferSubData (GL_ARRAY_BUFFER, 360 * 3 * sizeof (float), 12 * 6 * sizeof (float), p->vertices + 360 * 3);

  // рисуем набор окружностей
  for (i = 0, j = 0; i < p->num_circles; i++)
    {
      j++;
      if (j == p->bold || i == (p->num_circles - 1))
        {
          j = 0;
          set_uniform4fv (p->program, "color", c1);
        }
      else
        {
          set_uniform4fv (p->program, "color", c2);
        }
      set_uniform1f (p->program, "radius", p->circles[i]);
      glDrawArrays (GL_LINE_LOOP, 0, 360);
    }
  // рисуем 30-градусные азимутальные линии
  glDrawArrays (GL_LINES, 360, 24);
}

static void
layer_realize (HyScanGtkGlikoLayer *layer)
{
  HyScanGtkGlikoGridPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (layer, HYSCAN_TYPE_GTK_GLIKO_GRID, HyScanGtkGlikoGridPrivate);

  // создаем программу шейдера
  p->program = create_shader_program ();

  // создаем буфер для хранения вершин
  create_model (p);
}

static void
layer_interface_init (HyScanGtkGlikoLayerInterface *iface)
{
  iface->realize = layer_realize;
  iface->render = layer_render;
  iface->resize = layer_resize;
}

static void
update_circles (HyScanGtkGlikoGridPrivate *p)
{
  float step, r0;
  int i, k;
  const float x[3] = { 2.0f, 2.5f, 2.0f };
  const int num[3] = { 5, 5, 5 };
  const int n = 10;
  const float a = G_PI / 180.0f;
  float *line_vertices;

  if (p->radius == p->radius0 && p->scale == p->scale0)
    return;

  p->radius0 = p->radius;
  p->scale0 = p->scale;

  for (step = 0.1f, i = 0, k = num[0]; (step * n / p->scale) < p->radius; i++)
    {
      step *= x[i % 3];
      k = num[i % 3];
    }
  for (i = 0; (step * (i + 1)) < p->radius && i < MAX_CIRCLES - 1; i++)
    {
      p->circles[i] = step * (i + 1) / p->radius;
    }
  p->circles[i++] = 1.0f;
  p->num_circles = i;
  p->step = step;
  p->bold = k;
  r0 = p->step / p->radius;

  // координаты точек окружности
  for (i = 0; i < 360; i++)
    {
      p->vertices[i * 3 + 0] = sinf (a * i);
      p->vertices[i * 3 + 1] = cosf (a * i);
      p->vertices[i * 3 + 2] = 0.0f;
    }

  // координаты 30-градусных азимутальных линий
  line_vertices = p->vertices + 360 * 3;
  for (i = 0; i < 12; i++)
    {
      line_vertices[i * 6 + 0] = r0 * sinf (a * 30.0f * i);
      line_vertices[i * 6 + 1] = r0 * cosf (a * 30.0f * i);
      line_vertices[i * 6 + 2] = 0.0f;
      line_vertices[i * 6 + 3] = sinf (a * 30.0f * i);
      line_vertices[i * 6 + 4] = cosf (a * 30.0f * i);
      line_vertices[i * 6 + 5] = 0.0f;
    }
}

static void
hyscan_gtk_gliko_grid_init (HyScanGtkGlikoGrid *grid)
{
  HyScanGtkGlikoGridPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (grid, HYSCAN_TYPE_GTK_GLIKO_GRID, HyScanGtkGlikoGridPrivate);

  /* Create cache for faster access */
  grid->priv = p;

  p->program = -1;
  p->cx = 0.0f;
  p->cy = 0.0f;
  p->scale = 1.0f;
  p->scale0 = 0.0f;
  p->radius = 100.0f;
  p->radius0 = 0.0f;
  p->alpha = 0.0f;

  p->color[0] = 0.0f;
  p->color[1] = 0.0f;
  p->color[2] = 0.0f;
  p->color[3] = 0.4f;

  update_circles (p);
}

static void
hyscan_gtk_gliko_grid_class_init (HyScanGtkGlikoGridClass *klass)
{
  GObjectClass *g_class = G_OBJECT_CLASS (klass);
  static const int rw = (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_type_class_add_private (klass, sizeof (HyScanGtkGlikoGridPrivate));
  g_class->set_property = set_property;
  g_class->get_property = get_property;

  obj_properties[P_CX] = g_param_spec_float ("gliko-cx", "Center X", "Center point coordinate X", -G_MAXFLOAT, G_MAXFLOAT, 0.0, rw);
  obj_properties[P_CY] = g_param_spec_float ("gliko-cy", "Center Y", "Center point coordinate Y", -G_MAXFLOAT, G_MAXFLOAT, 0.0, rw);
  obj_properties[P_SCALE] = g_param_spec_float ("gliko-scale", "Scale", "Scale of image", -G_MAXFLOAT, G_MAXFLOAT, 0.0, rw);
  obj_properties[P_RADIUS] = g_param_spec_float ("gliko-radius", "Radius", "Radius in meters", 0, G_MAXFLOAT, 100.0, rw);
  obj_properties[P_STEP] = g_param_spec_float ("gliko-step-distance", "Step", "Grid step by distance in meters", 0, G_MAXFLOAT, 10.0, rw);
  obj_properties[P_ALPHA] = g_param_spec_float ("gliko-rotate", "Rotate", "Grid rotate in degrees", -G_MAXFLOAT, G_MAXFLOAT, 0.0, rw);
  obj_properties[P_COLOR_ALPHA] = g_param_spec_float ("gliko-color-alpha", "ColorAlpha", "Alpha channel of color", 0.0, 1.0, 1.0, rw);
  obj_properties[P_COLOR] = g_param_spec_string ("gliko-color-rgb", "Color", "Color for grid #RRGGBB", "#FFFFFF", rw);

  g_object_class_install_properties (g_class, N_PROPERTIES, obj_properties);
}

static float *
get_pfloat (HyScanGtkGlikoGridPrivate *p, const int id)
{
  switch (id)
    {
    case P_CX:
      return &p->cx;
    case P_CY:
      return &p->cy;
    case P_SCALE:
      return &p->scale;
    case P_RADIUS:
      return &p->radius;
    case P_STEP:
      return &p->step;
    case P_ALPHA:
      return &p->alpha;
    case P_COLOR_ALPHA:
      return &p->color[3];
    default:
      break;
    }
  return NULL;
}

static float *
get_prgb (HyScanGtkGlikoGridPrivate *p, const int id)
{
  switch (id)
    {
    case P_COLOR:
      return p->color;
    default:
      break;
    }
  return NULL;
}

/* Overriden g_object methods */
static void
set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  HyScanGtkGlikoGridPrivate *p = HYSCAN_GTK_GLIKO_GRID (object)->priv;
  float *pf;

  if ((pf = get_pfloat (p, prop_id)) != NULL)
    {
      *pf = g_value_get_float (value);
    }
  else if ((pf = get_prgb (p, prop_id)) != NULL)
    {
      int n, r, g, b;
      const gchar *s = g_value_get_string (value);
      const float frac_1_255 = 0.003921569f;

      n = sscanf (s, "#%2X%2X%2X", &r, &g, &b);
      if (n == 3)
        {
          pf[0] = frac_1_255 * r;
          pf[1] = frac_1_255 * g;
          pf[2] = frac_1_255 * b;
        }
    }
  else
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  HyScanGtkGlikoGridPrivate *p = HYSCAN_GTK_GLIKO_GRID (object)->priv;
  float *pf;

  if ((pf = get_pfloat (p, prop_id)) != NULL)
    {
      g_value_set_float (value, *pf);
    }
  else if ((pf = get_prgb (p, prop_id)) != NULL)
    {
      gchar t[64];

      sprintf (t, "#%02X%02X%02X", (int) (255.0 * pf[0]), (int) (255.0 * pf[1]), (int) (255.0 * pf[2]));
      g_value_set_string (value, t);
    }
  else
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

HYSCAN_API
HyScanGtkGlikoGrid *
hyscan_gtk_gliko_grid_new (void)
{
  return HYSCAN_GTK_GLIKO_GRID (g_object_new (hyscan_gtk_gliko_grid_get_type (), NULL));
}
