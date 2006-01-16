/*  gtksourcelanguage-parser-2.c
 *  Language specification parser for 2.0 version .lang files
 *
 *  Copyright (C) 2003 - Gustavo Giráldez <gustavo.giraldez@gmx.net>
 *  Copyright (C) 2005 - Emanuele Aina, Marco Barisione
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*#define ENABLE_DEBUG*/

#ifdef ENABLE_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x)
#endif

/* TODO: included for strcmp(). Why there isn't something similar in glib? */
#include <string.h>

#include <libxml/xmlreader.h>

#include "gtksourceview-i18n.h"
#include "gtksourcebuffer.h"
#include "gtksourcetag.h"
#include "gtksourcelanguage.h"
#include "gtksourcelanguagesmanager.h"
#include "gtksourcelanguage-private.h"
#include "gtksourcecontextengine.h"
#include "libegg/regex/eggregex.h"


#define RNG_SCHEMA_DIR  DATADIR "/gtksourceview-1.0/schemas/language2.rng"

#define PARSER_ERROR parser_error_quark ()

typedef enum {
	PARSER_ERROR_CANNOT_OPEN     = 0,
	PARSER_ERROR_CANNOT_VALIDATE,
	PARSER_ERROR_INVALID_DOC,
	PARSER_ERROR_WRONG_VERSION,
	PARSER_ERROR_WRONG_ID,
	PARSER_ERROR_MALFORMED_REGEX,
	PARSER_ERROR_MALFORMED_MAP_TO
} ParserError;

struct _ParserState 
{
	/* The args passed to _file_parse_version2() */
	xmlTextReader *reader;
	GtkSourceLanguage *language;
	GtkSourceContextEngine *engine;
	GSList **tags;
	gboolean populate_styles_table;

	/* A stack of id that representing parent contexts */
	GQueue *curr_parents;

	/* The id of the current language (used to decorate ids) */
	gchar *current_lang_id;

	/* An hash table with the defined regex as strings used to
	 * resolve references (the key is the id) */
	GHashTable *defined_regexes;

	/* The mapping between style ids and their default styles */
	GHashTable *styles_mapping;

	/* The list of loaded languages */
	GSList **loaded_lang_ids;

	/* A serial number incremented to get unique generated names */
	guint id_cookie;

	/* The default flags used by the regexes */
	EggRegexCompileFlags regex_compile_flags;

	gchar *opening_delimiter;
	gchar *closing_delimiter;

	GError **error;
};
typedef struct _ParserState ParserState;



GQuark            parser_error_quark           (void);
static gboolean   str_to_bool                  (gchar *string);
static gchar     *generate_new_id              (ParserState *parser_state);
static gboolean   id_is_decorated              (gchar *id,
                                                gchar **lang_id);
static gchar     *decorate_id                  (ParserState *parser_state,
                                                gchar *id);

static ParserState *parser_state_new           (GtkSourceLanguage      *language,
                                                GSList                **tags,
                                                GtkSourceContextEngine *engine,
                                                gboolean                populate_styles_table,
                                                GHashTable             *defined_regexes,
                                                GHashTable             *styles_mapping,
                                                xmlTextReader          *reader,
                                                GSList                **loaded_lang_ids);
static void       parser_state_destroy         (ParserState *parser_state);

static gboolean   file_parse                   (gchar                  *filename,
                                                GtkSourceLanguage      *language,
                                                GSList                **tags,
                                                GtkSourceContextEngine *engine,
                                                gboolean                populate_styles_table,
                                                GHashTable             *defined_regexes,
                                                GHashTable             *styles,
                                                GSList                **loaded_lang_ids,
                                                GError                **error);

static EggRegexCompileFlags
		  update_regex_flags	       (EggRegexCompileFlags flags,
						gchar *option_name,
						gboolean value);

static gboolean   create_definition            (ParserState *parser_state,
                                                gchar *id,
                                                gchar *parent_id,
                                                gchar *style,
						GError **error);

static void       handle_context_element       (ParserState *parser_state,
                                                GError **error);
static void       handle_language_element      (ParserState *parser_state,
                                                GError **error);
static void       handle_define_regex_element  (ParserState *parser_state,
                                                GError **error);
static void       handle_default_regex_options_element 
                                               (ParserState *parser_state,
                                                GError **error);
static void       element_start                (ParserState *parser_state,
						GError **error);
static void       element_end                  (ParserState *parser_state,
						GError **error);
static gboolean   replace_by_id                (const EggRegex *egg_regex, 
                                                const gchar *regex, 
                                                GString *expanded_regex, 
                                                gpointer data);
static gchar     *expand_regex                 (ParserState *parser_state,
                                                gchar *regex,
						EggRegexCompileFlags flags,
						gboolean do_expand_vars,
						gboolean insert_parentheses,
						GError **error);

GQuark
parser_error_quark (void)
{
	static GQuark err_q = 0;
	if (err_q == 0)
		err_q = g_quark_from_static_string (
			"parser-error-quark");

	return err_q;
}

static gboolean
str_to_bool (gchar *string)
{
	g_assert (string);
	if (g_ascii_strcasecmp ("true", string) == 0)
		return TRUE;
	else
		return FALSE;
}

static gchar *
generate_new_id (ParserState *parser_state)
{
	gchar *id;

	id = g_strdup_printf ("unnamed-%u", parser_state->id_cookie);
	parser_state->id_cookie++;

	DEBUG (g_message ("generated id %s", id));

	return id;
}

static gboolean
id_is_decorated (gchar *id, gchar **lang_id)
{
	/* This function is quite simple because the XML validator check for
	 * the correctness of the id with a regex */

	gchar **tokens;
	gboolean is_decorated;

	tokens = g_strsplit (id, ":", 2);

	g_return_val_if_fail (tokens != NULL, FALSE);

	if (tokens [1] == NULL)
	{
		/* There is no ":" in the id */
		is_decorated = FALSE;
	}
	else if (tokens [1] != NULL && strcmp ("*", tokens[1]) == 0)
	{
		/* This is an undecorated "import all", and not a decorated
		 * reference */
		is_decorated = FALSE;
	}
	else
	{
		is_decorated = TRUE;
		if (lang_id != NULL)
		{
			*lang_id = g_strdup (tokens[0]);
		}
	}

	g_strfreev (tokens);

	return is_decorated;
}

