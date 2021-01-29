#include <gtk/gtk.h>
#include <GL/gl.h>
#include "hyscan-gtk-gliko-minimal.h"

struct _HyScanGtkGlikoMinimal
{
  GtkGLArea parent;
  HyScanGtkGlikoMinimalPrivate *priv;
};

struct _HyScanGtkGlikoMinimalClass
{
  GtkGLAreaClass parent_class;
};

struct _HyScanGtkGlikoMinimalPrivate
{
  gboolean panning;
  int w, h;
  int program; // shader program
  unsigned int vao, vbo, ebo;     // vertex array object
};

/* Define type */
G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkGlikoMinimal, hyscan_gtk_gliko_minimal, GTK_TYPE_GL_AREA);

/* Internal API */
static void finalize( GObject *gobject );

static void get_preferred_width( GtkWidget *widget, gint *minimal_width, gint *natural_width );
static void get_preferred_height( GtkWidget *widget, gint *minimal_height, gint *natural_height );

static const char *vertexShaderSource =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";
static const char *fragmentShaderSource =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
    "}\n\0";

static int create_shader_program()
{
  int vertexShader;
  int fragmentShader;
  int shaderProgram;
  int success;
  char infoLog[512];

  // build and compile our shader program
  // ------------------------------------
  // vertex shader
  vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
  glCompileShader(vertexShader);
  // check for shader compile errors
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    glGetShaderInfoLog(vertexShader, sizeof( infoLog ), NULL, infoLog);
    fprintf( stderr, "Vertex shader compile error: %s\n", infoLog );
    return -1;
  }
  // fragment shader
  fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
  glCompileShader(fragmentShader);
  // check for shader compile errors
  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    glGetShaderInfoLog(fragmentShader, sizeof( infoLog ), NULL, infoLog);
    fprintf( stderr, "Fragment compile error: %s\n", infoLog );
    return -1;
  }
  // link shaders
  shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);
  // check for linking errors
  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
  if (!success)
  {
    glGetProgramInfoLog(shaderProgram, sizeof( infoLog ), NULL, infoLog);
    fprintf( stderr, "Link program error: %s\n", infoLog );
    return -1;
  }
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
  return shaderProgram;
}

// VAO - vertex array object
static void create_model( unsigned int *vbo, unsigned int *vao, unsigned int *ebo )
{
  // set up vertex data (and buffer(s)) and configure vertex attributes
  // ------------------------------------------------------------------
  static const float vertices[] =
  { // positions          // texture coords
     0.5f,  0.5f, 0.0f,   1.0f, 1.0f, // top right
     0.5f, -0.5f, 0.0f,   1.0f, 0.0f, // bottom right
    -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, // bottom left
    -0.5f,  0.5f, 0.0f,   0.0f, 1.0f  // top left
  };
  static const unsigned int indices[] =
  {  // note that we start from 0!
     0, 1, 3,  // first Triangle
     1, 2, 3   // second Triangle
  };

  glGenVertexArrays(1, vao);
  glGenBuffers(1, vbo);
  glGenBuffers(1, ebo);
  // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
  glBindVertexArray(*vao);

  glBindBuffer(GL_ARRAY_BUFFER, *vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0 );
  glEnableVertexAttribArray(0);

  glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)) );
  glEnableVertexAttribArray(1);

  // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
  // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
  glBindVertexArray(0);
}

static void on_resize (GtkGLArea *area, gint width, gint height)
{
  glViewport( 0, 0, width, height );
}

static gboolean on_render( GtkGLArea *area, GdkGLContext *context )
{
  HyScanGtkGlikoMinimalPrivate *p;

  p = G_TYPE_INSTANCE_GET_PRIVATE( area, HYSCAN_TYPE_GTK_GLIKO_MINIMAL, HyScanGtkGlikoMinimalPrivate );

  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // uncomment this call to draw in wireframe polygons.
  //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  // render
  // ------
  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  if( p->program == -1 ) return TRUE;

  glUseProgram( p->program );
  glBindVertexArray( p->vao ); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
  //glDrawArrays( GL_TRIANGLES, 0, 3 );
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  // glBindVertexArray(0); // no need to unbind it every time

  return TRUE;
}

