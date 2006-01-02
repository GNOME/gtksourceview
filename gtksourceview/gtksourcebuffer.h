/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *  gtksourcebuffer.h
 *
 *  Copyright (C) 1999,2000,2001,2002 by:
 *          Mikael Hermansson <tyan@linux.se>
 *          Chris Phelps <chicane@reninet.com>
 *          Jeroen Zwartepoorte <jeroen@xs4all.nl>
 *
 *  Copyright (C) 2003 - Paolo Maggi, Gustavo Giráldez
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

#include <gtk/gtk.h>
#include <gtksourceview/gtksourcetagtable.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_BUFFER            (gtk_source_buffer_get_type ())
#define GTK_SOURCE_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_BUFFER, GtkSourceBuffer))
#define GTK_SOURCE_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_BUFFER, GtkSourceBufferClass))
#define GTK_IS_SOURCE_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_BUFFER))
#define GTK_IS_SOURCE_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_BUFFER))
#define GTK_SOURCE_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_BUFFER, GtkSourceBufferClass))

typedef struct _GtkSourceBuffer			GtkSourceBuffer;
typedef struct _GtkSourceBufferClass		GtkSourceBufferClass;
typedef struct _GtkSourceBufferPrivate		GtkSourceBufferPrivate;

struct _GtkSourceBuffer 
{
	GtkTextBuffer text_buffer;

	GtkSourceBufferPrivate *priv;
};

#include <gtksourceview/gtksourcefold.h>

struct _GtkSourceBufferClass 
{
	GtkTextBufferClass parent_class;

	void (* can_undo)		(GtkSourceBuffer *buffer,
					 gboolean         can_undo);
	void (* can_redo)		(GtkSourceBuffer *buffer,
					 gboolean         can_redo);
	/* views connect to this signal */
	void (* highlight_updated)      (GtkSourceBuffer *buffer,
					 GtkTextIter     *start,
					 GtkTextIter     *end);
	void (* marker_updated)         (GtkSourceBuffer *buffer,
					 GtkTextIter     *where);
	void (* fold_added)		(GtkSourceBuffer *buffer,
					 GtkSourceFold   *fold);
	void (* fold_remove)		(GtkSourceBuffer *buffer,
					 GtkSourceFold   *fold);

	/* highlighting engines connect to these */
	void (* text_inserted)          (GtkSourceBuffer   *buffer,
					 const GtkTextIter *start,
					 const GtkTextIter *end);
	void (* text_deleted)           (GtkSourceBuffer   *buffer,
					 const GtkTextIter *where,
					 gint               end_offset,
					 const gchar       *text,
					 gboolean           forward);
	void (* update_highlight)       (GtkSourceBuffer   *buffer,
					 const GtkTextIter *start,
					 const GtkTextIter *end,
					 gboolean           synchronous);

	/* Padding for future expansion */
	void (*_gtk_source_reserved1) 	(void);
	void (*_gtk_source_reserved2) 	(void);
	void (*_gtk_source_reserved3) 	(void);
};

#include <gtksourceview/gtksourcemarker.h>
#include <gtksourceview/gtksourcelanguage.h>

GType           	 gtk_source_buffer_get_type 		(void) G_GNUC_CONST;


/* Constructor */
GtkSourceBuffer	 	*gtk_source_buffer_new 			(GtkSourceTagTable      *table);
GtkSourceBuffer 	*gtk_source_buffer_new_with_language 	(GtkSourceLanguage      *language);

/* Properties. */
gboolean		 gtk_source_buffer_get_check_brackets   (GtkSourceBuffer        *buffer);
void			 gtk_source_buffer_set_check_brackets	(GtkSourceBuffer        *buffer,
							       	 gboolean                check_brackets);
void                     gtk_source_buffer_set_bracket_match_style 
                                                                (GtkSourceBuffer         *source_buffer,
								 const GtkSourceTagStyle *style);

gboolean		 gtk_source_buffer_get_highlight	(GtkSourceBuffer        *buffer);
void			 gtk_source_buffer_set_highlight	(GtkSourceBuffer        *buffer,
								 gboolean                highlight);

gint			 gtk_source_buffer_get_max_undo_levels	(GtkSourceBuffer        *buffer);
void			 gtk_source_buffer_set_max_undo_levels	(GtkSourceBuffer        *buffer,
							    	 gint                    max_undo_levels);

