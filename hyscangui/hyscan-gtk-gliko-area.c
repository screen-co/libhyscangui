/* hyscan-gtk-gliko-area.с
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

#if defined (_MSC_VER)
#include <glad/glad.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hyscan-gtk-gliko-area.h"

#define SHADER_RESOURCE_PATH "/org/hyscan/gl/hyscan-gtk-gliko-area-shader.c"

/* Properties enum */
enum
{
  P_RESERVED,
  P_CX,        // координата X центра
  P_CY,        // координата Y центра
  P_SCALE,     // масштаб
  P_BRIGHT,    // яркость
  P_CONTRAST,  // контраст
  P_BLACK,     // уровень черного 0..1
  P_WHITE,     // уровень белого 0..1
  P_GAMMA,     // степень нелинейности 1=линейно
  P_ROTATE,    // поворот оси, 0..360
  P_FADE_COEF, // коэффициент затухания 0..1
  P_REMAIN,    // коэффициент при перезаписи
  P_BOTTOM,     // глубина (номер отсчета дна)
  P_COLOR1,
  P_COLOR2,
  P_BACKGROUND,
  P_COLOR1_ALPHA,
  P_COLOR2_ALPHA,
  P_BACKGROUND_ALPHA,
  N_PROPERTIES
};

#define TEX_MAX 16

#define TEX_SIZE_BITS 8
#define TEX_SIZE (1 << TEX_SIZE_BITS)

typedef float sample_t;

struct _HyScanGtkGlikoAreaPrivate
{
  gboolean panning;
  int w, h;
  unsigned int program;       // shader program
  unsigned int vao, vbo, ebo; // vertex array object

  int init_stage;
  int n_tex;
  int nd_bits;
  int na_bits;
  int nd;
  int na;
  int bottom;

  GLuint tex[2][TEX_MAX], tex_fade[2], tex_beam[2];
  float center_x;
  float center_y;
  float scale;
  float bright;
  float contrast;
  float rotate;
  float rotate1;
  float rotate2;
  float fade_coef;
  float remain;
  int tna;
  int tnd[TEX_MAX];
  int beam_pos[2];
  int beam_valid[2];
  float color[2][4];
  float background[4];
  sample_t *buf[2][TEX_MAX];
  sample_t *fade[2];
  sample_t *beam[2];

  float white;
  float black;
  float gamma;

  int data1_loc, data2_loc, beam1_loc, beam2_loc, fade1_loc, fade2_loc;
};

static void hyscan_gtk_gliko_area_interface_init (HyScanGtkGlikoLayerInterface *iface);
static void hyscan_gtk_gliko_area_set_property (GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec);
static void hyscan_gtk_gliko_area_get_property (GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec);
static void hyscan_gtk_gliko_area_object_constructed (GObject *object);
static void hyscan_gtk_gliko_area_object_finalize (GObject *object);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkGlikoArea, hyscan_gtk_gliko_area, G_TYPE_OBJECT, G_ADD_PRIVATE (HyScanGtkGlikoArea) G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_GLIKO_LAYER, hyscan_gtk_gliko_area_interface_init))

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL };

static const char *vertexShaderSource =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "\n"
    "out vec2 TexCoord;\n"
    "\n"
    "void main()\n"
    "{\n"
    "  gl_Position = vec4(aPos, 1.0);\n"
    "  TexCoord = vec2(aTexCoord.x, aTexCoord.y);\n"
    "}\n";

