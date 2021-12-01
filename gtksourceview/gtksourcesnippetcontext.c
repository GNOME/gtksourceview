/*
 * This file is part of GtkSourceView
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "gtksourcesnippetcontext-private.h"

/**
 * GtkSourceSnippetContext:
 *
 * Context for expanding [class@SnippetChunk].
 *
 * This class is currently used primary as a hashtable. However, the longer
 * term goal is to have it hold onto a `GjsContext` as well as other languages
 * so that [class@SnippetChunk] can expand themselves by executing
 * script within the context.
 *
 * The [class@Snippet] will build the context and then expand each of the
 * chunks during the insertion/edit phase.
 */

struct _GtkSourceSnippetContext
{
  GObject     parent_instance;

  GHashTable *constants;
  GHashTable *variables;
  gchar      *line_prefix;
  gint        tab_width;
  guint       use_spaces : 1;
};

struct _GtkSourceSnippetContextClass
{
  GObjectClass parent;
};

G_DEFINE_TYPE (GtkSourceSnippetContext, gtk_source_snippet_context, G_TYPE_OBJECT)

enum {
  CHANGED,
  N_SIGNALS
};

typedef gchar *(*InputFilter) (const gchar *input);

static GHashTable *filters;
static guint signals[N_SIGNALS];

/**
 * gtk_source_snippet_context_new:
 *
 * Creates a new #GtkSourceSnippetContext.
 *
 * Generally, this isn't needed unless you are controlling the
 * expansion of snippets manually.
 *
 * Returns: (transfer full): a #GtkSourceSnippetContext
 */
GtkSourceSnippetContext *
gtk_source_snippet_context_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_SNIPPET_CONTEXT, NULL);
}

/**
 * gtk_source_snippet_context_clear_variables:
 * @self: a #GtkSourceSnippetContext
 *
 * Removes all variables from the context.
 */
void
gtk_source_snippet_context_clear_variables (GtkSourceSnippetContext *self)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CONTEXT (self));

	g_hash_table_remove_all (self->variables);
}

/**
 * gtk_source_snippet_context_set_variable:
 * @self: a #GtkSourceSnippetContext
 * @key: the variable name
 * @value: the value for the variable
 *
 * Sets a variable within the context.
 *
 * This variable may be overridden by future updates to the
 * context.
 */
void
gtk_source_snippet_context_set_variable (GtkSourceSnippetContext *self,
                                         const gchar             *key,
                                         const gchar             *value)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CONTEXT (self));
	g_return_if_fail (key);

	g_hash_table_replace (self->variables, g_strdup (key), g_strdup (value));
}

/**
 * gtk_source_snippet_context_set_constant:
 * @self: a #GtkSourceSnippetContext
 * @key: the constant name
 * @value: the value of the constant
 *
 * Sets a constatnt within the context. 
 *
 * This is similar to a variable set with [method@SnippetContext.set_variable]
 * but is expected to not change during use of the snippet.
 *
 * Examples would be the date or users name.
 */
void
gtk_source_snippet_context_set_constant (GtkSourceSnippetContext *self,
                                         const gchar             *key,
                                         const gchar             *value)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CONTEXT (self));
	g_return_if_fail (key);

	g_hash_table_replace (self->constants, g_strdup (key), g_strdup (value));
}

/**
 * gtk_source_snippet_context_get_variable:
 * @self: a #GtkSourceSnippetContext
 * @key: the name of the variable
 *
 * Gets the current value for a variable named @key.
 *
 * Returns: (transfer none) (nullable): the value for the variable, or %NULL
 */
const gchar *
gtk_source_snippet_context_get_variable (GtkSourceSnippetContext *self,
                                         const gchar             *key)
{
	const gchar *ret;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_CONTEXT (self), NULL);

	if (!(ret = g_hash_table_lookup (self->variables, key)))
		ret = g_hash_table_lookup (self->constants, key);

	return ret;
}

static gchar *
filter_lower (const gchar *input)
{
	return input != NULL ? g_utf8_strdown (input, -1) : NULL;
}

static gchar *
filter_upper (const gchar *input)
{
	return input != NULL ? g_utf8_strup (input, -1) : NULL;
}

