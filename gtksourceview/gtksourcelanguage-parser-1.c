/*  gtksourcelanguage-parser-ver1.c
 *  Language specification parser for 1.0 version .lang files
 *
 *  Copyright (C) 2003 - Paolo Maggi <paolo.maggi@polito.it>
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

#include <string.h>

#include <libxml/parser.h>
#include "gtksourceview-i18n.h"
#include "gtksourcebuffer.h"
#include "gtksourcetag.h"
#include "gtksourcelanguage.h"
#include "gtksourcelanguagesmanager.h"
#include "gtksourcelanguage-private.h"

static gchar * 
build_keyword_list (const gchar  *id,
		    const GSList *keywords,
		    gboolean      case_sensitive,
		    gboolean      match_empty_string_at_beginning,
		    gboolean      match_empty_string_at_end,
		    const gchar  *beginning_regex,
		    const gchar  *end_regex)
{
	GString *str;
	gint keyword_count;
	
	g_return_val_if_fail (keywords != NULL, NULL);

	str =  g_string_new ("");

	if (keywords != NULL)
	{
		if (match_empty_string_at_beginning)
			g_string_append (str, "\\b");

		if (beginning_regex != NULL)
			g_string_append (str, beginning_regex);

		if (case_sensitive)
			g_string_append (str, "(?:");
		else
			g_string_append (str, "(?i:");
	
		keyword_count = 0;
		/* Due to a bug in GNU libc regular expressions
		 * implementation we can't have keyword lists of more
		 * than 250 or so elements, so we truncate such a
		 * list.  This is a temporary solution, as the correct
		 * approach would be to generate multiple keyword
		 * lists.  (See bug #110991) */

#define KEYWORD_LIMIT 250

		while (keywords != NULL && keyword_count < KEYWORD_LIMIT)
		{
			g_string_append (str, (gchar*)keywords->data);
			
			keywords = g_slist_next (keywords);
			keyword_count++;
			
			if (keywords != NULL && keyword_count < KEYWORD_LIMIT)
				g_string_append (str, "|");
		}
		g_string_append (str, ")");
		
		if (keyword_count >= KEYWORD_LIMIT)
		{
			g_warning ("Keyword list '%s' too long. Only the first %d "
				   "elements will be highlighted. See bug #110991 for "
				   "further details.", id, KEYWORD_LIMIT);
		}

		if (end_regex != NULL)
			g_string_append (str, end_regex);
		
		if (match_empty_string_at_end)
			g_string_append (str, "\\b");
	}

	return g_string_free (str, FALSE);
}

static void 
parseLineComment (xmlNodePtr             cur,
		  gchar                 *id,
		  xmlChar               *style,
		  GtkSourceSimpleEngine *se)
{
	xmlNodePtr child;

	child = cur->xmlChildrenNode;
		
	if ((child != NULL) && !xmlStrcmp (child->name, (const xmlChar *)"start-regex"))
	{
		xmlChar *start_regex;
			
		start_regex = xmlNodeListGetString (child->doc, child->xmlChildrenNode, 1);
			
		gtk_source_simple_engine_add_syntax_pattern (se, id, style,
							     _gtk_source_language_strconvescape (start_regex), 
							     "\n");

		xmlFree (start_regex);
	}
	else
	{
		g_warning ("Missing start-regex in tag 'line-comment' (%s, line %ld)", 
			   child->doc->name, xmlGetLineNo (child));
	}
}

static void
parseBlockComment (xmlNodePtr             cur,
		   gchar                 *id,
		   xmlChar               *style,
		   GtkSourceSimpleEngine *se)
{
	xmlChar *start_regex = NULL;
	xmlChar *end_regex = NULL;

	xmlNodePtr child;

	child = cur->xmlChildrenNode;
	
	while (child != NULL)
	{	
		if (!xmlStrcmp (child->name, (const xmlChar *)"start-regex"))
		{
			start_regex = xmlNodeListGetString (child->doc, child->xmlChildrenNode, 1);
		}
		else
		if (!xmlStrcmp (child->name, (const xmlChar *)"end-regex"))
		{
			end_regex = xmlNodeListGetString (child->doc, child->xmlChildrenNode, 1);
		}

		child = child->next;
	}

	if (start_regex == NULL)
	{
		g_warning ("Missing start-regex in tag 'block-comment' (%s, line %ld)", 
			   child->doc->name, xmlGetLineNo (cur));

		return;
	}

	if (end_regex == NULL)
	{
		xmlFree (start_regex);

		g_warning ("Missing end-regex in tag 'block-comment' (%s, line %ld)", 
			   child->doc->name, xmlGetLineNo (cur));

		return;
	}

	gtk_source_simple_engine_add_syntax_pattern (se, id, style,
						     _gtk_source_language_strconvescape (start_regex), 
						     _gtk_source_language_strconvescape (end_regex));
	
	xmlFree (start_regex);
	xmlFree (end_regex);
}