static gchar *
decorate_id (ParserState *parser_state,
		gchar *id)
{
	gchar *decorated_id;

	decorated_id = g_strdup_printf ("%s:%s", 
			parser_state->current_lang_id, id);

	DEBUG (g_message ("decorated '%s' to '%s'", id, decorated_id));

	return decorated_id;
}

static gboolean
lang_id_is_already_loaded (ParserState *parser_state, gchar *lang_id)
{
	GSList *l;
	gchar *loaded_lang;

	l = *(parser_state->loaded_lang_ids);

	g_return_val_if_fail (lang_id != NULL, FALSE);

	while (l != NULL)
	{
		loaded_lang = l->data;
		DEBUG (g_message ("language '%s' is loaded", loaded_lang));
		if (strcmp (loaded_lang, lang_id) == 0)
		{
			DEBUG (g_message ("language '%s' already loaded", lang_id));
			return TRUE;
		}
		l = g_slist_next (l);
	}
	return FALSE;
}


static gboolean
create_definition (ParserState *parser_state, 
		gchar *id,
		gchar *parent_id,
		gchar *style,
		GError **error)
{
	gchar *match = NULL, *start = NULL, *end = NULL;
	gchar *prefix = NULL, *suffix = NULL;
	gchar *tmp;
	gboolean extend_parent = TRUE;
	gboolean end_at_line_end = FALSE;

	xmlNode *context_node, *child;

	GString *all_items;
	gchar *item;

	EggRegexCompileFlags flags, match_flags = 0, start_flags = 0, end_flags = 0;

	GError *tmp_error = NULL;

	g_assert (parser_state->engine != NULL);

	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* extend-parent */
	tmp = xmlTextReaderGetAttribute (parser_state->reader, "extend-parent");
	if (tmp)
		extend_parent = str_to_bool (tmp);
	xmlFree (tmp);
	
	/* end-at-line-end */
	tmp = xmlTextReaderGetAttribute (parser_state->reader, "end-at-line-end");
	if (tmp)
		end_at_line_end = str_to_bool (tmp);
	xmlFree (tmp);


	DEBUG (g_message ("creating context %s, child of %s", id, parent_id));

	/* Fetch the content of the sublements using the tree API on
	 * the current node */
	context_node = xmlTextReaderExpand (parser_state->reader);

	/* The file should be validated so this should not happen */
	g_assert (context_node != NULL);

	child = context_node->children;
	while (child != NULL)
	{
		/* FIXME: add PCRE_EXTRA support in EggRegex */
		flags = parser_state->regex_compile_flags;
		
		xmlAttr *attribute;
		for (attribute = child->properties; 
				attribute != NULL;
				attribute = attribute->next)
		{
			g_assert (attribute->children);
			tmp = attribute->children->content;
			if (strcmp ("extended", attribute->name) == 0)
			{
				flags = update_regex_flags (flags,
					"extended", str_to_bool (tmp));
			}
			else if (strcmp ("case-insensitive", attribute->name) == 0)
			{
				flags = update_regex_flags (flags,
					"case-insensitive", str_to_bool (tmp));
			}
			else if (strcmp ("dot-match-all", attribute->name) == 0)
			{
				flags = update_regex_flags (flags,
					"dot-match-all", str_to_bool (tmp));
			}
		}
		/*flags = update_regex_flags (parser_state->regex_compile_flags,
				"extended", TRUE);*/

		g_assert (child->name);
		if (strcmp ("match", child->name) == 0 
				&& child->children)
		{
			/* <match> */
			match = g_strdup (child->children->content);
			match_flags = flags;
		}
		else if (strcmp ("start", child->name) == 0)
		{	
			/* <start> */
			if (child->children)
			{
				start = g_strdup (child->children->content);
			}
			else
			{
				/* If the <start> element is present but 
				 * has no content use an empty string */
				start = g_strdup ("");
			}
			start_flags = flags;
		}
		else if (strcmp ("end", child->name) == 0)
		{	
			/* <end> */
			if (child->children)
				end = g_strdup (child->children->content);
			else
			{
				/* If the <end> element is present but 
				 * has no content use an empty string */
				end = g_strdup ("");
			}
			end_flags = flags;
		}
		else if (strcmp ("prefix", child->name) == 0)
		{
			/* <prefix> */
			if (child->children != NULL)
				prefix = g_strdup (child->children->content);
			else
				prefix = g_strdup ("");
			g_strstrip (prefix);
		}
		else if (strcmp ("suffix", child->name) == 0)
		{
			/* <suffix> */
			if (child->children != NULL)
				suffix = g_strdup (child->children->content);
			else
				suffix = g_strdup ("");
			g_strstrip (suffix);
		}
		else if ((strcmp ("keyword", child->name) == 0) 
				&& child->children)
		{
			/* <keyword> */
			all_items = g_string_new (NULL);
			if (prefix != NULL)
			{
				g_string_append (all_items, prefix);
			}
			else
			{
				g_string_append (all_items, 
						parser_state->opening_delimiter);
			}
			g_string_append (all_items, "(");

			/* Read every keyword/symbol concatenating them
			 * in a single string */
			while (TRUE)
			{
				/* TODO: this could be done destructively, 
				 * modifing the string in place without the 
				 * copy */
				item = g_strdup (child->children->content);
				g_strstrip (item);

				g_string_append (all_items, item);
				g_free (item);

				child = child->next;

				/* Skip text nodes */
				if (child != NULL
						&& strcmp ("text", child->name) == 0)
					child = child->next;

				/* These are the conditions which control
				 * the loop */
				if (child == NULL)
					break;
				if (strcmp ("keyword",	child->name) != 0)
					break;
				if (child->children == NULL)
					break;

				/* If we get here the next node will be added 
				 * to the keywords/symbols list, so we add the 
				 * separator */
				g_string_append (all_items, "|");
			} 

			g_string_append (all_items, ")");
			if (suffix != NULL)
			{
				g_string_append (all_items, suffix);
			}
			else
			{
				g_string_append (all_items, 
						parser_state->closing_delimiter);
			}

			match = g_string_free (all_items, FALSE);
			match_flags = flags;
		}

		if (child != NULL)
			child = child->next;
	}

	DEBUG (g_message ("start: '%s'", start));
	DEBUG (g_message ("end: '%s'", end));
	DEBUG (g_message ("match: '%s'", match));


	if (tmp_error == NULL && start != NULL)
	{
		g_strstrip (start);
		tmp = start;
		start = expand_regex (parser_state, start, start_flags, 
				TRUE, FALSE, &tmp_error);
		g_free (tmp);
	}
	if (tmp_error == NULL && end != NULL)
	{
		g_strstrip (end);
		tmp = end;
		end = expand_regex (parser_state, end, end_flags,
				TRUE, FALSE, &tmp_error);
		g_free (tmp);
	}
	if (tmp_error == NULL && match != NULL)
	{
		g_strstrip (match);
		tmp = match;
		match = expand_regex (parser_state, match, match_flags,
				TRUE, FALSE, &tmp_error);
		g_free (tmp);
	}

	if (tmp_error == NULL)
	{
		gtk_source_context_engine_define_context (
				parser_state->engine, id, parent_id,
				match, start, end, style, 
				extend_parent, end_at_line_end,
				&tmp_error);
	}

	xmlFree (match);
	xmlFree (start);
	xmlFree (end);
	xmlFree (prefix);
	xmlFree (suffix);
	
	if (tmp_error != NULL)
	{
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	return TRUE;
}

static gboolean
add_ref (ParserState *parser_state, gchar *ref, GError **error)
{
	gchar *container_id;
	gboolean all = FALSE;
	gchar *ref_id;
	gchar *lang_id;

	GtkSourceLanguagesManager *lm;
	GtkSourceLanguage *imported_language;

	GError *tmp_error = NULL;

	/* Return if an error is already set */
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (id_is_decorated (ref, &lang_id))
	{
		if (!lang_id_is_already_loaded (parser_state, lang_id))
		{
			lm = gtk_source_language_get_languages_manager (
					parser_state->language);
			imported_language = gtk_source_languages_manager_get_language_from_id (
					lm, lang_id);
			
			if (imported_language == NULL)
			{
				g_set_error (&tmp_error,
						PARSER_ERROR,
						PARSER_ERROR_WRONG_ID,
						"unable to resolve language '%s' in ref '%s'", 
						lang_id, ref);
			}
			else
			{
				file_parse (imported_language->priv->lang_file_name,
						parser_state->language,
						parser_state->tags,	
						parser_state->engine,
						parser_state->populate_styles_table,
						parser_state->defined_regexes,
						parser_state->styles_mapping,
						parser_state->loaded_lang_ids,
						&tmp_error);
			}
		}
		ref_id = g_strdup (ref);
	}
	else
	{
		ref_id = decorate_id (parser_state, ref);
	}

	if (tmp_error == NULL && parser_state->engine != NULL)
	{
		if (g_str_has_suffix (ref, ":*"))
		{
			all = TRUE;
			ref_id [strlen (ref_id) - 2] = '\0';
		}
		
		container_id = g_queue_peek_head (parser_state->curr_parents);

		/* If the document is validated container_id is never NULL */
		g_assert (container_id);

		gtk_source_context_engine_add_ref (parser_state->engine, 
				container_id, ref_id, all, &tmp_error);

		DEBUG (g_message ("appended %s in %s", ref_id, container_id));
	}

	g_free (ref_id);

	if (tmp_error != NULL)
	{
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	return TRUE;
}

static gboolean
create_sub_pattern (ParserState *parser_state,
                    gchar *id,
                    gchar *sub_pattern,
                    gchar *style,
                    GError **error)
{
	gchar *container_id;
	gchar *where;

	GError *tmp_error = NULL;

	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	container_id = g_queue_peek_head (parser_state->curr_parents);

	/* If the document is validated container is never NULL */
	g_assert (container_id);

	where = xmlTextReaderGetAttribute (parser_state->reader, "where");

	gtk_source_context_engine_add_sub_pattern (parser_state->engine,
			id, container_id, sub_pattern, where, style, 
			&tmp_error);

	g_free (where);

	if (tmp_error != NULL)
	{
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	return TRUE;
}

static void
handle_context_element (ParserState *parser_state,
			GError **error)
{
	gchar *id, *tmp, *parent_id, *style_ref;
	gchar *ref, *sub_pattern;
	int is_empty;
	gboolean success;

	GError *tmp_error = NULL;

	/* Return if an error is already set */
	g_return_if_fail (error == NULL || *error == NULL);

	ref = xmlTextReaderGetAttribute (parser_state->reader, "ref");
	sub_pattern = xmlTextReaderGetAttribute (parser_state->reader, 
			"sub-pattern");

	tmp = xmlTextReaderGetAttribute (parser_state->reader, 
			"style-ref");

	if (tmp == NULL || id_is_decorated (tmp, NULL))
	{
		style_ref = tmp;
	}
	else
	{
		style_ref = decorate_id (parser_state, tmp);
		g_free (tmp);
	}

	if (style_ref != NULL &&
			g_hash_table_lookup (parser_state->styles_mapping,
				style_ref) == NULL)
	{
		g_warning ("style '%s' not defined", style_ref);
	}
	
	if (ref != NULL)
	{
		add_ref (parser_state, ref, &tmp_error);
	}
	else
	{
		tmp = xmlTextReaderGetAttribute (parser_state->reader, "id");
		if (tmp == NULL)
			tmp = generate_new_id (parser_state);

		if (id_is_decorated (tmp, NULL))
			id = g_strdup (tmp);
		else
			id = decorate_id (parser_state, tmp);
	
		g_free (tmp);

		if (parser_state->engine != NULL)
		{
			if (sub_pattern != NULL)
			{
				create_sub_pattern (parser_state, id, 
						sub_pattern, style_ref,
						&tmp_error);
			}
			else
			{
				parent_id = g_queue_peek_head (
						parser_state->curr_parents);

				is_empty = xmlTextReaderIsEmptyElement (
						parser_state->reader);

				success = create_definition (parser_state, id, parent_id, 
						style_ref, &tmp_error);

				if (success && !is_empty)
				{
					/* Push the new context in the curr_parents 
					 * stack only if other contexts can be 
					 * defined inside it */
					g_queue_push_head (parser_state->curr_parents, 
							g_strdup (id));
				}
			}
		}

		g_free (id);
	}


	g_free (style_ref);
	g_free (sub_pattern);
	g_free (ref);

	if (tmp_error != NULL)
	{
		g_propagate_error (error, tmp_error);
		return;
	}
}

static void
handle_language_element (ParserState *parser_state,
			 GError **error)
{
	gchar *expected_version = "2.0";

	/* Return if an error is already set */
	g_return_if_fail (error == NULL || *error == NULL);

	/* FIXME: check that the language name, version, etc. are the 
	 * right ones - Paolo */
	xmlChar *lang_id, *lang_version;

	lang_version = xmlTextReaderGetAttribute (parser_state->reader, "version");

	if (lang_version == NULL || 
			(strcmp (expected_version, lang_version) != 0))
	{
		g_set_error (error,
				PARSER_ERROR,
				PARSER_ERROR_WRONG_VERSION,
				"wrong language version '%s', expected '%s'", 
				lang_version, expected_version);
	}

	lang_id = xmlTextReaderGetAttribute (parser_state->reader, "id");

	parser_state->current_lang_id = g_strdup (lang_id);

	*(parser_state->loaded_lang_ids) = g_slist_prepend (
			*(parser_state->loaded_lang_ids), lang_id);


	xmlFree (lang_version);
}


static gboolean
replace_by_id (const EggRegex *egg_regex, 
	       const gchar *regex, 
	       GString *expanded_regex, 
	       gpointer data)
{
	gchar *id, *subst, *escapes;
	gchar *tmp;
	GError *tmp_error = NULL;
	ParserState *parser_state = data;

	escapes = egg_regex_fetch (egg_regex, regex, 1);
	tmp = egg_regex_fetch (egg_regex, regex, 2);

	g_strstrip (tmp);

	if (id_is_decorated (tmp, NULL))
		id = g_strdup (tmp);
	else
		id = decorate_id (parser_state, tmp);
	g_free (tmp);

	subst = g_hash_table_lookup (parser_state->defined_regexes, id);
	if (subst == NULL)
	{
		g_set_error (&tmp_error,
				PARSER_ERROR, PARSER_ERROR_WRONG_ID,
				_ ("wrong id '%s' in regex '%s'"), id, regex);

	}

	if (tmp_error == NULL)
	{
		g_string_append (expanded_regex, escapes);
		g_string_append (expanded_regex, subst);
	}
	
	g_free (escapes);
	g_free (id);

	if (tmp_error != NULL)
	{
		g_propagate_error (parser_state->error, tmp_error);
		return TRUE;
	}

	return FALSE;
}

static EggRegexCompileFlags
update_regex_flags (EggRegexCompileFlags flags, gchar *option_name, gboolean value)
{
	EggRegexCompileFlags single_flag;

	DEBUG (g_message ("setting the '%s' regex flag to %d", option_name, value));

	if (strcmp ("case-insensitive", option_name) == 0)
	{
		single_flag = EGG_REGEX_CASELESS;
	}
	else if (strcmp ("extended", option_name) == 0)
	{
		single_flag = EGG_REGEX_EXTENDED;
	}
	else if (strcmp ("dot-match-all", option_name) == 0)
	{
		single_flag = EGG_REGEX_DOTALL;
	}
	else
		return flags;

	if (value)
		flags |= single_flag;
	else
		flags &= ~single_flag;

	return flags;
}

static gboolean
calc_regex_flags (const gchar *flags_string, EggRegexCompileFlags *flags)
{
	gboolean add = TRUE;
	while (*flags_string != '\0')
	{
		EggRegexCompileFlags single_flag = 0;
		switch (g_utf8_get_char (flags_string))
		{
			case '-':
				add = FALSE;
				break;
			case '+':
				add = TRUE;
				break;
			case 'i':
				single_flag = EGG_REGEX_CASELESS;
				break;
			case 'x':
				single_flag = EGG_REGEX_EXTENDED;
				break;
			case 's':
				/* FIXME: do we need this? */
				single_flag = EGG_REGEX_DOTALL;
				break;
			default:
				return FALSE;
		}
		if (single_flag != 0)
		{
			if (add)
				*flags |= single_flag;
			else
				*flags &= ~single_flag;
		}
		flags_string = g_utf8_next_char (flags_string);
	}

	return TRUE;
}

static gchar *
expand_regex_vars (ParserState *parser_state, gchar *regex, gint len, GError **error)
{
	/* This is the commented regex without the doubled escape needed
	 * in a C string:
	 * 
	 * (?<!\\)(\\\\)*\\%\{([^@]*?)\}
	 * |------------||---||------||-|
	 *      |          |      |    |
	 *      |    the string   |   the ending
	 *      |       "\%{"     |   bracket "}"
	 *      |                 |
	 * a even sequence        |
	 * of escapes or      the id of the
	 * a char that        included regex
	 * is not an          (not greedy)
	 * escape (this
	 * is not matched)
	 *
	 * The first block is needed to ensure that the sequence is
	 * not escaped.
	 * Matches with an id containing a "@" sign are ignored and
	 * passed to the engine because they are references to subpatterns
	 * in a different regex (i.e. in the start regex while we are in the
	 * end regex.)
	 * The sub pattern containing the id is the second.
	 */
	gchar *re = "(?<!\\\\)(\\\\\\\\)*\\\\%\\{([^@]*?)\\}";
	gchar *expanded_regex;
	EggRegex *egg_re;

	if (regex == NULL)
		return NULL;

	egg_re = egg_regex_new (re, 0, 0, NULL);

	/* Use parser_state to pass the GError because we cannot pass
	 * it directly to the callback */
	parser_state->error = error;
	expanded_regex = egg_regex_replace_eval (egg_re, regex, len, 0,
			replace_by_id, parser_state,
			0);
	parser_state->error = NULL;

	if (*error == NULL)
		DEBUG (g_message ("expanded regex vars '%s' to '%s'", 
					regex, expanded_regex));
	
	egg_regex_free (egg_re);
	
	if (*error != NULL)
	{
		g_free (expanded_regex);
		return NULL;
	}
	
	return expanded_regex;
}

static gboolean
replace_delimiter (const EggRegex *egg_regex, 
	       const gchar *regex, 
	       GString *expanded_regex, 
	       gpointer data)
{
	gchar *delim, *escapes;
	ParserState *parser_state = data;

	escapes = egg_regex_fetch (egg_regex, regex, 1);
	g_string_append (expanded_regex, escapes);
	g_free (escapes);

	delim = egg_regex_fetch (egg_regex, regex, 2);
	DEBUG (g_message ("replacing '\\%%%s'", delim));

	switch (delim[0])
	{
		case '[':
			g_string_append (expanded_regex, 
					parser_state->opening_delimiter);
			break;
		case ']':
			g_string_append (expanded_regex, 
					parser_state->closing_delimiter);
			break;
	}

	return FALSE;
}

static gchar *
expand_regex_delimiters (ParserState *parser_state, 
		gchar *regex, 
		gint len)
{
	/* This is the commented regex without the doubled escape needed
	 * in a C string:
	 * 
	 * (?<!\\)(\\\\)*\\%([\[|\])
	 * |------------||---------|
	 *      |             |
	 *      |        the strings
	 *      |        "\%[" or "\%]"
	 *      |
	 * a even sequence of escapes or
	 * a char that is not an escape 
	 * (this is not matched)
	 *
	 * The first block is needed to ensure that the sequence is
	 * not escaped.
	 */
	gchar *re = "(?<!\\\\)(\\\\\\\\)*\\\\%(\\[|\\])";
	gchar *expanded_regex;
	EggRegex *egg_re;

	if (regex == NULL)
		return NULL;

	egg_re = egg_regex_new (re, 0, 0, NULL);

	expanded_regex = egg_regex_replace_eval (egg_re, regex, len, 0,
			replace_delimiter, parser_state,
			0);

	DEBUG (g_message ("expanded regex delims '%s' to '%s'", 
				regex, expanded_regex));

	egg_regex_free (egg_re);
	
	return expanded_regex;
}

static gchar *
expand_regex (ParserState *parser_state,
	      gchar *regex,
	      EggRegexCompileFlags flags,
	      gboolean do_expand_vars,
	      gboolean insert_parentheses,
	      GError **error)
{
	gchar *tmp_regex;
	GString *expanded_regex;
	/* Length in *bytes* of the regex passed to expand_regex_vars. */
	gint len;

	g_assert (parser_state != NULL);

	if (regex == NULL)
		return NULL;

	if (g_utf8_get_char (regex) == '/')
	{
		gchar *regex_end = g_utf8_strrchr (regex, -1, '/');
		if (regex_end == regex)
		{
			g_set_error (error,
					PARSER_ERROR, PARSER_ERROR_MALFORMED_REGEX,
					_ ("malformed regex '%s'"), regex);
			return NULL;
		}
		regex = g_utf8_next_char (regex);
		len = regex_end - regex;
	}
	else
	{
		len = -1;
	}

	if (do_expand_vars)
	{
		tmp_regex = expand_regex_vars (parser_state, regex, len, error);
	}
	else
	{
		if (len == -1)
			len = strlen (regex);
		tmp_regex = g_strndup (regex, len);
	}

	regex = tmp_regex;
	tmp_regex = expand_regex_delimiters (parser_state, regex, len);
	g_free (regex);

	/* Set the options and add not capturing parentheses if
	 * insert_parentheses is TRUE (this is needed for included
	 * regular expressions.) */
	expanded_regex = g_string_new ("");
	if (insert_parentheses)
		g_string_append (expanded_regex, "(?:");
	g_string_append (expanded_regex, "(?");
	if (flags != 0)
	{
		if (flags & EGG_REGEX_CASELESS)
			g_string_append (expanded_regex, "i");
		if (flags & EGG_REGEX_EXTENDED)
			g_string_append (expanded_regex, "x");
		if (flags & EGG_REGEX_DOTALL)
			g_string_append (expanded_regex, "s");
	}
	if (flags != (EGG_REGEX_CASELESS | EGG_REGEX_EXTENDED |
		EGG_REGEX_DOTALL))
	{
		g_string_append (expanded_regex, "-");
		if (!(flags & EGG_REGEX_CASELESS))
			g_string_append (expanded_regex, "i");
		if (!(flags & EGG_REGEX_EXTENDED))
			g_string_append (expanded_regex, "x");
		if (!(flags & EGG_REGEX_DOTALL))
			g_string_append (expanded_regex, "s");
	}
	g_string_append (expanded_regex, ")");
	g_string_append (expanded_regex, tmp_regex);
	if (insert_parentheses)
	{
		/* The '\n' is needed otherwise, if the regex is "extended" 
		 * and it ends with a comment, the ')' is appended inside the
		 * comment itself */
		if (flags & EGG_REGEX_EXTENDED)
		{
			g_string_append (expanded_regex, "\n");
		}
		/* FIXME: if the regex is not exteded this doesn't works */
		g_string_append (expanded_regex, ")");
	}
	g_free (tmp_regex);

	return g_string_free (expanded_regex, FALSE);
}

static void
handle_define_regex_element (ParserState *parser_state,
			     GError **error)
{
	gchar *id, *regex;
	gchar *tmp;
	gchar *expanded_regex;
	int i;
	gchar *regex_options[] = {"extended", "case-insensitive", "dot-match-all", NULL};
	EggRegexCompileFlags flags;
	GError *tmp_error = NULL;

	int type;

	/* Return if an error is already set */
	g_return_if_fail (error == NULL || *error == NULL);

	if (parser_state->engine == NULL)
		return;

	tmp = xmlTextReaderGetAttribute (parser_state->reader, "id");

	/* If the file is validated <define-regex> must have an id
	 * attribute */
	g_assert (tmp != NULL);

	if (id_is_decorated (tmp, NULL))
		id = g_strdup (tmp);
	else
		id = decorate_id (parser_state, tmp);
	g_free (tmp);
	
	flags = parser_state->regex_compile_flags;

	for (i=0; regex_options[i] != NULL; i++)
	{
		tmp = xmlTextReaderGetAttribute (parser_state->reader,
				regex_options[i]);
		if (tmp != NULL)
		{
			flags = update_regex_flags (flags, regex_options[i], 
					str_to_bool (tmp));
		}
		g_free (tmp);
	}


	xmlTextReaderRead (parser_state->reader);

	type = xmlTextReaderNodeType (parser_state->reader);

	if (type == XML_READER_TYPE_TEXT || 
		 type == XML_READER_TYPE_CDATA)
	{
		regex = xmlTextReaderValue (parser_state->reader);
	}
	else
	{
		regex = xmlStrdup ("");
	}

	g_strstrip (regex);
	expanded_regex = expand_regex (parser_state, regex, flags,
			FALSE, TRUE, &tmp_error);

	if (tmp_error == NULL)
	{
		DEBUG (g_message ("defined regex %s: \"%s\"", id, regex));

		g_hash_table_insert (parser_state->defined_regexes, id, expanded_regex);
	}

	g_free (regex);

	if (tmp_error != NULL)
	{
		g_propagate_error (error, tmp_error);
		return;
	}
}

static void
handle_default_regex_options_element (ParserState *parser_state,
			     GError **error)
{
	gchar *options;
	int ret, type;

	/* Return if an error is already set */
	g_return_if_fail (error == NULL || *error == NULL);

	if (!parser_state->engine)
		return;

	do {
		ret = xmlTextReaderRead (parser_state->reader);
		g_assert (ret == 1);

		type = xmlTextReaderNodeType (parser_state->reader);

	} while (type != XML_READER_TYPE_TEXT && 
		 type != XML_READER_TYPE_CDATA);

	options = xmlTextReaderValue (parser_state->reader);

	g_strstrip (options);

	if (!calc_regex_flags (options, &(parser_state->regex_compile_flags)))
	{
		g_set_error (error,
				PARSER_ERROR,
				PARSER_ERROR_MALFORMED_REGEX,
				_ ("malformed regex options '%s'"), options);
	}

	g_free (options);
}

static void
populate_styles (ParserState *parser_state, gchar *style_id,
		GError **error)
{
	GHashTable *ht;
	GError *tmp_error = NULL;

	ht = parser_state->language->priv->tag_id_to_style_name;

	DEBUG (g_message ("associating '%s' to '%s'", style_id, style_id));

	if (g_hash_table_lookup (ht, style_id) != NULL)
	{
		g_set_error (&tmp_error, 
				PARSER_ERROR,
				PARSER_ERROR_WRONG_ID,
				"duplicated style id '%s'",
				style_id);
	}
	else
	{
		g_hash_table_insert (ht, g_strdup (style_id), 
				g_strdup (style_id));
	}
	
	if (tmp_error != NULL)
	{
		g_propagate_error (error, tmp_error);
		return;
	}
}

static gchar *
map_style (ParserState *parser_state, 
		gchar *style_id, 
		gchar *map_to,
		GError **error)
{
	gchar *mapped_style;

	if (map_to != NULL)
	{
		mapped_style = g_hash_table_lookup (parser_state->styles_mapping, 
				map_to);
	}
	else
	{
		mapped_style = style_id;
	}
	
	DEBUG (g_message ("mapping the style of '%s' to '%s' -> '%s'",
				style_id, map_to, mapped_style));

	if (mapped_style != NULL)
	{
		g_hash_table_insert (parser_state->styles_mapping, 
				g_strdup (style_id), g_strdup (mapped_style));
	}
	else
	{
		g_set_error (error,
				PARSER_ERROR,
				PARSER_ERROR_WRONG_ID,
				"unable to map style '%s' to '%s'", 
				style_id, map_to);
		return NULL;
	}

	return g_strdup (mapped_style);
}



static GtkTextTag *
create_tag (ParserState *parser_state,
		gchar *id,
		gchar *name,
		gchar *default_style,
		GError **error)
{
	GtkTextTag *tag;
	GtkSourceTagStyle *ts = NULL;

	g_return_val_if_fail (parser_state != NULL, NULL);
	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	DEBUG (g_message ("new tag id '%s', name '%s'", id, name));

	tag = gtk_source_tag_new (name, id);

	DEBUG (g_message ("mapping the style of '%s' to '%s'", 
				id, default_style));

	if (default_style != NULL)
	{
		ts = gtk_source_language_get_tag_style (parser_state->language, 
				default_style);
	}

	if (ts != NULL)
	{
		gtk_source_tag_set_style (GTK_SOURCE_TAG (tag), ts);
		gtk_source_tag_style_free (ts);
	}
	
	return tag;
}

static void
parse_language_with_id (ParserState *parser_state, 
		gchar *lang_id, 
		GError **error)
{
	GtkSourceLanguagesManager *lm;
	GtkSourceLanguage *imported_language;

	GError *tmp_error = NULL;

	lm = gtk_source_language_get_languages_manager (
			parser_state->language);
	imported_language = gtk_source_languages_manager_get_language_from_id (
			lm, lang_id);

	if (imported_language == NULL)
	{
		g_set_error (&tmp_error,
				PARSER_ERROR,
				PARSER_ERROR_WRONG_ID,
				"unable to resolve language '%s'", 
				lang_id);
	}
	else
	{
		file_parse (imported_language->priv->lang_file_name,
				parser_state->language,
				parser_state->tags,
				parser_state->engine,
				parser_state->populate_styles_table,
				parser_state->defined_regexes,
				parser_state->styles_mapping,
				parser_state->loaded_lang_ids,
				&tmp_error);
	}
	
	if (tmp_error != NULL)
	{
		g_propagate_error (error, tmp_error);
		return;
	}
}

static void
parse_style (ParserState *parser_state,
		GError **error)
{
	gchar *id, *name, *map_to;
	gchar *mapped_style;

	gchar *tmp, *lang_id = NULL;

	GError *tmp_error = NULL;

	tmp = xmlTextReaderGetAttribute (parser_state->reader, 
			"id");

	if (id_is_decorated (tmp, NULL))
		id = g_strdup (tmp);
	else
		id = decorate_id (parser_state, tmp);

	g_free (tmp);

	name = xmlTextReaderGetAttribute (parser_state->reader, 
			"_name");
	if (name != NULL)
	{
		tmp = g_strdup (dgettext (
					parser_state->language->priv->translation_domain,
					name));
		g_free (name);
		name = tmp;
	}
	else
	{
		name = xmlTextReaderGetAttribute (parser_state->reader, 
				"name");
	}

	map_to = xmlTextReaderGetAttribute (parser_state->reader, 
			"map-to");

	if (map_to != NULL && 
			!id_is_decorated (map_to, &lang_id))
	{
		g_set_error (&tmp_error, 
				PARSER_ERROR,
				PARSER_ERROR_MALFORMED_MAP_TO,
				"the map-to attribute '%s' for the style '%s' lacks the prefix",
				map_to, id);
	}

	if (tmp_error == NULL &&
			parser_state->populate_styles_table)
	{
		populate_styles (parser_state, id, &tmp_error);
	}

	if (tmp_error == NULL &&
			lang_id != NULL && strcmp ("", lang_id) == 0)
	{
		g_free (lang_id);
		lang_id = NULL;
	}

	if (tmp_error == NULL && 
			lang_id != NULL && 
			!lang_id_is_already_loaded (parser_state, lang_id))
	{
		parse_language_with_id (parser_state, lang_id, &tmp_error);
	}

	DEBUG (g_message ("style %s (%s) to be mapped to '%s'", 
			name, id, map_to));

	mapped_style = map_style (parser_state, id, map_to, &tmp_error);

	if (tmp_error == NULL && 
			parser_state->tags != NULL)
	{
		GtkTextTag *tag;

		tag = create_tag (parser_state, id, name, mapped_style, NULL);

		if (tag != NULL)
		{
			*parser_state->tags = g_slist_prepend (*parser_state->tags, 
					tag);
		}
	}

	g_free (lang_id);
	g_free (id);
	g_free (name);
	g_free (map_to);
	g_free (mapped_style);
	
	if (tmp_error != NULL)
	{
		g_propagate_error (error, tmp_error);
		return;
	}
}

static void
handle_keyword_char_class_element (ParserState *parser_state,
			     GError **error)
{
	gchar *char_class;
	int ret, type;

	/* Return if an error is already set */
	g_return_if_fail (error == NULL || *error == NULL);

	if (!parser_state->engine)
		return;

	do {
		ret = xmlTextReaderRead (parser_state->reader);
		g_assert (ret == 1);

		type = xmlTextReaderNodeType (parser_state->reader);

	} while (type != XML_READER_TYPE_TEXT && 
		 type != XML_READER_TYPE_CDATA);

	char_class = xmlTextReaderValue (parser_state->reader);

	g_strstrip (char_class);

	g_free (parser_state->opening_delimiter);
	g_free (parser_state->closing_delimiter);

	parser_state->opening_delimiter = g_strdup_printf ("(?!<%s)(?=%s)", 
			char_class, char_class);
	parser_state->closing_delimiter = g_strdup_printf ("(?<=%s)(?!%s)", 
			char_class, char_class);

	g_free (char_class);
}

static void
handle_styles_element (ParserState *parser_state,
			     GError **error)
{
	int ret, type;
	const gchar *tag_name;

	GError *tmp_error = NULL;

	while (TRUE)
	{
		ret = xmlTextReaderRead (parser_state->reader);
		if (!xmlTextReaderIsValid (parser_state->reader))
		{
			/* TODO: get the error message from the 
			 * xml parser */
			g_set_error (&tmp_error, 
					PARSER_ERROR,
					PARSER_ERROR_INVALID_DOC,
					"invalid language file");
			break;
		}

		tag_name = xmlTextReaderConstName (parser_state->reader);
		type = xmlTextReaderNodeType  (parser_state->reader);

		/* End at the closing </styles> tag */
		if (tag_name != NULL && 
				type == XML_READER_TYPE_END_ELEMENT &&
				(strcmp ("styles", tag_name) == 0))
			break;

		/* Skip nodes that aren't <style> elements */
		if (tag_name == NULL || 
				(strcmp ("style", tag_name) != 0))
			continue;

		/* Handle <style> elements */
		parse_style (parser_state, &tmp_error);

		if (tmp_error != NULL)
			break;
	}
	
	if (tmp_error != NULL)
	{
		g_propagate_error (error, tmp_error);
		return;
	}
}


static void
element_start (ParserState *parser_state, GError **error)
{
	const xmlChar *name;
	GError *tmp_error = NULL;
	
	/* TODO: check the namespace and ignore everithing is not in our namespace */
	name = xmlTextReaderConstName (parser_state->reader);

	if (strcmp ("context", name) == 0)
	{
		handle_context_element (parser_state,
				&tmp_error);
	}
	else if (strcmp ("define-regex", name) == 0)
	{
		handle_define_regex_element (parser_state,
				&tmp_error);
	}
	else if (strcmp ("language", name) == 0)
	{
		handle_language_element (parser_state,
				&tmp_error);
	}
	else if (strcmp ("styles", name) == 0)
	{
		handle_styles_element (parser_state,
				&tmp_error);
	}
	else if (strcmp ("keyword-char-class", name) == 0)
	{
		handle_keyword_char_class_element (parser_state,
				&tmp_error);
	}
	else if (strcmp ("default-regex-options", name) == 0)
	{
		handle_default_regex_options_element (parser_state,
				&tmp_error);
	}
	
	if (tmp_error != NULL)
	{
		g_propagate_error (error, tmp_error);
		return;
	}
}

static void
element_end (ParserState *parser_state, GError **error)
{
	const xmlChar *name;
	gchar *popped_id;

	name = xmlTextReaderConstName (parser_state->reader);
	
	if (strcmp (name, "context") == 0) 
	{
		/* pop the first element in the curr_parents list */
		popped_id = g_queue_pop_head (parser_state->curr_parents);
		g_free (popped_id);
	}
}

static gboolean
file_parse (gchar                 *filename,
	    GtkSourceLanguage         *language,
	    GSList                   **tags,
	    GtkSourceContextEngine    *engine,
	    gboolean                   populate_styles_table,
	    GHashTable                *defined_regexes,
	    GHashTable                *styles,
	    GSList                   **loaded_lang_ids,
	    GError                   **error)
{
	gchar *rng_lang_schema = RNG_SCHEMA_DIR;

	ParserState *parser_state;
	xmlTextReader *reader;
	int ret;

	GError *tmp_error = NULL;

	DEBUG (g_message ("loading file '%s'", 	filename));
	
	reader = xmlNewTextReaderFilename (filename);

	if (reader == NULL)
	{
		g_set_error (&tmp_error,
				PARSER_ERROR,
				PARSER_ERROR_CANNOT_OPEN,
				"unable to open the file");
	}
	
	/* Validate using a RelaxNG schema */
	if (tmp_error == NULL &&
			xmlTextReaderRelaxNGValidate (reader, 
				rng_lang_schema))
	{
		g_set_error (&tmp_error,
				PARSER_ERROR,
				PARSER_ERROR_CANNOT_VALIDATE,
				"unable to load the RelaxNG schema '%s'", 
				rng_lang_schema);
	}
	
	parser_state = parser_state_new (language, tags, engine, 
			populate_styles_table, defined_regexes, styles,
			reader, loaded_lang_ids);

	if (tmp_error == NULL)
	{
		ret = xmlTextReaderRead (parser_state->reader);
		while (ret == 1)
		{
			if (!xmlTextReaderIsValid (parser_state->reader))
			{
				/* TODO: get the error message from the 
				 * xml parser */
				g_set_error (&tmp_error, 
						PARSER_ERROR,
						PARSER_ERROR_INVALID_DOC,
						"invalid language file");
			}

			if (tmp_error != NULL)
				break;


			int type = xmlTextReaderNodeType (parser_state->reader);
			switch (type)
			{
				case XML_READER_TYPE_ELEMENT: 
					element_start (parser_state, &tmp_error);
					break;
				case XML_READER_TYPE_END_ELEMENT:
					element_end (parser_state, &tmp_error);
					break;
			}

			if (tmp_error != NULL)
				break;

			ret = xmlTextReaderRead (parser_state->reader);
		}

	}

	if (tmp_error == NULL && parser_state->tags != NULL)
	{
		*parser_state->tags = g_slist_reverse (
				*(parser_state->tags));
	}

	parser_state_destroy (parser_state);

	if (tmp_error != NULL)
	{
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	return TRUE;
}

static ParserState *
parser_state_new (GtkSourceLanguage       *language,
		  GSList                 **tags,
		  GtkSourceContextEngine  *engine,
		  gboolean                 populate_styles_table,
		  GHashTable              *defined_regexes,
		  GHashTable              *styles_mapping,
		  xmlTextReader	          *reader,
		  GSList                 **loaded_lang_ids)
{
	ParserState *parser_state;
	parser_state = g_new (ParserState, 1);

	parser_state->language = language;
	parser_state->engine = engine;
	parser_state->tags = tags;
	parser_state->populate_styles_table = populate_styles_table;

	parser_state->current_lang_id = NULL;

	parser_state->id_cookie = 0;
	parser_state->regex_compile_flags = 0;

	parser_state->reader = reader;
		
	parser_state->defined_regexes = defined_regexes;
	parser_state->styles_mapping = styles_mapping;

	parser_state->loaded_lang_ids = loaded_lang_ids;

	parser_state->curr_parents = g_queue_new ();

	parser_state->opening_delimiter = g_strdup ("\\b");
	parser_state->closing_delimiter = g_strdup ("\\b");

	return parser_state;
}

static void
parser_state_destroy (ParserState *parser_state)
{
	if (parser_state->reader != NULL)
		xmlFreeTextReader (parser_state->reader);

	g_queue_free (parser_state->curr_parents);
	g_free (parser_state->current_lang_id);
	
	g_free (parser_state->opening_delimiter);
	g_free (parser_state->closing_delimiter);

	g_free (parser_state);
}



gboolean 
_gtk_source_language_file_parse_version2 (GtkSourceLanguage       *language,
					  GSList                 **tags,
					  GtkSourceContextEngine  *engine,
					  gboolean                 populate_styles_table)
{
	GHashTable *defined_regexes, *styles;
	gboolean success;
	GError *error = NULL;
	gchar *filename;
	GSList *loaded_lang_ids = NULL;
	GSList *l;

	if (!tags && !engine)
		return FALSE;

	filename = language->priv->lang_file_name;

	/* TODO: tell the parser to validate the document while parsing
	 * (XML_PARSE_DTD_VALID or XML_PARSER_VALIDATE). */
	/* TODO: as an optimization tell the parser to merge CDATA 
	 * as text nodes (XML_PARSE_NOCDATA), and to ignore blank 
	 * nodes (XML_PARSE_NOBLANKS), if it is possible with 
	 * xmlTextReader. */
	xmlKeepBlanksDefault (0);
	xmlLineNumbersDefault (1);
	xmlSubstituteEntitiesDefault (1);
	DEBUG (xmlPedanticParserDefault (1));

	defined_regexes = g_hash_table_new_full (g_str_hash, g_str_equal, 
			g_free, g_free);
	styles = g_hash_table_new_full (g_str_hash, g_str_equal, 
			g_free, g_free);

	success = file_parse (filename, language, tags, engine, populate_styles_table,
			defined_regexes, styles, &loaded_lang_ids, &error);

	for (l = loaded_lang_ids; l != NULL; l = l->next)
	{
		g_free (l->data);
	}
	g_slist_free (loaded_lang_ids);

	g_hash_table_destroy (defined_regexes);
	g_hash_table_destroy (styles);

	if (!success)
	{
		g_warning ("Failed to load '%s': %s", 
				filename, 
				error->message);
		g_error_free (error);

		if (tags != NULL)
		{
			for (l = *tags; l != NULL; l = l->next)
			{
				GtkSourceTag *tag = GTK_SOURCE_TAG (l->data);
				g_object_unref (tag);
			}
			g_slist_free (*tags);
			*tags = NULL;
		}

		return FALSE;
	}

	return TRUE;
}