static gchar *
filter_capitalize (const gchar *input)
{
	gunichar c;
	GString *str;

	if (input == NULL)
		return NULL;

	if (*input == 0)
		return g_strdup ("");

	c = g_utf8_get_char (input);
	if (g_unichar_isupper (c))
		return g_strdup (input);

	str = g_string_new (NULL);
	input = g_utf8_next_char (input);
	g_string_append_unichar (str, g_unichar_toupper (c));
	if (*input)
		g_string_append (str, input);

	return g_string_free (str, FALSE);
}

static gchar *
filter_uncapitalize (const gchar *input)
{
	gunichar c;
	GString *str;

	if (input == NULL)
		return NULL;

	c = g_utf8_get_char (input);
	if (g_unichar_islower (c))
		return g_strdup (input);

	str = g_string_new (NULL);
	input = g_utf8_next_char (input);
	g_string_append_unichar (str, g_unichar_tolower (c));
	g_string_append (str, input);

	return g_string_free (str, FALSE);
}

static gchar *
filter_html (const gchar *input)
{
	GString *str;

	if (input == NULL)
		return NULL;

	str = g_string_new (NULL);

	for (; *input; input = g_utf8_next_char (input))
	{
		gunichar c = g_utf8_get_char (input);

		switch (c)
		{
		case '<':
			g_string_append_len (str, "&lt;", 4);
			break;

		case '>':
			g_string_append_len (str, "&gt;", 4);
			break;

		case '&':
			g_string_append_len (str, "&amp;", 5);
			break;

		default:
			g_string_append_unichar (str, c);
			break;
		}
	}

	return g_string_free (str, FALSE);
}

static gchar *
filter_camelize (const gchar *input)
{
	gboolean next_is_upper = TRUE;
	gboolean skip = FALSE;
	GString *str;

	if (input == NULL)
		return NULL;

	if (!strchr (input, '_') && !strchr (input, ' ') && !strchr (input, '-'))
		return filter_capitalize (input);

	str = g_string_new (NULL);

	for (; *input; input = g_utf8_next_char (input))
	{
		gunichar c = g_utf8_get_char (input);

		switch (c)
		{
		case '_':
		case '-':
		case ' ':
			next_is_upper = TRUE;
			skip = TRUE;
			break;

		default:
			break;
		}

		if (skip)
		{
			skip = FALSE;
			continue;
		}

		if (next_is_upper)
		{
			c = g_unichar_toupper (c);
			next_is_upper = FALSE;
		}
		else
		{
			c = g_unichar_tolower (c);
		}

		g_string_append_unichar (str, c);
	}

	if (g_str_has_suffix (str->str, "Private"))
		g_string_truncate (str, str->len - strlen ("Private"));

	return g_string_free (str, FALSE);
}

static gchar *
filter_functify (const gchar *input)
{
	gunichar last = 0;
	GString *str;

	if (input == NULL)
		return NULL;

	str = g_string_new (NULL);

	for (; *input; input = g_utf8_next_char (input))
	{
		gunichar c = g_utf8_get_char (input);
		gunichar n = g_utf8_get_char (g_utf8_next_char (input));

		if (last)
		{
			if ((g_unichar_islower (last) && g_unichar_isupper (c)) ||
			    (g_unichar_isupper (c) && g_unichar_islower (n)))
			{
				g_string_append_c (str, '_');
			}
		}

		if ((c == ' ') || (c == '-'))
		{
			c = '_';
		}

		g_string_append_unichar (str, g_unichar_tolower (c));

		last = c;
	}

	if (g_str_has_suffix (str->str, "_private") ||
	    g_str_has_suffix (str->str, "_PRIVATE"))
	{
		g_string_truncate (str, str->len - strlen ("_private"));
	}

	return g_string_free (str, FALSE);
}