static void
parseString (xmlNodePtr             cur,
	     gchar                 *id,
	     xmlChar               *style,
	     GtkSourceSimpleEngine *se)
{  
	xmlChar *start_regex = NULL;
	xmlChar *end_regex = NULL;

	xmlChar *prop = NULL;
	gboolean end_at_line_end = TRUE;

	xmlNodePtr child;

	prop = xmlGetProp (cur, "end-at-line-end");
	if (prop != NULL)
	{
		if (!xmlStrcasecmp (prop, (const xmlChar *)"TRUE") ||
		    !xmlStrcmp (prop, (const xmlChar *)"1"))

				end_at_line_end = TRUE;
			else
				end_at_line_end = FALSE;

		xmlFree (prop);	
	}
	
	child = cur->xmlChildrenNode;
	
	while (child != NULL)
	{	
		if (!xmlStrcmp (child->name, (const xmlChar *)"start-regex"))
		{
			start_regex = xmlNodeListGetString (child->doc, child->xmlChildrenNode, 1);
		}
		else
		if (!xmlStrcmp (child->name, (const xmlChar *)"end-regex"))
		{
			end_regex = xmlNodeListGetString (child->doc, child->xmlChildrenNode, 1);
		}
	
		child = child->next;
	}

	if (start_regex == NULL)
	{
		g_warning ("Missing start-regex in tag 'string' (%s, line %ld)", 
			   child->doc->name, xmlGetLineNo (cur));

		return;
	}

	if (end_regex == NULL)
	{
		xmlFree (start_regex);

		g_warning ("Missing end-regex in tag 'string' (%s, line %ld)", 
			   child->doc->name, xmlGetLineNo (cur));

		return;
	}

	_gtk_source_language_strconvescape (start_regex);
	_gtk_source_language_strconvescape (end_regex);
	
	if (!end_at_line_end)
		gtk_source_simple_engine_add_syntax_pattern (se, id, style,
							     start_regex,
							     end_regex);
	else
	{
		gchar *end;
		
		end = g_strdup_printf ("%s|\n", end_regex);

		gtk_source_simple_engine_add_syntax_pattern (se, id, style,
							     start_regex,
							     end);
		
		g_free (end);
	}

	xmlFree (start_regex);
	xmlFree (end_regex);
}

