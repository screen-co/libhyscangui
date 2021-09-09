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

#include "hyscan-gtk-gliko-drawing.h"

#define DRAW_EXAMPLE 0

typedef struct _HyScanGtkGlikoDrawingPrivate HyScanGtkGlikoDrawingPrivate;

struct _HyScanGtkGlikoDrawing
{
  GObject parent;
  HyScanGtkGlikoDrawingPrivate *priv;
};

struct _HyScanGtkGlikoDrawingClass
{
  GObjectClass parent_class;
};

struct _HyScanGtkGlikoDrawingPrivate
{
  GLuint tex;
  GLuint program;       // shader program
  GLuint vao, vbo, ebo; // vertex array object
  int tex_valid;
  int program_valid;
  int tex_update;
  int data1_loc;

  cairo_surface_t *surface;
  cairo_t *context;
  unsigned char *pixeldata;
  int width;
  int height;
  int stride;
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

G_DEFINE_TYPE_WITH_CODE (HyScanGtkGlikoDrawing, hyscan_gtk_gliko_drawing, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_GLIKO_LAYER, layer_interface_init))

static const char *vertexShaderSource =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "out vec2 TexCoord;\n"
    "void main()\n"
    "{\n"
    "  gl_Position = vec4(aPos, 1.0);\n"
    "  TexCoord = vec2(aTexCoord.x, aTexCoord.y);\n"
    "}\n";

static const char *fragmentShaderSource =
    "#version 330 core\n"
    "in vec2 TexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D data1;\n"
    "void main()\n"
    "{\n"
    "   vec4 c = texture(data1,TexCoord);\n"
    "   if (c.a < (1./256.)) discard;\n"
    "   c.rgb = c.rgb / c.a;\n"
    "   FragColor = c;\n"
    "}\n";

static int
create_shader_program (GLuint *prog)
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
      return 0;
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
      return 0;
    }
  // link shaders
  GLuint shaderProgram = glCreateProgram ();
  glAttachShader (shaderProgram, vertexShader);
  glAttachShader (shaderProgram, fragmentShader);
  glLinkProgram (shaderProgram);
  // check for linking errors
  glGetProgramiv (shaderProgram, GL_LINK_STATUS, &success);
  if (!success)
    {
      glGetProgramInfoLog (shaderProgram, sizeof (infoLog), NULL, infoLog);
      fprintf (stderr, "Link program error: %s\n", infoLog);
      return 0;
    }
  glDeleteShader (vertexShader);
  glDeleteShader (fragmentShader);
  *prog = shaderProgram;
  return 1;
}

// VAO - vertex array object
static void
create_model (HyScanGtkGlikoDrawingPrivate *p)
{
  // set up vertex data (and buffer(s)) and configure vertex attributes
  // ------------------------------------------------------------------
  static const float vertices[] =
      {
        // positions          // texture coords
        1.0f, -1.0f, 0.0f, 1.0f, 1.0f,   // top right
        1.0f, 1.0f, 0.0f, 1.0f, 0.0f,  // bottom right
        -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, // bottom left
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f   // top left
      };
  static const unsigned int indices[] =
      {
        // note that we start from 0!
        0, 1, 3, // first Triangle
        1, 2, 3  // second Triangle
      };

  glGenVertexArrays (1, &p->vao);
  glGenBuffers (1, &p->vbo);
  glGenBuffers (1, &p->ebo);
  // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
  glBindVertexArray (p->vao);

  glBindBuffer (GL_ARRAY_BUFFER, p->vbo);
  glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_DYNAMIC_DRAW);

  glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, p->ebo);
  glBufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices, GL_STATIC_DRAW);

  glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof (float), (void *) 0);
  glEnableVertexAttribArray (0);

  glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof (float), (void *) (3 * sizeof (float)));
  glEnableVertexAttribArray (1);

  // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
  glBindBuffer (GL_ARRAY_BUFFER, 0);

  // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
  // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
  glBindVertexArray (0);
}

static void draw_example( cairo_t *cr )
{
#if defined( DRAW_EXAMPLE )
  double xc = 128.0;
  double yc = 128.0;
  double radius = 100.0;
  double angle1 = 45.0  * (M_PI/180.0);  /* angles are specified */
  double angle2 = 180.0 * (M_PI/180.0);  /* in radians           */

  cairo_set_line_width (cr, 10.0);
  cairo_arc (cr, xc, yc, radius, angle1, angle2);
  cairo_stroke (cr);

  /* draw helping lines */
  cairo_set_source_rgba (cr, 1, 0.2, 0.2, 0.6);
  cairo_set_line_width (cr, 6.0);

  cairo_arc (cr, xc, yc, 10.0, 0, 2*M_PI);
  cairo_fill (cr);

  cairo_arc (cr, xc, yc, radius, angle1, angle1);
  cairo_line_to (cr, xc, yc);
  cairo_arc (cr, xc, yc, radius, angle2, angle2);
  cairo_line_to (cr, xc, yc);
  cairo_stroke (cr);
#endif
}