static gchar *
filter_namespace (const gchar *input)
{
	gunichar last = 0;
	GString *str;
	gboolean first_is_lower = FALSE;

	if (input == NULL)
		return NULL;

	str = g_string_new (NULL);

	for (; *input; input = g_utf8_next_char (input))
	{
		gunichar c = g_utf8_get_char (input);
		gunichar n = g_utf8_get_char (g_utf8_next_char (input));

		if (c == '_')
			break;

		if (last)
		{
			if ((g_unichar_islower (last) && g_unichar_isupper (c)) ||
			    (g_unichar_isupper (c) && g_unichar_islower (n)))
				break;
		}
		else
		{
			first_is_lower = g_unichar_islower (c);
		}

		if ((c == ' ') || (c == '-'))
			break;

		g_string_append_unichar (str, c);

		last = c;
	}

	if (first_is_lower)
	{
		gchar *ret;

		ret = filter_capitalize (str->str);
		g_string_free (str, TRUE);

		return g_steal_pointer (&ret);
	}

	return g_string_free (str, FALSE);
}

static gchar *
filter_class (const gchar *input)
{
	gchar *camel;
	gchar *ns;
	gchar *ret = NULL;

	if (input == NULL)
		return NULL;

	camel = filter_camelize (input);
	ns = filter_namespace (input);

	if (g_str_has_prefix (camel, ns))
	{
		ret = g_strdup (camel + strlen (ns));
	}
	else
	{
		ret = camel;
		camel = NULL;
	}

	g_free (camel);
	g_free (ns);

	return ret;
}

static gchar *
filter_instance (const gchar *input)
{
	const gchar *tmp;
	gchar *funct = NULL;
	gchar *ret;

	if (input == NULL)
		return NULL;

	if (!strchr (input, '_'))
	{
		funct = filter_functify (input);
		input = funct;
	}

	if ((tmp = strrchr (input, '_')))
	{
		ret = g_strdup (tmp+1);
	}
	else
	{
		ret = g_strdup (input);
	}

	g_free (funct);

	return g_steal_pointer (&ret);
}

static gchar *
filter_space (const gchar *input)
{
	GString *str;

	if (input == NULL)
		return NULL;

	str = g_string_new (NULL);
	for (; *input; input = g_utf8_next_char (input))
		g_string_append_c (str, ' ');

	return g_string_free (str, FALSE);
}

static gchar *
filter_descend_path (const gchar *input)
{
	const gchar *pos;

	if (input == NULL)
		return NULL;

	while (*input == G_DIR_SEPARATOR)
	{
		input++;
	}

	if ((pos = strchr (input, G_DIR_SEPARATOR)))
	{
		return g_strdup (pos + 1);
	}

	return NULL;
}

static gchar *
filter_stripsuffix (const gchar *input)
{
	const gchar *endpos;

	if (input == NULL)
		return NULL;

	endpos = strrchr (input, '.');

	if (endpos != NULL)
	{
		return g_strndup (input, (endpos - input));
	}

	return g_strdup (input);
}

static gchar *
filter_slash_to_dots (const gchar *input)
{
	GString *str;

	if (input == NULL)
		return NULL;

	str = g_string_new (NULL);

	for (; *input; input = g_utf8_next_char (input))
	{
		gunichar ch = g_utf8_get_char (input);

		if (ch == G_DIR_SEPARATOR)
		{
			g_string_append_c (str, '.');
		}
		else
		{
			g_string_append_unichar (str, ch);
		}
	}

	return g_string_free (str, FALSE);
}

static gchar *
apply_filter (gchar       *input,
              const gchar *filter)
{
	InputFilter filter_func;

	if ((filter_func = g_hash_table_lookup (filters, filter)))
	{
		gchar *tmp = input;
		input = filter_func (input);
		g_free (tmp);
	}

	return input;
}

static gchar *
apply_filters (GString     *str,
               const gchar *filters_list)
{
	gchar **filter_names;
	gchar *input = g_string_free (str, FALSE);

	filter_names = g_strsplit (filters_list, "|", 0);
	for (guint i = 0; filter_names[i]; i++)
		input = apply_filter (input, filter_names[i]);

	g_strfreev (filter_names);

	return input;
}

static gchar *
scan_forward (const gchar  *input,
              const gchar **endpos,
              gunichar      needle)
{
	const gchar *begin = input;

	for (; *input; input = g_utf8_next_char (input))
	{
		gunichar c = g_utf8_get_char (input);

		if (c == needle)
		{
			*endpos = input;
			return g_strndup (begin, (input - begin));
		}
	}

	*endpos = NULL;

	return NULL;
}

