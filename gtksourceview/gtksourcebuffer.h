/*  gtksourcebuffer.h
 *
 *  Mikael Hermansson <tyan@linux.se>
 *  Chris Phelps <chicane@reninet.com>
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

#include <gtk/gtk.h>
#include <gtk/gtktextbuffer.h>
#include <regex.h>
#include <gtksourcetag.h>

#define GTK_TYPE_SOURCE_BUFFER                  (gtk_source_buffer_get_type ())
#define GTK_SOURCE_BUFFER(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_SOURCE_BUFFER, GtkSourceBuffer))
#define GTK_SOURCE_BUFFER_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_BUFFER, GtkSourceBufferClass))
#define GTK_IS_SOURCE_BUFFER(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_SOURCE_BUFFER))
#define GTK_IS_SOURCE_BUFFER_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_BUFFER))

#define UNDO_TYPE_INSERT_TEXT 1
#define UNDO_TYPE_REMOVE_RANGE 2

typedef struct _GtkSourceBufferUndoEntry {
	gint type;
	gint offset;
	gint length;
	gpointer data;
}GtkSourceBufferUndoEntry;

typedef struct _GtkSourceBufferInfo
{
  gchar *filename;
  gint buffersize;   /* bytes in memory */
  gint filesize;       /* bytes written on disc */
  gint filedate;      /* last time file was saved */
  gint modified : 1;  /* is buffer modified since last write */
}GtkSourceBufferInfo;

typedef struct _GtkSourceBuffer {
	GtkTextBuffer TextBuffer;

    GtkSourceBufferInfo *info;

	gint highlight :1;
	gint check_brackets :1;
	gint refresh_start;
	gint refresh_length;
	GtkTextTag *bracket_match_tag;
	GtkTextMark *mark;
	GHashTable *line_markers;
	/* Undo coding */
	gint undo_max;
	GList *undo_redo;
	guint undo_level;
	guint undo_redo_processing : 1;
	GList *syntax_items;
	GList *pattern_items;
	GList *embedded_items;
	Regex reg_syntax_all;
} GtkSourceBuffer;

typedef struct _GtkSourceBufferClass {
	GtkTextBufferClass parent_class;
} GtkSourceBufferClass;



GType gtk_source_buffer_get_type(void);

GtkTextBuffer* gtk_source_buffer_new(GtkTextTagTable *table);
void gtk_source_buffer_attach_to_view(GtkSourceBuffer *buffer, GtkTextView *view);

gint gtk_source_buffer_regex_search(const char *text, gint pos, Regex *regex,
				    gboolean forward, GtkSourceBufferMatch *m);
gint gtk_source_buffer_regex_match(const char *text, gint pos, gint end, Regex *regex);

void gtk_source_buffer_set_check_brackets(GtkSourceBuffer *buf, gboolean check);
void gtk_source_buffer_set_highlight(GtkSourceBuffer *buf, gboolean set);

GtkSyntaxTag* gtk_source_buffer_iter_has_syntax_tag(GtkTextIter *text);
gint gtk_source_buffer_get_tag_start(GtkTextIter *iter);
gint gtk_source_buffer_get_tag_end(GtkTextIter *iter);

/* Get all the entries */
GList *gtk_source_buffer_get_regex_tags(GtkSourceBuffer *buf);
void gtk_source_buffer_purge_regex_tags(GtkSourceBuffer *buf);
void gtk_source_buffer_install_regex_tags(GtkSourceBuffer *buf, GList *entries);

void gtk_source_buffer_sync_syntax_regex(GtkSourceBuffer *buf);
GtkSyntaxTag *gtk_source_buffer_iter_has_syntax_tag(GtkTextIter *iter);

/* Get only the syntax, pattern, or embedded entries */
GList *gtk_source_buffer_get_syntax_entries(GtkSourceBuffer *buf);
GList *gtk_source_buffer_get_pattern_entries(GtkSourceBuffer *buf);
GList *gtk_source_buffer_get_embedded_entries(GtkSourceBuffer *buf);

gchar *gtk_source_buffer_convert_to_html(GtkSourceBuffer *buf, const gchar*htmltitle);
gboolean gtk_source_buffer_find_bracket_match(GtkTextIter *orig);

gboolean gtk_source_buffer_undo(GtkSourceBuffer *buf);
gboolean gtk_source_buffer_redo(GtkSourceBuffer *buf);
gboolean gtk_source_buffer_undo_is_empty(GtkSourceBuffer *buf);
gboolean gtk_source_buffer_redo_is_empty(GtkSourceBuffer *buf);
gint gtk_source_buffer_undo_get_max(GtkSourceBuffer *buf);
gboolean gtk_source_buffer_undo_set_max(GtkSourceBuffer *buf, gint max);
void gtk_source_buffer_undo_clear_all(GtkSourceBuffer *buf);

/* Set the line marker, and remove any others if there are already some set for the line */
void gtk_source_buffer_line_set_marker(GtkSourceBuffer *buffer, gint line, const gchar *marker);
void gtk_source_buffer_line_add_marker(GtkSourceBuffer *buffer, gint line, const gchar *marker);
gint gtk_source_buffer_line_has_markers(GtkSourceBuffer *view, gint line);
const GList *gtk_source_buffer_line_get_markers(GtkSourceBuffer *view, gint line);
gint gtk_source_buffer_line_remove_markers(GtkSourceBuffer *buffer, gint line);
gboolean gtk_source_buffer_line_remove_marker(GtkSourceBuffer *buffer, gint line,
					      const gchar *marker);
/* return value is the number removed */
/* pass -1 for start and end to remove all */
gint gtk_source_view_remove_all_markers(GtkSourceBuffer *buffer, gint line_start, gint line_end);

/* save/load and status API */

void gtk_source_buffer_set_filename (GtkSourceBuffer *buffer, const gchar *filename);
const gchar *gtk_source_buffer_get_filename (GtkSourceBuffer *buffer);
const GtkSourceBufferInfo* gtk_source_buffer_get_info (GtkSourceBuffer *buffer);

gboolean gtk_source_buffer_load (GtkSourceBuffer *buffer, const gchar *filename);
gboolean gtk_source_buffer_load_with_character_encoding (GtkSourceBuffer *buffer, const gchar *filename, const gchar *input_encoding);
gboolean gtk_source_buffer_save (GtkSourceBuffer *buffer, const gchar *filename);
gboolean gtk_source_buffer_save_with_character_encoding (GtkSourceBuffer *buffer, const gchar *filename, const gchar *output_encoding);

#ifdef __cplusplus
}
#endif
#endif