static int
create_shader_program ()
{
  GBytes * shader_bytes;
  const gchar * shader_source;
  int vertexShader;
  int fragmentShader;
  int shaderProgram;
  int success;
  char infoLog[512];

  // build and compile our shader program
  // ------------------------------------
  // vertex shader
  vertexShader = glCreateShader (GL_VERTEX_SHADER);
  glShaderSource (vertexShader, 1, &vertexShaderSource, NULL);
  glCompileShader (vertexShader);
  // check for shader compile errors
  glGetShaderiv (vertexShader, GL_COMPILE_STATUS, &success);
  if (!success)
    {
      glGetShaderInfoLog (vertexShader, sizeof (infoLog), NULL, infoLog);
      fprintf (stderr, "Vertex shader compile error: %s\n", infoLog);
      return -1;
    }
  // fragment shader
  shader_bytes = g_resources_lookup_data (SHADER_RESOURCE_PATH,
                                          G_RESOURCE_LOOKUP_FLAGS_NONE,
                                          NULL);
  shader_source = (gchar*) g_bytes_get_data (shader_bytes, NULL);

  fragmentShader = glCreateShader (GL_FRAGMENT_SHADER);
  glShaderSource (fragmentShader, 1, &shader_source, NULL);
  glCompileShader (fragmentShader);

  g_bytes_unref (shader_bytes);

  // check for shader compile errors
  glGetShaderiv (fragmentShader, GL_COMPILE_STATUS, &success);
  if (!success)
    {
      glGetShaderInfoLog (fragmentShader, sizeof (infoLog), NULL, infoLog);
      fprintf (stderr, "Fragment compile error: %s\n", infoLog);
      return -1;
    }
  // link shaders
  shaderProgram = glCreateProgram ();
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
create_model (HyScanGtkGlikoAreaPrivate *p)
{
  // set up vertex data (and buffer(s)) and configure vertex attributes
  // ------------------------------------------------------------------
  static const float vertices[] =
      {
        // positions          // texture coords
        0.5f, 0.5f, 0.0f, 1.0f, 1.0f,   // top right
        0.5f, -0.5f, 0.0f, 1.0f, 0.0f,  // bottom right
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, // bottom left
        -0.5f, 0.5f, 0.0f, 0.0f, 1.0f   // top left
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

/*
static void glerr()
{
  GLenum e;

  if( (e = glGetError()) != GL_NO_ERROR )
  {
    fprintf( stderr, "GL error 0x%X\n", (int)e ); fflush( stderr );
  }
}
*/
#define glerr()                                                                   \
  {                                                                               \
    GLenum e;                                                                     \
    if ((e = glGetError ()) != GL_NO_ERROR)                                       \
      {                                                                           \
        fprintf (stderr, "%s(%d): GL error 0x%X\n", __FILE__, __LINE__, (int) e); \
        fflush (stderr);                                                          \
      }                                                                           \
  }

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
set_uniform1i (const unsigned int prog, const char *name, const int val)
{
  int loc = glGetUniformLocation (prog, name);
  if (loc == -1)
    {
      glerr ();
      return;
    }
  glUniform1i (loc, val);
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

static void
resample2 (sample_t *dst, const sample_t *src, const int ndst)
{
  int i;
  sample_t *d = dst;
  const sample_t *s = src;

  for (i = ndst; i != 0; i--)
    {
      d[0] = 0.5f * (s[0] + s[1]);
      //d[0] = s[0] > s[1] ? s[0]: s[1];
      d++;
      s += 2;
    }
}

#if 0
static void pattern( HyScanGtkGlikoAreaPrivate *p )
{
  int i, j, k;
  sample_t *b;
  int NA = (1 << p->na_bits);
  int ND = (1 << p->nd_bits);

  b = p->buf[0];
  for( i = 0; i < 360; i += 10 )
  {
    k = (i * NA / 360) * ND;
    for( j = 1; j < ND - 1; j++ )
      b[ k + j ] = 0x40;
  }
  for( i = 0; i < 360; i += 30 )
  {
    k = (i * NA / 360) * ND;
    for( j = 1; j < ND - 1; j++ )
      b[ k + j ] = 0x80;
  }
  for( i = 0; i < 360; i += 90 )
  {
    k = (((i * NA / 360) + 2) % NA) * ND;
    for( j = 1; j < ND - 1; j++ )
      b[ k + j ] = 0x80;
  }

  for( i = 0; i < NA; i++ )
  {
    k = i * ND;
    for( j = 10; j < 200; j += 10 )
      b[ k + (j * ND / 200) ] = 0x80;
    for( j = 50; j < 200; j += 50 )
      b[ k + (j * ND / 200) ] = 0x80;
    b[ k + ( 50 * ND / 200) - 2 ] = 0x80;
    b[ k + (100 * ND / 200) - 2 ] = 0x80;
    b[ k + (100 * ND / 200) - 4 ] = 0x80;
    b[ k + (150 * ND / 200) - 2 ] = 0x80;
    b[ k + (150 * ND / 200) - 4 ] = 0x80;
    b[ k + (150 * ND / 200) - 6 ] = 0x80;
    b[ k + ND - 2 ] = 0x80;
  }
  for( i = 0; i < NA; i++ )
  {
    b[ i * ND + (i % ND) ] = 0xC0;
  }

  for( i = 1, j = (1 << p->nd_bits); i < p->n_tex; i++ )
  {
    j >>= 1;
    for( k = 0; k < NA; k++ )
    {
      resample2( p->buf[ i ] + k * j, p->buf[ i - 1 ] + k * (j << 1), j );
    }
  }
}
#endif

static void
init_textures (HyScanGtkGlikoAreaPrivate *p)
{
  int i, j, k;
  int ia, id;

  if (p->init_stage != 1)
    return;
  p->init_stage = 2;

  glGenTextures (p->n_tex, p->tex[0]);
  glGenTextures (p->n_tex, p->tex[1]);
  glGenTextures (2, p->tex_fade);
  glGenTextures (2, p->tex_beam);

  p->tna = (1 << (p->na_bits - TEX_SIZE_BITS));

  for (k = 0; k < 2; k++)
    {
      for (i = 0, j = (1 << p->nd_bits); i < p->n_tex; i++, j >>= 1)
        {
          p->tnd[i] = j / TEX_SIZE;

          glBindTexture (GL_TEXTURE_2D_ARRAY, p->tex[k][i]);
          //glTexStorage3D (GL_TEXTURE_2D_ARRAY, 1, GL_R32F, TEX_SIZE, TEX_SIZE, p->tna * p->tnd[i]);
          glPixelStorei (GL_UNPACK_ROW_LENGTH, j * sizeof (sample_t));
          glTexImage3D (GL_TEXTURE_2D_ARRAY, 0, GL_R32F, TEX_SIZE, TEX_SIZE, p->tna * p->tnd[i], 0, GL_RED, GL_FLOAT, NULL);
          glTexParameteri (GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
          glTexParameteri (GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
          glTexParameteri (GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
          glTexParameteri (GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
          //glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP);
          //glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
          //glPixelStorei (GL_UNPACK_ROW_LENGTH, j * sizeof (sample_t));
          for (ia = 0; ia < p->tna; ia++)
            {
              for (id = 0; id < p->tnd[i]; id++)
                {
                  glTexSubImage3D (GL_TEXTURE_2D_ARRAY,
                                   0,                         // mipmap number
                                   0, 0, ia * p->tnd[i] + id, // xoffset, yoffset, zoffset,
                                   TEX_SIZE, TEX_SIZE, 1,     // width, height, depth
                                   GL_RED,                    // format
                                   GL_FLOAT,                  // type
                                   p->buf[k][i] + ia * TEX_SIZE * j + id * TEX_SIZE);
                }
            }
        }
      glBindTexture (GL_TEXTURE_1D_ARRAY, p->tex_beam[k]);
      //glTexStorage2D (GL_TEXTURE_1D_ARRAY, 1, GL_R32F, TEX_SIZE, p->tna);
      glPixelStorei (GL_UNPACK_ROW_LENGTH, (1 << p->nd_bits) * sizeof (sample_t));
      glTexImage2D(GL_TEXTURE_1D_ARRAY, 0, GL_R32F, TEX_SIZE, p->tna, 0, GL_RED, GL_FLOAT, NULL);
      glTexParameteri (GL_TEXTURE_1D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
      glTexParameteri (GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
      glTexParameteri (GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      for (ia = 0; ia < p->tna; ia++)
        {
          glTexSubImage2D (GL_TEXTURE_1D_ARRAY, 0, 0, ia, TEX_SIZE, 1, GL_RED, GL_FLOAT, p->beam[k] + ia * TEX_SIZE);
        }
      glBindTexture (GL_TEXTURE_1D_ARRAY, p->tex_fade[k]);
      glPixelStorei (GL_UNPACK_ROW_LENGTH, (1 << p->nd_bits) * sizeof (sample_t));
      //glTexStorage2D (GL_TEXTURE_1D_ARRAY, 1, GL_R32F, TEX_SIZE, p->tna);
      glTexImage2D(GL_TEXTURE_1D_ARRAY, 0, GL_R32F, TEX_SIZE, p->tna, 0, GL_RED, GL_FLOAT, NULL);
      glTexParameteri (GL_TEXTURE_1D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
      glTexParameteri (GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
      glTexParameteri (GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      for (ia = 0; ia < p->tna; ia++)
        {
          glTexSubImage2D (GL_TEXTURE_1D_ARRAY, 0, 0, ia, TEX_SIZE, 1, GL_RED, GL_FLOAT, p->fade[k] + ia * TEX_SIZE);
        }
    }
  glerr ();
}

static void
layer_resize (HyScanGtkGlikoLayer *layer, gint width, gint height)
{
  HyScanGtkGlikoAreaPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (layer, HYSCAN_TYPE_GTK_GLIKO_AREA, HyScanGtkGlikoAreaPrivate);

  p->w = width;
  p->h = height;
}

static void
layer_render (HyScanGtkGlikoLayer *layer, GdkGLContext *context)
{
  HyScanGtkGlikoAreaPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (layer, HYSCAN_TYPE_GTK_GLIKO_AREA, HyScanGtkGlikoAreaPrivate);
  int i;
  float x0 = 0.0f, x1 = p->w, y0 = 0.0f, y1 = p->h;
  float real_scale;
  float sh = y1 - y0;
  float sw = x1 - x0;
  //float sx = x0;
  //float sy = y0;
  float tx0, ty0, tx1, ty1;
  float d;
  float contrast;
  float real_distance;
  float real_bottom;

  switch (p->init_stage)
    {
    case 0:
      return;
    case 1:
      init_textures (p);
      break;
    default:
      break;
    }

  //glClearColor( p->background[0], p->background[1], p->background[2], p->background[3] );
  //glClear(GL_COLOR_BUFFER_BIT);

  // uncomment this call to draw in wireframe polygons.
  //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  // render
  // ------
  if (p->program == 0)
    return;

  glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
  glUseProgram (p->program);

  glUniform1i (p->data1_loc, 0);
  glUniform1i (p->data2_loc, 1);
  glUniform1i (p->beam1_loc, 2);
  glUniform1i (p->beam2_loc, 3);
  glUniform1i (p->fade1_loc, 4);
  glUniform1i (p->fade2_loc, 5);

  real_scale = p->scale * p->nd * 2.0f / (y1 - y0);
  real_distance = p->nd;
  real_distance /= (1 << p->nd_bits);
  real_bottom = p->bottom;
  real_bottom /= (1 << p->nd_bits);
  //set_uniform1f( iko_prog, "noise", 0.f );//real_scale > 1.0f ? (0.375f / ND): 0.f );

  for (i = 0; (i + 1) < p->n_tex && real_scale > 1.0f; i++, real_scale *= 0.5f )
    ;

  glActiveTexture (GL_TEXTURE0 + 0);
  glBindTexture (GL_TEXTURE_2D_ARRAY, p->tex[0][i]);

  glActiveTexture (GL_TEXTURE0 + 1);
  glBindTexture (GL_TEXTURE_2D_ARRAY, p->tex[1][i]);

  glActiveTexture (GL_TEXTURE0 + 2);
  glBindTexture (GL_TEXTURE_1D_ARRAY, p->tex_beam[0]);

  glActiveTexture (GL_TEXTURE0 + 3);
  glBindTexture (GL_TEXTURE_1D_ARRAY, p->tex_beam[1]);

  glActiveTexture (GL_TEXTURE0 + 4);
  glBindTexture (GL_TEXTURE_1D_ARRAY, p->tex_fade[0]);

  glActiveTexture (GL_TEXTURE0 + 5);
  glBindTexture (GL_TEXTURE_1D_ARRAY, p->tex_fade[1]);

  set_uniform1f (p->program, "distance", real_distance);
  set_uniform1f (p->program, "bottom", real_bottom);

  set_uniform1i (p->program, "tna", p->tna);
  set_uniform1i (p->program, "tnd", p->tnd[i]);

  contrast = p->contrast;
  if (contrast > 1.0f)
    {
      contrast = 1.0f;
    }
  else if (contrast < -1.0f)
    {
      contrast = -1.0f;
    }
  if (contrast > 0.0f)
    {
      contrast = 1.f / (1.0f - 0.9999f * contrast);
    }
  else
    {
      contrast = 1.0f + contrast;
    }
  set_uniform1f (p->program, "contrast", contrast);
  set_uniform1f (p->program, "bright", p->bright);
  set_uniform1f (p->program, "ampoffset", p->black);
  set_uniform1f (p->program, "amprange", 1.0f / (p->white - p->black));
  set_uniform1f (p->program, "gamma", p->gamma);

  set_uniform1f (p->program, "rotate", p->rotate * 3.1415926536f / 180.f);

  set_uniform4fv (p->program, "colorr", p->color[0]);
  set_uniform4fv (p->program, "colorg", p->color[1]);
  set_uniform4fv (p->program, "background", p->background);
  d = (float) p->nd / (float) (1 << p->nd_bits);

  tx0 = 0.5f * (1.0f - d * sw * p->scale / sh) - d * p->center_x;
  tx1 = 0.5f * (1.0f + d * sw * p->scale / sh) - d * p->center_x;
  ty0 = 0.5f * (1.0f - d * p->scale) - d * p->center_y;
  ty1 = 0.5f * (1.0f + d * p->scale) - d * p->center_y;

  sw = 1.0;
  sh = 1.0;

  float vertices[] =
      {
        // positions            // texture coords
        sw, sh, 0.0f, tx1, ty1,   // top right
        sw, -sh, 0.0f, tx1, ty0,  // bottom right
        -sw, -sh, 0.0f, tx0, ty0, // bottom left
        -sw, sh, 0.0f, tx0, ty1   // top left
      };

  glBindVertexArray (p->vao); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
  glBindBuffer (GL_ARRAY_BUFFER, p->vbo);
  glBufferSubData (GL_ARRAY_BUFFER, 0, sizeof (vertices), vertices);

  glDrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  return;
}

static void
layer_realize (HyScanGtkGlikoLayer *layer)
{
  HyScanGtkGlikoAreaPrivate *p;

  p = G_TYPE_INSTANCE_GET_PRIVATE (layer, HYSCAN_TYPE_GTK_GLIKO_AREA, HyScanGtkGlikoAreaPrivate);

  init_textures (p);

  // Init programs:
  p->program = create_shader_program ();

  p->data1_loc = glGetUniformLocation (p->program, "data1");
  p->data2_loc = glGetUniformLocation (p->program, "data2");
  p->beam1_loc = glGetUniformLocation (p->program, "beam1");
  p->beam2_loc = glGetUniformLocation (p->program, "beam2");
  p->fade1_loc = glGetUniformLocation (p->program, "fade1");
  p->fade2_loc = glGetUniformLocation (p->program, "fade2");

  create_model (p);

  // Init background:
  //background_init();

  // Init model:
  //model_init();
}

static void
fill_buffer (sample_t *buffer, const sample_t value, const int length)
{
  int i;

  for (i = 0; i < length; i++)
    buffer[i] = value;
}

void
hyscan_gtk_gliko_area_init_dimension (HyScanGtkGlikoArea *instance, const int na, const int nd)
{
  HyScanGtkGlikoAreaPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO_AREA, HyScanGtkGlikoAreaPrivate);
  int i, j, k;

  p->init_stage = 1;

  if (p->n_tex != 0)
    {
      glDeleteTextures (p->n_tex, p->tex[0]);
      glDeleteTextures (p->n_tex, p->tex[1]);
    }

  if (p->buf[0][0] != NULL)
    {
      free (p->buf[0][0]);
      p->buf[0][0] = NULL;
    }
  p->na = na;
  p->nd = nd;

  for (p->nd_bits = 1; (1 << p->nd_bits) < nd; p->nd_bits++)
    ;
  for (p->na_bits = 1; (1 << p->na_bits) < na; p->na_bits++)
    ;
  for (p->n_tex = 0, j = (1 << p->nd_bits); p->n_tex < TEX_MAX && j > TEX_SIZE; p->n_tex++, j >>= 1)
    ;

  k = (1 << p->nd_bits) * (1 << p->na_bits) * 2 + (1 << p->na_bits) + (1 << p->na_bits);
  if ((p->buf[0][0] = malloc (k * 2 * sizeof (sample_t))) == NULL)
    return;

  p->buf[1][0] = p->buf[0][0] + k;

  for (i = 1, j = (1 << p->nd_bits); i < p->n_tex; i++, j >>= 1)
    {
      p->buf[0][i] = p->buf[0][i - 1] + (1 << p->na_bits) * j;
      p->buf[1][i] = p->buf[1][i - 1] + (1 << p->na_bits) * j;
    }

  p->fade[0] = p->buf[0][0] + k - (1 << p->na_bits) - (1 << p->na_bits);
  p->fade[1] = p->buf[1][0] + k - (1 << p->na_bits) - (1 << p->na_bits);
  p->beam[0] = p->buf[0][0] + k - (1 << p->na_bits);
  p->beam[1] = p->buf[1][0] + k - (1 << p->na_bits);

  fill_buffer (p->buf[0][0], 0.0f, k * 2);
  //pattern( p );

  for (i = 0; i < (1 << p->na_bits); i++)
    {
      p->fade[0][i] = 0.0f;
      p->fade[1][i] = 0.0f;
      p->beam[0][i] = 0;
      p->beam[1][i] = 0;
    }
}

void
hyscan_gtk_gliko_area_set_data (HyScanGtkGlikoArea *instance, const int channel, const int azimuth, const gfloat *data)
{
  HyScanGtkGlikoAreaPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO_AREA, HyScanGtkGlikoAreaPrivate);
  sample_t *dst, *dst_row;
  const sample_t *src;
  sample_t k, c;
  int i, j;
  int ia, id;
  int a;

  if (p->init_stage != 2)
    return;

  if (azimuth < 0 || azimuth >= p->na)
    return;

  // по номеру канала
  switch (channel)
    {
    case 0:
      // поворот относительно оси
      a = azimuth + (int) (p->rotate1 * p->na / 360.f);
      break;
    case 1:
      // поворот относительно оси
      a = azimuth + (int) (p->rotate2 * p->na / 360.f);
      break;
    default:
      return;
    }
  if (a >= p->na)
    {
      a -= p->na;
    }

  j = (1 << p->nd_bits);
  dst_row = p->buf[channel][0] + a * j;
  k = p->remain * p->fade[channel][a];
  p->fade[channel][a] = 1.0f;

  for (dst = dst_row, src = data, i = p->nd; i != 0; i--, dst++, src++)
    {
      c = (k * (*dst)) + (*src);
      if (c > 1.0f)
        c = 1.0f;
      *dst = c;
    }

  glBindTexture (GL_TEXTURE_2D_ARRAY, p->tex[channel][0]);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, j * sizeof (sample_t));
  for (id = 0; id < p->tnd[0]; id++)
    {
      glTexSubImage3D (GL_TEXTURE_2D_ARRAY,
                       0,                                                // mipmap number
                       0, a % TEX_SIZE, (a / TEX_SIZE) * p->tnd[0] + id, // xoffset, yoffset, zoffset,
                       TEX_SIZE, 1, 1,                                   // width, height, depth
                       GL_RED,                                           // format
                       GL_FLOAT,                                         // type
                       dst_row + id * TEX_SIZE);
    }
  //glTexSubImage2D( GL_TEXTURE_2D, 0, 0, a, j, 1, comp, GL_UNSIGNED_BYTE, dst_row );

  for (i = 1, j = (1 << p->nd_bits); i < p->n_tex; i++)
    {
      j >>= 1;
      resample2 (p->buf[channel][i] + a * j, p->buf[channel][i - 1] + a * (j << 1), j);
      glBindTexture (GL_TEXTURE_2D_ARRAY, p->tex[channel][i]);
      glPixelStorei (GL_UNPACK_ROW_LENGTH, j * sizeof (sample_t));
      for (id = 0; id < p->tnd[i]; id++)
        {
          glTexSubImage3D (GL_TEXTURE_2D_ARRAY,
                           0,                                                // mipmap number
                           0, a % TEX_SIZE, (a / TEX_SIZE) * p->tnd[i] + id, // xoffset, yoffset, zoffset,
                           TEX_SIZE, 1, 1,                                   // width, height, depth
                           GL_RED,                                           // format
                           GL_FLOAT,                                         // type
                           p->buf[channel][i] + a * j + id * TEX_SIZE);
          //glTexSubImage2D( GL_TEXTURE_2D, 0, 0, a, j, 1, comp, GL_UNSIGNED_BYTE, p->buf[ i ] + a * j );
        }
    }

  // формируем модуляцию яркости для лучей двух каналов
  fill_buffer (p->beam[channel], 0.0f, p->na);

  p->beam[channel][a] = 1.0f;

  for (i = 1; i < 4; i++)
    {
      p->beam[channel][(a + i) % p->na] =
          p->beam[channel][(a + p->na - i) % p->na] = 1.0f / (1.0f + i);
    }
  glPixelStorei (GL_UNPACK_ROW_LENGTH, (1 << p->na_bits) * sizeof (sample_t));

  glBindTexture (GL_TEXTURE_1D_ARRAY, p->tex_beam[channel]);
  for (ia = 0; ia < p->tna; ia++)
    {
      glTexSubImage2D (GL_TEXTURE_1D_ARRAY, 0, 0, ia, TEX_SIZE, 1, GL_RED, GL_FLOAT, p->beam[channel] + ia * TEX_SIZE);
    }
  //glTexSubImage2D( GL_TEXTURE_1D_ARRAY, 0, 0, 0, 1 << p->na_bits, 1, GL_RED, GL_UNSIGNED_BYTE, p->beam );
  //glTexSubImage1D( GL_TEXTURE_1D, 0, 0, 1 << p->na_bits, GL_RED, GL_UNSIGNED_BYTE, p->beam );

  glBindTexture (GL_TEXTURE_1D_ARRAY, p->tex_fade[channel]);
  for (ia = 0; ia < p->tna; ia++)
    {
      glTexSubImage2D (GL_TEXTURE_1D_ARRAY, 0, 0, ia, TEX_SIZE, 1, GL_RED, GL_FLOAT, p->fade[channel] + ia * TEX_SIZE);
    }
  glerr ();
}

void
hyscan_gtk_gliko_area_fade (HyScanGtkGlikoArea *instance)
{
  HyScanGtkGlikoAreaPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO_AREA, HyScanGtkGlikoAreaPrivate);
  int i, ia;

  if (p->init_stage != 2)
    return;

  for (i = 0; i < p->na; i++)
    {
      p->fade[0][i] = (p->fade_coef * p->fade[0][i]);
      p->fade[1][i] = (p->fade_coef * p->fade[1][i]);
    }
  glPixelStorei (GL_UNPACK_ROW_LENGTH, (1 << p->na_bits) * sizeof (sample_t));
  glBindTexture (GL_TEXTURE_1D_ARRAY, p->tex_fade[0]);
  for (ia = 0; ia < p->tna; ia++)
    {
      glTexSubImage2D (GL_TEXTURE_1D_ARRAY, 0, 0, ia, TEX_SIZE, 1, GL_RED, GL_FLOAT, p->fade[0] + ia * TEX_SIZE);
    }
  glBindTexture (GL_TEXTURE_1D_ARRAY, p->tex_fade[1]);
  for (ia = 0; ia < p->tna; ia++)
    {
      glTexSubImage2D (GL_TEXTURE_1D_ARRAY, 0, 0, ia, TEX_SIZE, 1, GL_RED, GL_FLOAT, p->fade[1] + ia * TEX_SIZE);
    }
  glerr ();
}

/* Initialization */
static void
hyscan_gtk_gliko_area_class_init (HyScanGtkGlikoAreaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  static const int rw = (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  object_class->set_property = hyscan_gtk_gliko_area_set_property;
  object_class->get_property = hyscan_gtk_gliko_area_get_property;
  object_class->constructed = hyscan_gtk_gliko_area_object_constructed;
  object_class->finalize = hyscan_gtk_gliko_area_object_finalize;

  obj_properties[P_CX] = g_param_spec_float ("gliko-cx", "Center X", "Center point coordinate X", -G_MAXFLOAT, G_MAXFLOAT, 0.0, rw);
  obj_properties[P_CY] = g_param_spec_float ("gliko-cy", "Center Y", "Center point coordinate Y", -G_MAXFLOAT, G_MAXFLOAT, 0.0, rw);
  obj_properties[P_SCALE] = g_param_spec_float ("gliko-scale", "Scale", "Scale of image", -G_MAXFLOAT, G_MAXFLOAT, 0.0, rw);
  obj_properties[P_BRIGHT] = g_param_spec_float ("gliko-bright", "Bright", "Bright of image", -G_MAXFLOAT, G_MAXFLOAT, 0.0, rw);
  obj_properties[P_CONTRAST] = g_param_spec_float ("gliko-contrast", "Contrast", "Contrast of image", -G_MAXFLOAT, G_MAXFLOAT, 0.0, rw);
  obj_properties[P_BLACK] = g_param_spec_float ("gliko-black", "Black", "Black level", -G_MAXFLOAT, G_MAXFLOAT, 0.0, rw);
  obj_properties[P_WHITE] = g_param_spec_float ("gliko-white", "White", "White level", -G_MAXFLOAT, G_MAXFLOAT, 1.0, rw);
  obj_properties[P_GAMMA] = g_param_spec_float ("gliko-gamma", "Gamma", "Gamma for non-linear", -G_MAXFLOAT, G_MAXFLOAT, 1.0, rw);
  obj_properties[P_ROTATE] = g_param_spec_float ("gliko-rotate", "Rotate", "Angle of rotation in degrees", -G_MAXFLOAT, G_MAXFLOAT, 0.0, rw);
  obj_properties[P_FADE_COEF] = g_param_spec_float ("gliko-fade-coef", "FadeCoef", "Fade coefficient", 0.0, 1.0, 0.98, rw);
  obj_properties[P_REMAIN] = g_param_spec_float ("gliko-remain", "Remain", "Remain coefficient", 0.0, 1.0, 1.0, rw);
  obj_properties[P_BOTTOM] = g_param_spec_int ("gliko-bottom", "Bottom", "Depth (bottom's offset)", 0, G_MAXINT, 0, rw);
  obj_properties[P_COLOR1_ALPHA] = g_param_spec_float ("gliko-color1-alpha", "ColorAlpha", "Alpha channel of color", 0.0, 1.0, 1.0, rw);
  obj_properties[P_COLOR2_ALPHA] = g_param_spec_float ("gliko-color2-alpha", "ColorAlpha", "Alpha channel of color", 0.0, 1.0, 1.0, rw);
  obj_properties[P_BACKGROUND_ALPHA] = g_param_spec_float ("gliko-background-alpha", "BackgroundAlpha", "Alpha channel of background", 0.0, 1.0, 1.0, rw);

  obj_properties[P_COLOR1] = g_param_spec_string ("gliko-color1-rgb", "Color1", "Color for channel 1 #RRGGBB", "#FFFFFF", rw);
  obj_properties[P_COLOR2] = g_param_spec_string ("gliko-color2-rgb", "Color2", "Color for channel 2 #RRGGBB", "#FFFFFF", rw);
  obj_properties[P_BACKGROUND] = g_param_spec_string ("gliko-background-rgb", "Background", "Background #RRGGBB", "#000000", rw);

  g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);
}

static void
hyscan_gtk_gliko_area_init (HyScanGtkGlikoArea *area)
{
  HyScanGtkGlikoAreaPrivate *p;

  //p = G_TYPE_INSTANCE_GET_PRIVATE (area, HYSCAN_TYPE_GTK_GLIKO_AREA, HyScanGtkGlikoAreaPrivate);
  p = hyscan_gtk_gliko_area_get_instance_private (area);

  /* Create cache for faster access */
  area->priv = p;

  p->init_stage = 0;
  p->program = 0;
  p->vao = 0;

  p->w = 0;
  p->h = 0;

  p->n_tex = 0;
  p->nd_bits = 0;
  p->na_bits = 0;
  p->nd = 0;
  p->na = 0;
  p->bottom = 0;

  p->center_x = 0.f;
  p->center_y = 0.f;
  p->scale = 1.0f;
  p->bright = 0.f;
  p->contrast = 0.f;
  p->black = 0.0f;
  p->white = 1.0f;
  p->gamma = 1.0f;
  p->rotate = 0.f;
  p->rotate1 = 270.0f;
  p->rotate2 = 90.0f;
  p->remain = 0.5f;

  p->buf[0][0] = NULL;
}

static void
hyscan_gtk_gliko_area_object_finalize (GObject *object)
{
  HyScanGtkGlikoArea *gtk_gliko_area = HYSCAN_GTK_GLIKO_AREA (object);
  HyScanGtkGlikoAreaPrivate *p = gtk_gliko_area->priv;

  glDeleteVertexArrays (1, &p->vao);
  glDeleteBuffers (1, &p->vbo);
  glDeleteBuffers (1, &p->ebo);

  if (p->buf[0][0] != NULL)
    {
      free (p->buf[0][0]);
      p->buf[0][0] = NULL;
    }

  G_OBJECT_CLASS (hyscan_gtk_gliko_area_parent_class)
      ->finalize (object);
}

static float *
get_pfloat (HyScanGtkGlikoAreaPrivate *p, const int id)
{
  switch (id)
    {
    case P_CX:
      return &p->center_x;
    case P_CY:
      return &p->center_y;
    case P_SCALE:
      return &p->scale;
    case P_BLACK:
      return &p->black;
    case P_WHITE:
      return &p->white;
    case P_GAMMA:
      return &p->gamma;
    case P_BRIGHT:
      return &p->bright;
    case P_CONTRAST:
      return &p->contrast;
    case P_ROTATE:
      return &p->rotate;
    case P_FADE_COEF:
      return &p->fade_coef;
    case P_REMAIN:
      return &p->remain;
    case P_COLOR1_ALPHA:
      return &p->color[0][3];
    case P_COLOR2_ALPHA:
      return &p->color[1][3];
    case P_BACKGROUND_ALPHA:
      return &p->background[3];
    default:
      break;
    }
  return NULL;
}

static float *
get_prgb (HyScanGtkGlikoAreaPrivate *p, const int id)
{
  switch (id)
    {
    case P_COLOR1:
      return p->color[0];
    case P_COLOR2:
      return p->color[1];
    case P_BACKGROUND:
      return p->background;
    default:
      break;
    }
  return NULL;
}

static int *
get_pint (HyScanGtkGlikoAreaPrivate *p, const int id)
{
  switch (id)
    {
    case P_BOTTOM:
      return &p->bottom;
    default:
      break;
    }
  return NULL;
}


/* Overriden g_object methods */
static void
hyscan_gtk_gliko_area_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  HyScanGtkGlikoArea *gtk_gliko_area = HYSCAN_GTK_GLIKO_AREA (object);
  HyScanGtkGlikoAreaPrivate *p = gtk_gliko_area->priv;
  float *pf;
  int *pi;

  if ((pi = get_pint (p, prop_id)) != NULL)
    {
      *pi = g_value_get_int (value);
    }
  else if ((pf = get_pfloat (p, prop_id)) != NULL)
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
hyscan_gtk_gliko_area_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  HyScanGtkGlikoArea *gtk_gliko_area = HYSCAN_GTK_GLIKO_AREA (object);
  HyScanGtkGlikoAreaPrivate *p = gtk_gliko_area->priv;
  float *pf;
  int *pi;

  if ((pi = get_pint (p, prop_id)) != NULL)
    {
      g_value_set_int (value, *pi);
    }
  else if ((pf = get_pfloat (p, prop_id)) != NULL)
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

static void
hyscan_gtk_gliko_area_object_constructed (GObject *object)
{
  /*
  HyScanGtkGlikoArea *gtk_gliko_area = HYSCAN_GTK_GLIKO_AREA (object);
  HyScanGtkGlikoAreaPrivate *p = gtk_gliko_area->priv;
  */
}

static void
hyscan_gtk_gliko_area_interface_init (HyScanGtkGlikoLayerInterface *iface)
{
  iface->realize = layer_realize;
  iface->render = layer_render;
  iface->resize = layer_resize;
}

HyScanGtkGlikoArea *
hyscan_gtk_gliko_area_new (void)
{
  return HYSCAN_GTK_GLIKO_AREA (g_object_new (hyscan_gtk_gliko_area_get_type (), NULL));
}