gchar *
gtk_source_snippet_context_expand (GtkSourceSnippetContext *self,
                                   const gchar             *input)
{
	const gchar *expand;
	gboolean is_dynamic;
	GString *str;
	gchar key[12];
	glong n;
	gint i;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_CONTEXT (self), NULL);
	g_return_val_if_fail (input, NULL);

	is_dynamic = (*input == '$');

	str = g_string_new (NULL);

	for (; *input; input = g_utf8_next_char (input))
	{
		gunichar c = g_utf8_get_char (input);

		if (c == '\\')
		{
			input = g_utf8_next_char (input);
			if (!*input)
				break;
			c = g_utf8_get_char (input);
		}
		else if (is_dynamic && c == '$')
		{
			input = g_utf8_next_char (input);

			if (!*input)
				break;

			c = g_utf8_get_char (input);

			if (g_unichar_isdigit (c))
			{
				errno = 0;
				n = strtol (input, (gchar * *) &input, 10);
				if (((n == LONG_MIN) || (n == LONG_MAX)) && errno == ERANGE)
					break;
				input--;
				g_snprintf (key, sizeof key, "%ld", n);
				key[sizeof key - 1] = '\0';
				expand = gtk_source_snippet_context_get_variable (self, key);
				if (expand)
					g_string_append (str, expand);
				continue;
			}
			else
			{
				if (strchr (input, '|'))
				{
					gchar *lkey;

					lkey = g_strndup (input, strchr (input, '|') - input);
					expand = gtk_source_snippet_context_get_variable (self, lkey);
					g_free (lkey);

					if (expand)
					{
						g_string_append (str, expand);
						input = strchr (input, '|') - 1;
					}
					else
					{
						input += strlen (input) - 1;
					}
				}
				else
				{
					expand = gtk_source_snippet_context_get_variable (self, input);

					if (expand)
					{
						g_string_append (str, expand);
					}
					else
					{
						g_string_append_c (str, '$');
						g_string_append (str, input);
					}

					input += strlen (input) - 1;
				}

				continue;
			}
		}
		else if (is_dynamic && c == '|')
		{
			return apply_filters (str, input + 1);
		}
		else if (c == '`')
		{
			const gchar *endpos = NULL;
			gchar *slice;

			slice = scan_forward (input + 1, &endpos, '`');

			if (slice)
			{
				gchar *expanded;

				input = endpos;

				expanded = gtk_source_snippet_context_expand (self, slice);

				g_string_append (str, expanded);

				g_free (expanded);
				g_free (slice);

				continue;
			}
		}
		else if (c == '\t')
		{
			if (self->use_spaces)
			{
				for (i = 0; i < self->tab_width; i++)
					g_string_append_c (str, ' ');
			}
			else
			{
				g_string_append_c (str, '\t');
			}

			continue;
		}
		else if (c == '\n')
		{
			g_string_append_c (str, '\n');

			if (self->line_prefix)
			{
				g_string_append (str, self->line_prefix);
			}

			continue;
		}

		g_string_append_unichar (str, c);
	}

	return g_string_free (str, FALSE);
}

void
gtk_source_snippet_context_set_tab_width (GtkSourceSnippetContext *self,
                                          gint                     tab_width)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CONTEXT (self));

	if (tab_width != self->tab_width)
	{
		self->tab_width = tab_width;
	}
}

void
gtk_source_snippet_context_set_use_spaces (GtkSourceSnippetContext *self,
                                           gboolean                 use_spaces)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CONTEXT (self));

	use_spaces = !!use_spaces;

	if (self->use_spaces != use_spaces)
	{
		self->use_spaces = use_spaces;
	}
}

void
gtk_source_snippet_context_set_line_prefix (GtkSourceSnippetContext *self,
                                            const gchar             *line_prefix)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CONTEXT (self));

	if (g_strcmp0 (line_prefix, self->line_prefix) != 0)
	{
		g_free (self->line_prefix);
		self->line_prefix = g_strdup (line_prefix);
	}
}

