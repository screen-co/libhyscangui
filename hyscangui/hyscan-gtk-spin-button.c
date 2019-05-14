/* hyscan-gtk-spin-button.c
 *
 * Copyright 2018 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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

/**
 * SECTION: hyscan-gtk-spin-button
 * @Short_description: GtkSpinButton с поддержкой других систем счисления
 * @Title: HyScanGtkSpinButton
 *
 * Виджет является расширением #GtkSpinButton и позволяет работать с двоичными
 * и шестнадцатеричными числами. Сам класс никаких сигналов не рассылает,
 * пользователю предлагается подписываться на "notify::value". При этом
 * передается число в десятичной системе, а в двоичной и шестнадцатеричной они
 * только отображаются (или вводятся пользователем).
 */
#include "hyscan-gtk-spin-button.h"
#include <glib/gprintf.h>
#include <string.h>
#include <ctype.h>

typedef gchar* (filter_func) (const gchar * src);
typedef gint64 (parse_func) (const gchar * src);
typedef gchar* (format_func) (gint64   value,
                              gboolean prefix);

enum
{
  PROP_0,
  PROP_BASE,
  PROP_SHOW_PREFIX,

};

struct _HyScanGtkSpinButton
{
  GtkSpinButton  parent_instance;

  guint          base;            /* Основание системы счисления. */
  filter_func   *filter;          /* Функция фильтрации ввода. */
  parse_func    *parse;           /* Функция разбора значения. */
  format_func   *format;          /* Функция печати значения. */

  gboolean       show_prefix;     /* Показывать ли префикс. */
};

static void     hyscan_gtk_spin_button_set_property       (GObject          *object,
                                                           guint             prop_id,
                                                           const GValue     *value,
                                                           GParamSpec       *pspec);
static void     hyscan_gtk_spin_button_object_constructed (GObject          *object);
static void     hyscan_gtk_spin_button_buffer_changed     (GtkEntry         *self,
                                                           GParamSpec       *pspec);
static void     hyscan_gtk_spin_button_swap_base          (GtkEntry         *self,
                                                           GtkEntryIconPosition icon_pos,
                                                           GdkEvent         *event);
static gint     hyscan_gtk_spin_button_input              (GtkSpinButton    *self,
                                                           gdouble          *new_val);
static gboolean hyscan_gtk_spin_button_output             (GtkSpinButton    *self);
static void     hyscan_gtk_spin_button_text_changed       (GtkEntryBuffer   *buffer,
                                                           GParamSpec       *pspec,
                                                           HyScanGtkSpinButton *spin);

static gchar *  hyscan_gtk_spin_button_filter_hex         (const gchar      *src);
static gchar *  hyscan_gtk_spin_button_filter_bin         (const gchar      *src);
static gint64   hyscan_gtk_spin_button_parse_hex          (const gchar      *src);
static gint64   hyscan_gtk_spin_button_parse_bin          (const gchar      *src);
static gchar *  hyscan_gtk_spin_button_format_hex         (gint64            val,
                                                           gboolean          prefix);
static gchar *  hyscan_gtk_spin_button_format_bin         (gint64            val,
                                                           gboolean          prefix);

G_DEFINE_TYPE (HyScanGtkSpinButton, hyscan_gtk_spin_button, GTK_TYPE_SPIN_BUTTON);

