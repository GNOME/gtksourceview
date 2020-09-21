/*
 * This file is part of GtkSourceView
 *
 * Copyright 2013 - Sébastien Wilmet <swilmet@gnome.org>
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

#pragma once

#include <gtk/gtk.h>
#include "gtksourcetypes-private.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_MARKS_SEQUENCE (_gtk_source_marks_sequence_get_type())

G_GNUC_INTERNAL
G_DECLARE_FINAL_TYPE (GtkSourceMarksSequence, _gtk_source_marks_sequence, GTK_SOURCE, MARKS_SEQUENCE, GObject)

G_GNUC_INTERNAL
GtkSourceMarksSequence *_gtk_source_marks_sequence_new                (GtkTextBuffer          *buffer);
G_GNUC_INTERNAL
gboolean                _gtk_source_marks_sequence_is_empty           (GtkSourceMarksSequence *seq);
G_GNUC_INTERNAL
void                    _gtk_source_marks_sequence_add                (GtkSourceMarksSequence *seq,
                                                                       GtkTextMark            *mark);
G_GNUC_INTERNAL
void                    _gtk_source_marks_sequence_remove             (GtkSourceMarksSequence *seq,
                                                                       GtkTextMark            *mark);
G_GNUC_INTERNAL
GtkTextMark            *_gtk_source_marks_sequence_next               (GtkSourceMarksSequence *seq,
                                                                       GtkTextMark            *mark);
G_GNUC_INTERNAL
GtkTextMark            *_gtk_source_marks_sequence_prev               (GtkSourceMarksSequence *seq,
                                                                       GtkTextMark            *mark);
G_GNUC_INTERNAL
gboolean                _gtk_source_marks_sequence_forward_iter       (GtkSourceMarksSequence *seq,
                                                                       GtkTextIter            *iter);
G_GNUC_INTERNAL
gboolean                _gtk_source_marks_sequence_backward_iter      (GtkSourceMarksSequence *seq,
                                                                       GtkTextIter            *iter);
G_GNUC_INTERNAL
GSList                 *_gtk_source_marks_sequence_get_marks_at_iter  (GtkSourceMarksSequence *seq,
                                                                       const GtkTextIter      *iter);
G_GNUC_INTERNAL
GSList                 *_gtk_source_marks_sequence_get_marks_in_range (GtkSourceMarksSequence *seq,
                                                                       const GtkTextIter      *iter1,
                                                                       const GtkTextIter      *iter2);

G_END_DECLS