static void
parseKeywordList (xmlNodePtr             cur,
		  gchar                 *id,
		  xmlChar               *style,
		  GtkSourceSimpleEngine *se)
{
	gboolean case_sensitive = TRUE;
	gboolean match_empty_string_at_beginning = TRUE;
	gboolean match_empty_string_at_end = TRUE;
	gchar  *beginning_regex = NULL;
	gchar  *end_regex = NULL;

	GSList *list = NULL;
	gchar *regex;
	
	xmlChar *prop;

	xmlNodePtr child;

	prop = xmlGetProp (cur, "case-sensitive");
	if (prop != NULL)
	{
		if (!xmlStrcasecmp (prop, (const xmlChar *)"TRUE") ||
		    !xmlStrcmp (prop, (const xmlChar *)"1"))

				case_sensitive = TRUE;
			else
				case_sensitive = FALSE;

		xmlFree (prop);	
	}

	prop = xmlGetProp (cur, "match-empty-string-at-beginning");
	if (prop != NULL)
	{
		if (!xmlStrcasecmp (prop, (const xmlChar *)"TRUE") ||
		    !xmlStrcmp (prop, (const xmlChar *)"1"))

				match_empty_string_at_beginning = TRUE;
			else
				match_empty_string_at_beginning = FALSE;

		xmlFree (prop);	
	}

	prop = xmlGetProp (cur, "match-empty-string-at-end");
	if (prop != NULL)
	{
		if (!xmlStrcasecmp (prop, (const xmlChar *)"TRUE") ||
		    !xmlStrcmp (prop, (const xmlChar *)"1"))

				match_empty_string_at_end = TRUE;
			else
				match_empty_string_at_end = FALSE;

		xmlFree (prop);	
	}

	prop = xmlGetProp (cur, "beginning-regex");
	if (prop != NULL)
	{
		beginning_regex = g_strdup (prop);
		
		xmlFree (prop);	
	}

	prop = xmlGetProp (cur, "end-regex");
	if (prop != NULL)
	{
		end_regex = g_strdup (prop);
		
		xmlFree (prop);	
	}

	child = cur->xmlChildrenNode;
	
	while (child != NULL)
	{
		if (!xmlStrcmp (child->name, (const xmlChar *)"keyword"))
		{
			xmlChar *keyword;
			keyword = xmlNodeListGetString (child->doc, child->xmlChildrenNode, 1);
			
			list = g_slist_prepend (list, _gtk_source_language_strconvescape (keyword));
		}

		child = child->next;
	}

	list = g_slist_reverse (list);

	if (list == NULL)
	{
		g_warning ("No keywords in tag 'keyword-list' (%s, line %ld)", 
			   child->doc->name, xmlGetLineNo (cur));

		g_free (beginning_regex),
		g_free (end_regex);
		
		return;
	}

	regex = build_keyword_list (id,
				    list,
				    case_sensitive,
				    match_empty_string_at_beginning,
				    match_empty_string_at_end,
				    _gtk_source_language_strconvescape (beginning_regex),
				    _gtk_source_language_strconvescape (end_regex));

	g_free (beginning_regex),
	g_free (end_regex);

	g_slist_foreach (list, (GFunc) xmlFree, NULL);
	g_slist_free (list);

	gtk_source_simple_engine_add_simple_pattern (se, id, style, regex);

	g_free (regex);
}

static void 
parsePatternItem (xmlNodePtr             cur,
		  gchar                 *id,
		  xmlChar               *style,
		  GtkSourceSimpleEngine *se)
{
	xmlNodePtr child;

	child = cur->xmlChildrenNode;
		
	if ((child != NULL) && !xmlStrcmp (child->name, (const xmlChar *)"regex"))
	{
		xmlChar *regex;
			
		regex = xmlNodeListGetString (child->doc, child->xmlChildrenNode, 1);
			
		gtk_source_simple_engine_add_simple_pattern (se, id, style,
							     _gtk_source_language_strconvescape (regex));

		xmlFree (regex);
	}
	else
	{
		g_warning ("Missing regex in tag 'pattern-item' (%s, line %ld)", 
			   child->doc->name, xmlGetLineNo (child));
	}
}

static void 
parseSyntaxItem (xmlNodePtr             cur,
		 const gchar           *id,
		 xmlChar               *style,
		 GtkSourceSimpleEngine *se)
{
	xmlChar *start_regex = NULL;
	xmlChar *end_regex = NULL;

	xmlNodePtr child;

	child = cur->xmlChildrenNode;
	
	while (child != NULL)
	{	
		if (!xmlStrcmp (child->name, (const xmlChar *)"start-regex"))
		{
			start_regex = xmlNodeListGetString (child->doc, child->xmlChildrenNode, 1);
		}
		else
		if (!xmlStrcmp (child->name, (const xmlChar *)"end-regex"))
		{
			end_regex = xmlNodeListGetString (child->doc, child->xmlChildrenNode, 1);
		}

		child = child->next;
	}

	if (start_regex == NULL)
	{
		g_warning ("Missing start-regex in tag 'syntax-item' (%s, line %ld)", 
			   child->doc->name, xmlGetLineNo (cur));

		return;
	}

	if (end_regex == NULL)
	{
		xmlFree (start_regex);

		g_warning ("Missing end-regex in tag 'syntax-item' (%s, line %ld)", 
			   child->doc->name, xmlGetLineNo (cur));

		return;
	}

	gtk_source_simple_engine_add_syntax_pattern (se, id, style,
						     _gtk_source_language_strconvescape (start_regex),
						     _gtk_source_language_strconvescape (end_regex));

	xmlFree (start_regex);
	xmlFree (end_regex);
}