static void
hyscan_gtk_spin_button_class_init (HyScanGtkSpinButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkSpinButtonClass *spin_class = GTK_SPIN_BUTTON_CLASS (klass);

  spin_class->input = hyscan_gtk_spin_button_input;
  spin_class->output = hyscan_gtk_spin_button_output;

  object_class->constructed = hyscan_gtk_spin_button_object_constructed;
  object_class->set_property = hyscan_gtk_spin_button_set_property;

  g_object_class_install_property (object_class, PROP_BASE,
    g_param_spec_uint ("base", "Base", "Numeral system base (2, 10, 16)", 2, 16, 10,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_SHOW_PREFIX,
    g_param_spec_boolean ("show-prefix", "ShowPrefix", "Set prefix (0b, 0x) visibility", TRUE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
}

static void
hyscan_gtk_spin_button_init (HyScanGtkSpinButton *self)
{
}

static void
hyscan_gtk_spin_button_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  HyScanGtkSpinButton *self = HYSCAN_GTK_SPIN_BUTTON (object);

  if (prop_id == PROP_BASE)
    hyscan_gtk_spin_button_set_base (self, g_value_get_uint (value));
  else if (prop_id == PROP_SHOW_PREFIX)
    hyscan_gtk_spin_button_set_show_prefix (self, g_value_get_boolean (value));
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
hyscan_gtk_spin_button_object_constructed (GObject *object)
{
  HyScanGtkSpinButton *self = HYSCAN_GTK_SPIN_BUTTON (object);

  G_OBJECT_CLASS (hyscan_gtk_spin_button_parent_class)->constructed (object);

  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (self),
                                     GTK_ENTRY_ICON_SECONDARY, "gtk-refresh");

  g_signal_connect (self, "notify::buffer",
                    G_CALLBACK (hyscan_gtk_spin_button_buffer_changed), NULL);
  g_signal_connect (self, "icon-release",
                    G_CALLBACK (hyscan_gtk_spin_button_swap_base), NULL);
}

static void
hyscan_gtk_spin_button_buffer_changed (GtkEntry   *self,
                                       GParamSpec *pspec)
{
  GtkEntryBuffer *entry_buffer;

  entry_buffer = gtk_entry_get_buffer (GTK_ENTRY (self));
  g_signal_connect (entry_buffer, "notify::text",
                    G_CALLBACK (hyscan_gtk_spin_button_text_changed), self);
}

/* Функция циклически меняет основание системы счисления. */
static void
hyscan_gtk_spin_button_swap_base (GtkEntry             *entry,
                                  GtkEntryIconPosition  icon_pos,
                                  GdkEvent             *event)
{
  HyScanGtkSpinButton *self = HYSCAN_GTK_SPIN_BUTTON (entry);

  if (self->base == 2)
    hyscan_gtk_spin_button_set_base (self, 10);
  else if (self->base == 10)
    hyscan_gtk_spin_button_set_base (self, 16);
  else if (self->base == 16)
    hyscan_gtk_spin_button_set_base (self, 2);
}

/* Функция отлавливает сигнал "input" и превращает текст в значение. */
gint
hyscan_gtk_spin_button_input (GtkSpinButton *self,
                              gdouble       *new_val)
{
  parse_func *parser = HYSCAN_GTK_SPIN_BUTTON (self)->parse;
  const gchar *text;
  gint64 value;

  if (parser == NULL)
    return FALSE;

  text = gtk_entry_get_text (GTK_ENTRY (self));
  value = parser (text);
  *new_val = value;

  return TRUE;
}

/* Функция отлавливает сигнал "output" и форматирует вывод. */
gboolean
hyscan_gtk_spin_button_output (GtkSpinButton *self)
{
  HyScanGtkSpinButton *spin_button = HYSCAN_GTK_SPIN_BUTTON (self);
  format_func *formatter = spin_button->format;
  GtkAdjustment *adjustment;
  gchar *text;
  gint64 value;

  if (formatter == NULL)
    return FALSE;

  adjustment = gtk_spin_button_get_adjustment (self);
  value = gtk_adjustment_get_value (adjustment);

  text = formatter (value, spin_button->show_prefix);
  gtk_entry_set_text (GTK_ENTRY (self), text);
  g_free (text);

  return TRUE;
}

/* Обработчик "text-changed" от нижележащего GtkEntryBuffer.
 * Отбрасывает из ввода лишние символы. */
static void
hyscan_gtk_spin_button_text_changed (GtkEntryBuffer   *buffer,
                                     GParamSpec       *pspec,
                                     HyScanGtkSpinButton *self)
{
  filter_func *filterer = self->filter;
  gchar *src = NULL;
  gchar *dst = NULL;

  if (filterer == NULL)
    return;

  g_object_get (buffer, "text", &src, NULL);

  if (src == NULL)
    return;

  dst = self->filter (src);

  if (dst != NULL)
    g_object_set (buffer, "text", dst, NULL);

  g_free (src);
  g_free (dst);
}

/* Функция фильтрации шестнадцатеричных чисел. */
static gchar *
hyscan_gtk_spin_button_filter_hex (const gchar *src)
{
  gint len, i, j;
  gchar * dst;
  gboolean changed = FALSE;

  j = i = 0;
  len = strlen (src);
  dst = g_malloc0 (len + 1);

  if (len >= 2 && src[0] == '0' && src[1] == 'x')
    {
      dst[j++] = src[i++]; /* 0 */
      dst[j++] = src[i++]; /* x */
    }

  for (; i < len; ++i)
    {
      gchar c = src[i];

      /* Если есть хоть один не hex-символ, он будет отфильтрован.
       * hex-символы просто переписываем. */
      if (isxdigit (c))
        dst[j++] = c;
      else
        changed = TRUE;
    }

  if (!changed)
    g_clear_pointer (&dst, g_free);

  return dst;
}

/* Функция фильтрации двоичных чисел. */
static gchar *
hyscan_gtk_spin_button_filter_bin (const gchar *src)
{
  gint len;
  gchar * dst;
  gint i, j;
  gboolean changed = FALSE;

  i = j = 0;
  len = strlen (src);
  dst = g_malloc0 (len + 1);

  if (len >= 2 && src[0] == '0' && src[1] == 'b')
    {
      dst[j++] = src[i++]; /* 0 */
      dst[j++] = src[i++]; /* b */
    }

  for (; i < len; ++i)
    {
      gchar c = src[i];

      if (c == '0' || c == '1')
        dst[j++] = c;
      else
        changed = TRUE;

    }

  if (!changed)
    g_clear_pointer (&dst, g_free);

  return dst;
}

/* Функция парсинга шестнадцатеричных чисел. */
static gint64
hyscan_gtk_spin_button_parse_hex (const gchar *src)
{
  guint64 raw = g_ascii_strtoull (src, NULL, 16);
  gint64 value = *(gint64*)&raw;
  return value;
}

/* Функция парсинга двоичных чисел. */
static gint64
hyscan_gtk_spin_button_parse_bin (const gchar *src)
{
  guint64 raw;
  gint64 value;

  if (strlen (src) >= 2 && src[0] == '0' && src[1] == 'b')
    src += 2;

  raw = g_ascii_strtoull (src, NULL, 2);
  value = *(gint64*)&raw;

  return value;
}

/* Функция форматирования шестнадцатеричных чисел. */
static gchar *
hyscan_gtk_spin_button_format_hex (gint64   value,
                                   gboolean prefix)
{
  return g_strdup_printf ("%s%lX", prefix ? "0x" : "", value);
}

/* Функция печатает ровно 1 байт. */
static gboolean
hyscan_gtk_spin_button_print_byte (guchar    byte,
                                   gboolean  omit_leading,
                                   gboolean  reduce,
                                   gchar    *dst)
{
  gint count = 0;
  gint j;
  guchar bit;

  /* Copying_and_pasting_from_StackOverflow.png
   * https://stackoverflow.com/a/3974138/4226702 */

  /* Если выставлен флаг omit_leading,
   * то надо найти первый ненулевой бит. */
  for (j = 7; omit_leading && j >= 0; --j)
    {
      bit = (byte >> j) & 1;
      if (bit)
        break;
    }

  /* Если выставлен reduce, надо напечатать "0" */
  if (reduce && byte == 0)
    j = 0;

  /* Непосредственно печать. */
  for (count = 0; j >= 0; --j)
    {
      bit = (byte >> j) & 1;
      count += g_sprintf (dst + count, "%u", bit);
    }

  /* Возвращаем количество напечатанных символов. */
  return count;
}

/* Функция форматирования двоичных чисел. */
static gchar *
hyscan_gtk_spin_button_format_bin (gint64   value,
                                   gboolean prefix)
{
  gint i;
  gchar * str;
  gchar * strp;
  gboolean omit_leading = TRUE;
  guchar *b = (guchar*) &value;

  strp = str = g_malloc0 (70); /* "0b" + 64 + '\0' плюс немного сверху */

  if (prefix)
    strp += g_sprintf (strp, "0b");

  for (i = 7; i >= 0; --i)
    {
      gint nsyms;

      /* Существуют вот такие случаи:
       * 1 00000000 (256) - последний байт надо вывести целиком
       * 0 00000000 (0) - можно написать просто "0"
       * 0{1-7}х - надо отбросить лидирующие нули, которые не несут смысла
       *
       * - omit_leading висит TRUE до тех пор, пока не будет напечатан хоть один
       * значащий бит
       * - "0" будет написано, если до этого не было ни единого значащего И это
       * последний байт
       */
      nsyms = hyscan_gtk_spin_button_print_byte (b[i], omit_leading,
                                                 i == 0 && omit_leading, strp);

      if (nsyms)
        omit_leading = FALSE;

      strp += nsyms;
    }

  return str;
}

/**
 * hyscan_gtk_spin_button_new:
 *
 * Функция создаёт новый виджет #HyScanGtkSpinButton
 *
 * Returns: #HyScanGtkSpinButton.
 */
GtkWidget *
hyscan_gtk_spin_button_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_SPIN_BUTTON, NULL);
}