void
_gtk_source_snippet_context_emit_changed (GtkSourceSnippetContext *self)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CONTEXT (self));

	g_signal_emit (self, signals [CHANGED], 0);
}

static void
gtk_source_snippet_context_finalize (GObject *object)
{
	GtkSourceSnippetContext *self = (GtkSourceSnippetContext *)object;

	g_clear_pointer (&self->constants, g_hash_table_unref);
	g_clear_pointer (&self->variables, g_hash_table_unref);
	g_clear_pointer (&self->line_prefix, g_free);

	G_OBJECT_CLASS (gtk_source_snippet_context_parent_class)->finalize (object);
}

static void
gtk_source_snippet_context_class_init (GtkSourceSnippetContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_snippet_context_finalize;

	/**
	 * GtkSourceSnippetContext::changed:
	 *
	 * The signal is emitted when a change has been
	 * discovered in one of the chunks of the snippet which has
	 * caused a variable or other dynamic data within the context
	 * to have changed.
	 */
	signals[CHANGED] =
		g_signal_new ("changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_FIRST,
		              0,
		              NULL, NULL, NULL,
		              G_TYPE_NONE,
		              0);

	filters = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (filters, (gpointer) "lower", filter_lower);
	g_hash_table_insert (filters, (gpointer) "upper", filter_upper);
	g_hash_table_insert (filters, (gpointer) "capitalize", filter_capitalize);
	g_hash_table_insert (filters, (gpointer) "decapitalize", filter_uncapitalize);
	g_hash_table_insert (filters, (gpointer) "uncapitalize", filter_uncapitalize);
	g_hash_table_insert (filters, (gpointer) "html", filter_html);
	g_hash_table_insert (filters, (gpointer) "camelize", filter_camelize);
	g_hash_table_insert (filters, (gpointer) "functify", filter_functify);
	g_hash_table_insert (filters, (gpointer) "namespace", filter_namespace);
	g_hash_table_insert (filters, (gpointer) "class", filter_class);
	g_hash_table_insert (filters, (gpointer) "space", filter_space);
	g_hash_table_insert (filters, (gpointer) "stripsuffix", filter_stripsuffix);
	g_hash_table_insert (filters, (gpointer) "instance", filter_instance);
	g_hash_table_insert (filters, (gpointer) "slash_to_dots", filter_slash_to_dots);
	g_hash_table_insert (filters, (gpointer) "descend_path", filter_descend_path);
}

static void
gtk_source_snippet_context_init (GtkSourceSnippetContext *self)
{
	static const struct {
		const gchar *name;
		const gchar *format;
	} date_time_formats[] = {
		{ "CURRENT_YEAR", "%Y" },
		{ "CURRENT_YEAR_SHORT", "%y" },
		{ "CURRENT_MONTH", "%m" },
		{ "CURRENT_MONTH_NAME", "%B" },
		{ "CURRENT_MONTH_NAME_SHORT", "%b" },
		{ "CURRENT_DATE", "%e" },
		{ "CURRENT_DAY_NAME", "%A" },
		{ "CURRENT_DAY_NAME_SHORT", "%a" },
		{ "CURRENT_HOUR", "%H" },
		{ "CURRENT_MINUTE", "%M" },
		{ "CURRENT_SECOND", "%S" },
		{ "CURRENT_SECONDS_UNIX", "%s" },
	};
	GDateTime *dt;

	self->variables = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	self->constants = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

#define SET_CONSTANT(k, v) g_hash_table_insert (self->constants, g_strdup (k), g_strdup (v))
	SET_CONSTANT ("NAME_SHORT", g_get_user_name ());
	SET_CONSTANT ("NAME", g_get_real_name ());
	SET_CONSTANT ("EMAIL", "");
	SET_CONSTANT ("TM_FILENAME", "");
#undef SET_CONSTANT

	dt = g_date_time_new_now_local ();

	for (guint i = 0; i < G_N_ELEMENTS (date_time_formats); i++)
	{
		g_hash_table_insert (self->constants,
		                     g_strdup (date_time_formats[i].name),
		                     g_date_time_format (dt, date_time_formats[i].format));
	}

	g_date_time_unref (dt);
}