static void on_realize( GtkGLArea *area )
{
  HyScanGtkGlikoMinimalPrivate *p;
  GdkGLContext *glcontext;
  GdkWindow *glwindow;
  GdkFrameClock *frame_clock;

  p = G_TYPE_INSTANCE_GET_PRIVATE( area, HYSCAN_TYPE_GTK_GLIKO_MINIMAL, HyScanGtkGlikoMinimalPrivate );

  // Make current:
  gtk_gl_area_make_current( area );

  // Print version info:
  printf( "Renderer: %s\n", glGetString(GL_RENDERER) );
  printf( "OpenGL version supported %s\n", glGetString(GL_VERSION) );

  // Enable depth buffer:
  gtk_gl_area_set_has_depth_buffer(area, TRUE);

  // Init programs:
  p->program = create_shader_program();
  create_model( &p->vbo, &p->vao, &p->ebo );

  // Init background:
  //background_init();

  // Init model:
  //model_init();

  // Get frame clock:
  glcontext = gtk_gl_area_get_context(area);
  glwindow = gdk_gl_context_get_window(glcontext);
  frame_clock = gdk_window_get_frame_clock(glwindow);

  // Connect update signal:
  g_signal_connect_swapped( frame_clock, "update", G_CALLBACK(gtk_gl_area_queue_render), area );

  // Start updating:
  gdk_frame_clock_begin_updating(frame_clock);
}

/* Initialization */
static void hyscan_gtk_gliko_minimal_class_init( HyScanGtkGlikoMinimalClass *klass )
{
  GObjectClass *g_class = G_OBJECT_CLASS( klass );
  GtkWidgetClass *w_class = GTK_WIDGET_CLASS( klass );

  g_class->finalize = finalize;
  w_class->get_preferred_width  = get_preferred_width;
  w_class->get_preferred_height = get_preferred_height;
}

static void hyscan_gtk_gliko_minimal_init( HyScanGtkGlikoMinimal *area )
{
  HyScanGtkGlikoMinimalPrivate *p;

  p = G_TYPE_INSTANCE_GET_PRIVATE( area, HYSCAN_TYPE_GTK_GLIKO_MINIMAL, HyScanGtkGlikoMinimalPrivate );

  /* Create cache for faster access */
  area->priv = p;

  p->program = -1;
  p->vao = 0;

  p->w = 0;
  p->h = 0;

  g_signal_connect( area, "realize", G_CALLBACK(on_realize), NULL);
  g_signal_connect( area, "render", G_CALLBACK(on_render), NULL);
  g_signal_connect( area, "resize", G_CALLBACK(on_resize), NULL);
}

static void finalize( GObject *gobject )
{
  HyScanGtkGlikoMinimalPrivate *p;

  p = G_TYPE_INSTANCE_GET_PRIVATE( gobject, HYSCAN_TYPE_GTK_GLIKO_MINIMAL, HyScanGtkGlikoMinimalPrivate );

  glDeleteVertexArrays(1, &p->vao);
  glDeleteBuffers(1, &p->vbo);
  glDeleteBuffers(1, &p->ebo);

  G_OBJECT_CLASS( hyscan_gtk_gliko_minimal_parent_class )->finalize( gobject );
}

static void get_preferred_width( GtkWidget *widget, gint *minimal_width, gint *natural_width )
{
  *minimal_width = 512;
  *natural_width = 512;
}

static void get_preferred_height( GtkWidget *widget, gint *minimal_height, gint *natural_height )
{
  *minimal_height = 512;
  *natural_height = 512;
}

GtkWidget *hyscan_gtk_gliko_minimal_new( void )
{
  return GTK_WIDGET( g_object_new( hyscan_gtk_gliko_minimal_get_type(), NULL ) );
}