GtkSourceLanguage 	*gtk_source_buffer_get_language 	(GtkSourceBuffer        *buffer);
void			 gtk_source_buffer_set_language 	(GtkSourceBuffer        *buffer, 
								 GtkSourceLanguage      *language);

/* Undo/redo methods */
gboolean		 gtk_source_buffer_can_undo		(GtkSourceBuffer        *buffer);
gboolean		 gtk_source_buffer_can_redo		(GtkSourceBuffer        *buffer);

void			 gtk_source_buffer_undo			(GtkSourceBuffer        *buffer);
void			 gtk_source_buffer_redo			(GtkSourceBuffer        *buffer);

void			 gtk_source_buffer_begin_not_undoable_action (GtkSourceBuffer   *buffer);
void			 gtk_source_buffer_end_not_undoable_action   (GtkSourceBuffer   *buffer);

/* marker methods. */
GtkSourceMarker         *gtk_source_buffer_create_marker        (GtkSourceBuffer        *buffer,
								 const gchar            *name,
								 const gchar            *type,
								 const GtkTextIter      *where);

void                     gtk_source_buffer_move_marker          (GtkSourceBuffer        *buffer,
								 GtkSourceMarker        *marker,
								 const GtkTextIter      *where);
void                     gtk_source_buffer_delete_marker        (GtkSourceBuffer        *buffer,
								 GtkSourceMarker        *marker);
GtkSourceMarker         *gtk_source_buffer_get_marker           (GtkSourceBuffer        *buffer,
								 const gchar            *name);
GSList                  *gtk_source_buffer_get_markers_in_region 
                                                                (GtkSourceBuffer        *buffer,
								 const GtkTextIter      *begin,
								 const GtkTextIter      *end);
GtkSourceMarker         *gtk_source_buffer_get_first_marker     (GtkSourceBuffer        *buffer);
GtkSourceMarker         *gtk_source_buffer_get_last_marker      (GtkSourceBuffer        *buffer);
void                     gtk_source_buffer_get_iter_at_marker   (GtkSourceBuffer        *buffer,
								 GtkTextIter            *iter,
								 GtkSourceMarker        *marker);
GtkSourceMarker         *gtk_source_buffer_get_next_marker      (GtkSourceBuffer        *buffer,
								 GtkTextIter            *iter);
GtkSourceMarker         *gtk_source_buffer_get_prev_marker      (GtkSourceBuffer        *buffer,
								 GtkTextIter            *iter);

/* fold methods. */
gboolean		 gtk_source_buffer_get_folds_enabled	(GtkSourceBuffer        *buffer);
void			 gtk_source_buffer_set_folds_enabled	(GtkSourceBuffer        *buffer,
								 gboolean                enable_folds);

GtkSourceFold		*gtk_source_buffer_add_fold		(GtkSourceBuffer        *buffer,
								 const GtkTextIter      *begin,
								 const GtkTextIter      *end);
void			 gtk_source_buffer_remove_fold		(GtkSourceBuffer        *buffer,
								 GtkSourceFold          *fold);

void			 gtk_source_buffer_remove_folds_in_region
								(GtkSourceBuffer        *buffer,
								 const GtkTextIter      *begin,
								 const GtkTextIter      *end);

GtkSourceFold		*gtk_source_buffer_get_fold_at_iter	(GtkSourceBuffer        *buffer,
								 const GtkTextIter      *iter);

const GList		*gtk_source_buffer_get_root_folds	(GtkSourceBuffer        *buffer);

/* INTERNAL private stuff - not even exported from the library on
 * many platforms
 */
void 	                 _gtk_source_buffer_highlight_region    (GtkSourceBuffer        *source_buffer,
								 const GtkTextIter      *start,
								 const GtkTextIter      *end,
								 gboolean                highlight_now);

GList			*_gtk_source_buffer_get_folds_in_region	(GtkSourceBuffer        *buffer,
								 const GtkTextIter      *begin,
								 const GtkTextIter      *end);

GtkSourceFold		*_gtk_source_buffer_get_fold_at_line	(GtkSourceBuffer        *buffer,
								 gint                    line);


G_END_DECLS

#endif /* __GTK_SOURCE_BUFFER_H__ */