/**
 * hyscan_gtk_spin_button_set_base:
 * @self: виджет #HyScanGtkSpinButton
 * @base: основание системы счисления
 *
 * Поддерживаются только основания 2, 10 и 16.
 */
void
hyscan_gtk_spin_button_set_base (HyScanGtkSpinButton *self,
                                 guint                base)
{
  gdouble value;
  g_return_if_fail (HYSCAN_IS_GTK_SPIN_BUTTON (self));

  /* Запоминаем предыдущее значение. */
  value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self));

  if (base == 16)
    {
      self->base = 16;
      self->filter = hyscan_gtk_spin_button_filter_hex;
      self->parse = hyscan_gtk_spin_button_parse_hex;
      self->format = hyscan_gtk_spin_button_format_hex;
    }
  else if (base == 2)
    {
      self->base = 2;
      self->filter = hyscan_gtk_spin_button_filter_bin;
      self->parse = hyscan_gtk_spin_button_parse_bin;
      self->format = hyscan_gtk_spin_button_format_bin;
    }
  else if (base == 10)
    {
      self->base = 10;
      self->filter = NULL;
      self->parse = NULL;
      self->format = NULL;
    }
  else
    {
      g_warning ("HyScanGtkSpinButton: base %u is not supported. "
                 "Defaulting to 10.", base);
      hyscan_gtk_spin_button_set_base (self, 10);
    }

  /* Восстанавливаем значение, т.к. тот же текст распарсится по-другому. */
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self), value);
}

/**
 * hyscan_gtk_spin_button_set_show_prefix:
 * @self: виджет #HyScanGtkSpinButton
 * @show: TRUE, чтобы показывать префикс
 *
 * Для двоичной и шестнадцатеричной систем возможно отображение префиксов
 * (0b и 0x соответственно). Функция настраивает показ префикса.
 */
void
hyscan_gtk_spin_button_set_show_prefix (HyScanGtkSpinButton *self,
                                        gboolean             show)
{
  g_return_if_fail (HYSCAN_IS_GTK_SPIN_BUTTON (self));

  self->show_prefix = show;

  gtk_spin_button_update (GTK_SPIN_BUTTON (self));
}