static void 
parseTag (GtkSourceLanguage     *language,
	  xmlNodePtr             cur,
	  GSList               **tags,
	  GtkSourceSimpleEngine *engine,
	  gboolean               populate_styles_table)
{
	xmlChar *name;
	xmlChar *style;
	xmlChar *id;
	
	name = xmlGetProp (cur, "_name");
	if (name == NULL)
	{
		name = xmlGetProp (cur, "name");
		id = xmlStrdup (name);
	}
	else
	{
		xmlChar *tmp = xmlStrdup (dgettext (language->priv->translation_domain, name));
		id = name;
		name = tmp;
	}
	
	if (name == NULL)
	{
		return;
	}

	style = xmlGetProp (cur, "style");

	if (style == NULL)
	{
		/* FIXME */
		style = xmlStrdup ("Normal");
	}
	
	if (engine)
	{
		if (!xmlStrcmp (cur->name, (const xmlChar *)"line-comment"))
		{
			parseLineComment (cur, id, style, engine);
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *)"block-comment"))
		{
			parseBlockComment (cur, id, style, engine);		
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *)"string"))
		{
			parseString (cur, id, style, engine);
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *)"keyword-list"))
		{
			parseKeywordList (cur, id, style, engine);
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *)"pattern-item"))
		{
			parsePatternItem (cur, id, style, engine);
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *)"syntax-item"))
		{
			parseSyntaxItem (cur, id, style, engine);
		}
		else
		{
			g_print ("Unknown tag: %s\n", cur->name);
		}
	}
	
	if (populate_styles_table)
		g_hash_table_insert (language->priv->tag_id_to_style_name, 
				     g_strdup (id), 
				     g_strdup (style));

	if (tags)
	{
		GtkTextTag *tag;
		GtkSourceTagStyle *ts;

		tag = gtk_source_tag_new (id, name);
		*tags = g_slist_prepend (*tags, tag);

		ts = gtk_source_language_get_tag_style (language, id);

		if (ts != NULL)
		{
			gtk_source_tag_set_style (GTK_SOURCE_TAG (tag), ts);
			gtk_source_tag_style_free (ts);
		}
		
	}
		
	xmlFree (name);
	xmlFree (style);
	xmlFree (id);
}

gboolean 
_gtk_source_language_file_parse_version1 (GtkSourceLanguage     *language,
					  GSList               **tags,
					  GtkSourceSimpleEngine *engine,
					  gboolean               populate_styles_table)
{
	xmlDocPtr doc;
	xmlNodePtr cur;

	xmlKeepBlanksDefault (0);

	doc = xmlParseFile (language->priv->lang_file_name);
	if (doc == NULL)
	{
		g_warning ("Impossible to parse file '%s'",
			   language->priv->lang_file_name);
		return FALSE;
	}

	cur = xmlDocGetRootElement (doc);
	
	if (cur == NULL) 
	{
		g_warning ("The lang file '%s' is empty",
			   language->priv->lang_file_name);

		xmlFreeDoc (doc);
		return FALSE;
	}

	if (xmlStrcmp (cur->name, (const xmlChar *) "language")) {
		g_warning ("File '%s' is of the wrong type",
			   language->priv->lang_file_name);
		
		xmlFreeDoc (doc);
		return FALSE;
	}

	/* FIXME: check that the language name, version, etc. are the 
	 * right ones - Paolo */

	cur = xmlDocGetRootElement (doc);
	cur = cur->xmlChildrenNode;
	g_return_val_if_fail (cur != NULL, FALSE);
	
	while (cur != NULL)
	{
		if (!xmlStrcmp (cur->name, (const xmlChar *)"escape-char"))
		{
			xmlChar *escape;
			gunichar esc_char;
			
			escape = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			esc_char = g_utf8_get_char_validated (escape, -1);
			if (esc_char < 0)
			{
				g_warning ("Invalid (non UTF8) escape character in file '%s'",
					   language->priv->lang_file_name);
				esc_char = 0;
			}
			xmlFree (escape);

			if (engine)
				gtk_source_simple_engine_set_escape_char (engine, esc_char);
			
			if (!tags && !engine)
				break;
		}
		else if (tags || engine)
		{
			parseTag (language,
				  cur, 
				  tags,
				  engine,
				  populate_styles_table);
		}
		
		cur = cur->next;
	}

	if (tags)
		*tags = g_slist_reverse (*tags);
      
	xmlFreeDoc (doc);

	return TRUE;
}

