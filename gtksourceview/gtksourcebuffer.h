/*  gtksourcebuffer.h
 *
 *  Copyright (C) 1999,2000,2001,2002 by:
 *          Mikael Hermansson <tyan@linux.se>
 *          Chris Phelps <chicane@reninet.com>
 *          Jeroen Zwartepoorte <jeroen@xs4all.nl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_SOURCE_BUFFER_H__
#define __GTK_SOURCE_BUFFER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <regex.h>
#include <gtk/gtk.h>
#include "gtksourcetag.h"

#define GTK_TYPE_SOURCE_BUFFER			(gtk_source_buffer_get_type ())
#define GTK_SOURCE_BUFFER(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_SOURCE_BUFFER, GtkSourceBuffer))
#define GTK_SOURCE_BUFFER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_BUFFER, GtkSourceBufferClass))
#define GTK_IS_SOURCE_BUFFER(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_SOURCE_BUFFER))
#define GTK_IS_SOURCE_BUFFER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_BUFFER))

typedef struct _GtkSourceBuffer			GtkSourceBuffer;
typedef struct _GtkSourceBufferClass		GtkSourceBufferClass;
typedef struct _GtkSourceBufferPrivate		GtkSourceBufferPrivate;

struct _GtkSourceBuffer {
	GtkTextBuffer TextBuffer;

	GtkSourceBufferPrivate *priv;
};

struct _GtkSourceBufferClass {
	GtkTextBufferClass parent_class;

	void (* can_undo)		(GtkSourceBuffer *buffer,
					 gboolean         can_undo);
	void (* can_redo)		(GtkSourceBuffer *buffer,
					 gboolean         can_redo);
};

/* Creation. */
GType            gtk_source_buffer_get_type (void);
GtkSourceBuffer *gtk_source_buffer_new (GtkTextTagTable *table);
void             gtk_source_buffer_attach_to_view (GtkSourceBuffer *buffer,
						   GtkTextView     *view);

/* Properties. */
void             gtk_source_buffer_set_check_brackets (GtkSourceBuffer *buf,
						       gboolean         check_brackets);
gboolean         gtk_source_buffer_get_highlight      (GtkSourceBuffer *buffer);
void             gtk_source_buffer_set_highlight      (GtkSourceBuffer *buf,
						       gboolean         highlight);

/* Tags methods. */
gint             gtk_source_buffer_get_tag_start       (GtkTextIter *iter);
gint             gtk_source_buffer_get_tag_end         (GtkTextIter *iter);
GtkSyntaxTag    *gtk_source_buffer_iter_has_syntax_tag (GtkTextIter *iter);

/* Regex methods. */
gint             gtk_source_buffer_regex_search (const gchar          *text,
						 gint                  pos,
						 Regex                *regex,
						 gboolean              forward,
						 GtkSourceBufferMatch *match);
gint             gtk_source_buffer_regex_match  (const gchar          *text,
						 gint                  pos,
						 gint                  end,
						 Regex                *regex);

GList           *gtk_source_buffer_get_regex_tags      (GtkSourceBuffer *buffer);
void             gtk_source_buffer_purge_regex_tags    (GtkSourceBuffer *buffer);
void             gtk_source_buffer_install_regex_tags  (GtkSourceBuffer *buffer,
							GList           *entries);
void             gtk_source_buffer_sync_syntax_regex   (GtkSourceBuffer *buffer);
GtkSyntaxTag    *gtk_source_buffer_iter_has_syntax_tag (GtkTextIter     *iter);

/* Get only the syntax, pattern, or embedded entries. */
GList           *gtk_source_buffer_get_syntax_entries   (GtkSourceBuffer *buffer);
GList           *gtk_source_buffer_get_pattern_entries  (GtkSourceBuffer *buffer);
GList           *gtk_source_buffer_get_embedded_entries (GtkSourceBuffer *buffer);

/* Utility methods. */
gchar           *gtk_source_buffer_convert_to_html    (GtkSourceBuffer *buffer,
						       const gchar     *title);
gboolean         gtk_source_buffer_find_bracket_match (GtkTextIter     *orig);

/* Undo/redo methods. */
gboolean         gtk_source_buffer_can_undo (const GtkSourceBuffer *buffer);
gboolean         gtk_source_buffer_can_redo (const GtkSourceBuffer *buffer);

void             gtk_source_buffer_undo (GtkSourceBuffer *buffer);
void             gtk_source_buffer_redo (GtkSourceBuffer *buffer);

void             gtk_source_buffer_begin_not_undoable_action (GtkSourceBuffer *buffer);
void             gtk_source_buffer_end_not_undoable_action   (GtkSourceBuffer *buffer);

/* Line marker methods. */
void             gtk_source_buffer_line_add_marker     (GtkSourceBuffer *buffer,
							gint             line,
							const gchar     *marker);
void             gtk_source_buffer_line_set_marker     (GtkSourceBuffer *buffer,
							gint             line,
							const gchar     *marker);
gboolean         gtk_source_buffer_line_remove_marker  (GtkSourceBuffer *buffer,
							gint             line,
							const gchar     *marker);
const GList     *gtk_source_buffer_line_get_markers    (GtkSourceBuffer *buffer,
							gint             line);
gint             gtk_source_buffer_line_has_markers    (GtkSourceBuffer *buffer,
							gint             line);
gint             gtk_source_buffer_line_remove_markers (GtkSourceBuffer *buffer,
							gint             line);
gint             gtk_source_view_remove_all_markers    (GtkSourceBuffer *buffer,
							gint             line_start,
							gint             line_end);

/* IO utility methods. */
gboolean         gtk_source_buffer_load (GtkSourceBuffer *buffer,
					 const gchar     *filename);
gboolean         gtk_source_buffer_save (GtkSourceBuffer *buffer,
					 const gchar     *filename);
gboolean         gtk_source_buffer_load_with_character_encoding (GtkSourceBuffer *buffer,
								 const gchar     *filename,
								 const gchar     *encoding);
gboolean         gtk_source_buffer_save_with_character_encoding (GtkSourceBuffer *buffer,
								 const gchar     *filename,
								 const gchar     *encoding);

#ifdef __cplusplus
}
#endif

#endif /* __GTK_SOURCE_BUFFER_H__ */