static void
layer_resize (HyScanGtkGlikoLayer *layer, gint width, gint height)
{
  HyScanGtkGlikoDrawingPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (layer, HYSCAN_TYPE_GTK_GLIKO_DRAWING, HyScanGtkGlikoDrawingPrivate);

  if( width == p->width && height == p->height )
    return;

  p->width = width;
  p->height = height;

  if( p->pixeldata != NULL )
  {
    free( p->pixeldata );
    p->pixeldata = NULL;
  }
  p->stride = cairo_format_stride_for_width( CAIRO_FORMAT_ARGB32, p->width );
  p->pixeldata = malloc( p->stride * p->height );
  memset( p->pixeldata, 0, p->stride * p->height );

  if( p->context != NULL )
  {
    cairo_destroy( p->context );
    p->context = NULL;
  }
  if( p->surface != NULL )
  {
    cairo_surface_destroy( p->surface );
    p->surface = NULL;
  }
  p->surface = cairo_image_surface_create_for_data( p->pixeldata, CAIRO_FORMAT_ARGB32, p->width, p->height, p->stride );
  p->context = cairo_create( p->surface );

  draw_example( p->context );
  cairo_surface_flush( p->surface );

  if( p->tex_valid )
  {
    glDeleteTextures( 1, &p->tex );
    p->tex_valid = 0;
  }
  glGenTextures( 1, &p->tex );
  p->tex_valid = 1;
  glBindTexture( GL_TEXTURE_2D, p->tex );
  glPixelStorei (GL_UNPACK_ROW_LENGTH, p->stride >> 2);
  glTexImage2D( GL_TEXTURE_2D,
                0,
                GL_RGBA8,
                p->width,
                p->height,
                0,
                GL_BGRA,
                GL_UNSIGNED_BYTE,
                p->pixeldata );
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  p->tex_update = 0;
}

static void
layer_render (HyScanGtkGlikoLayer *layer, GdkGLContext *context)
{
  HyScanGtkGlikoDrawingPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (layer, HYSCAN_TYPE_GTK_GLIKO_DRAWING, HyScanGtkGlikoDrawingPrivate);

  if (p->program_valid == 0)
    return;

  if( p->tex_update )
  {
    glBindTexture( GL_TEXTURE_2D, p->tex );
    glPixelStorei (GL_UNPACK_ROW_LENGTH, p->stride >> 2);
    glTexSubImage2D( GL_TEXTURE_2D,
                     0,
                     0,
                     0,
                     p->width,
                     p->height,
                     GL_BGRA,
                     GL_UNSIGNED_BYTE,
                     p->pixeldata );
    p->tex_update = 0;
  }

  glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
  glUseProgram (p->program);
  glUniform1i (p->data1_loc, 0);

  glActiveTexture (GL_TEXTURE0 + 0);
  glBindTexture( GL_TEXTURE_2D, p->tex );

  glBindVertexArray (p->vao); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
  glBindBuffer (GL_ARRAY_BUFFER, p->vbo);
  glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, p->ebo);

  glDrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

static void
layer_realize (HyScanGtkGlikoLayer *layer)
{
  HyScanGtkGlikoDrawingPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (layer, HYSCAN_TYPE_GTK_GLIKO_DRAWING, HyScanGtkGlikoDrawingPrivate);

  // создаем программу шейдера
  p->program_valid = create_shader_program (&p->program);
  p->data1_loc = glGetUniformLocation (p->program, "data1");

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
hyscan_gtk_gliko_drawing_init (HyScanGtkGlikoDrawing *drawing)
{
  HyScanGtkGlikoDrawingPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (drawing, HYSCAN_TYPE_GTK_GLIKO_DRAWING, HyScanGtkGlikoDrawingPrivate);

  /* Create cache for faster access */
  drawing->priv = p;

  p->program = 0;
  p->program_valid = 0;

  p->tex = 0;
  p->tex_valid = 0;
  p->tex_update = 0;

  p->pixeldata = NULL;
  p->width = 0;
  p->height = 0;
  p->stride = 0;
  p->surface = NULL;
  p->context = NULL;
}

static void
hyscan_gtk_gliko_drawing_class_init (HyScanGtkGlikoDrawingClass *klass)
{
  //GObjectClass *g_class = G_OBJECT_CLASS (klass);
  //static const int rw = (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_type_class_add_private (klass, sizeof (HyScanGtkGlikoDrawingPrivate));
}

HYSCAN_API
HyScanGtkGlikoDrawing *
hyscan_gtk_gliko_drawing_new (void)
{
  return HYSCAN_GTK_GLIKO_DRAWING (g_object_new (hyscan_gtk_gliko_drawing_get_type (), NULL));
}

HYSCAN_API
cairo_t *hyscan_gtk_gliko_drawing_begin (HyScanGtkGlikoDrawing *instance)
{
  HyScanGtkGlikoDrawingPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO_DRAWING, HyScanGtkGlikoDrawingPrivate);

  memset( p->pixeldata, 0, p->stride * p->height );
  return p->context;
}

HYSCAN_API
void hyscan_gtk_gliko_drawing_end (HyScanGtkGlikoDrawing *instance)
{
  HyScanGtkGlikoDrawingPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO_DRAWING, HyScanGtkGlikoDrawingPrivate);

  cairo_surface_flush( p->surface );
  p->tex_update = 1;
}
