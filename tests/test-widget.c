/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2001 - Mikael Hermansson <tyan@linux.se>
 * Copyright (C) 2003 - Gustavo Giráldez <gustavo.giraldez@gmx.net>
 * Copyright (C) 2014 - Sébastien Wilmet <swilmet@gnome.org>
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

#include <string.h>
#include <gtksourceview/gtksource.h>

#define TEST_TYPE_ANNOTATION_PROVIDER (test_annotation_provider_get_type())
#define TEST_TYPE_WIDGET (test_widget_get_type())
#define TEST_TYPE_HOVER_PROVIDER (test_hover_provider_get_type())

G_DECLARE_FINAL_TYPE (TestWidget, test_widget, TEST, WIDGET, GtkGrid)
G_DECLARE_FINAL_TYPE (TestHoverProvider, test_hover_provider, TEST, HOVER_PROVIDER, GObject)
G_DECLARE_FINAL_TYPE (TestAnnotationProvider, test_annotation_provider, TEST, ANNOTATION_PROVIDER, GtkSourceAnnotationProvider)

struct _TestWidget
{
	GtkGrid parent_instance;

	GtkSourceView *view;
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceMap *map;
	GtkCheckButton *show_top_border_window_checkbutton;
	GtkCheckButton *show_map_checkbutton;
	GtkCheckButton *draw_spaces_checkbutton;
	GtkCheckButton *smart_backspace_checkbutton;
	GtkCheckButton *indent_width_checkbutton;
	GtkCheckButton *vim_checkbutton;
	GtkSpinButton *indent_width_spinbutton;
	GtkLabel *cursor_position_info;
	GtkSourceStyleSchemeChooserButton *chooser_button;
	GtkIMContext *vim_im_context;
	GtkDropDown *background_pattern;
	GtkWidget *top;
	GtkScrolledWindow *scrolledwindow1;
	GtkEventController *vim_controller;
	GtkLabel *command_bar;
	GtkSourceAnnotationProvider *annotation_provider;
	GtkSourceAnnotationProvider *error_provider;
};

struct _TestHoverProvider
{
	GObject parent_instance;
};

struct _TestAnnotationProvider
{
	GObject parent_instance;
};

static void hover_provider_iface_init (GtkSourceHoverProviderInterface *iface);

G_DEFINE_TYPE (TestWidget, test_widget, GTK_TYPE_GRID)
G_DEFINE_TYPE_WITH_CODE (TestHoverProvider, test_hover_provider, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_HOVER_PROVIDER, hover_provider_iface_init))
G_DEFINE_TYPE (TestAnnotationProvider, test_annotation_provider, GTK_SOURCE_TYPE_ANNOTATION_PROVIDER)

#define MARK_TYPE_1      "one"
#define MARK_TYPE_2      "two"

static GMainLoop *main_loop;
static const char *cmd_filename;

static void
remove_all_marks (GtkSourceBuffer *buffer)
{
	GtkTextIter start;
	GtkTextIter end;

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);

	gtk_source_buffer_remove_source_marks (buffer, &start, &end, NULL);
}

static GtkSourceLanguage *
get_language_for_file (GtkTextBuffer *buffer,
		       const gchar   *filename)
{
	GtkSourceLanguageManager *manager;
	GtkSourceLanguage *language;
	GtkTextIter start;
	GtkTextIter end;
	gchar *text;
	gchar *content_type;
	gboolean result_uncertain;

	gtk_text_buffer_get_start_iter (buffer, &start);
	gtk_text_buffer_get_iter_at_offset (buffer, &end, 1024);
	text = gtk_text_buffer_get_slice (buffer, &start, &end, TRUE);

	content_type = g_content_type_guess (filename,
					     (guchar*) text,
					     strlen (text),
					     &result_uncertain);

	if (result_uncertain)
	{
		g_free (content_type);
		content_type = NULL;
	}

	manager = gtk_source_language_manager_get_default ();
	language = gtk_source_language_manager_guess_language (manager,
							       filename,
							       content_type);

	g_message ("Detected '%s' mime type for file %s, chose language %s",
		   content_type != NULL ? content_type : "(null)",
		   filename,
		   language != NULL ? gtk_source_language_get_id (language) : "(none)");

	g_free (content_type);
	g_free (text);
	return language;
}

static GtkSourceLanguage *
get_language_by_id (const gchar *id)
{
	GtkSourceLanguageManager *manager;
	manager = gtk_source_language_manager_get_default ();
	return gtk_source_language_manager_get_language (manager, id);
}

static GtkSourceLanguage *
get_language (GtkTextBuffer *buffer,
	      GFile         *location)
{
	GtkSourceLanguage *language = NULL;
	GtkTextIter start;
	GtkTextIter end;
	gchar *text;
	gchar *lang_string;

	gtk_text_buffer_get_start_iter (buffer, &start);
	end = start;
	gtk_text_iter_forward_line (&end);

#define LANG_STRING "gtk-source-lang:"

	text = gtk_text_iter_get_slice (&start, &end);
	lang_string = strstr (text, LANG_STRING);

	if (lang_string != NULL)
	{
		gchar **tokens;

		lang_string += strlen (LANG_STRING);
		g_strchug (lang_string);

		tokens = g_strsplit_set (lang_string, " \t\n", 2);

		if (tokens != NULL && tokens[0] != NULL)
		{
			language = get_language_by_id (tokens[0]);
		}

		g_strfreev (tokens);
	}

	if (language == NULL)
	{
		gchar *filename = g_file_get_path (location);
		language = get_language_for_file (buffer, filename);
		g_free (filename);
	}

	g_free (text);
	return language;
}

static void
print_language_style_ids (GtkSourceLanguage *language)
{
	gchar **styles;

	g_assert_nonnull (language);

	styles = gtk_source_language_get_style_ids (language);

	if (styles == NULL)
	{
		g_print ("No styles in language '%s'\n",
			 gtk_source_language_get_name (language));
	}
	else
	{
		gchar **ids;
		g_print ("Styles in language '%s':\n",
			 gtk_source_language_get_name (language));

		for (ids = styles; *ids != NULL; ids++)
		{
			const gchar *name = gtk_source_language_get_style_name (language, *ids);

			g_print ("- %s (name: '%s')\n", *ids, name);
		}

		g_strfreev (styles);
	}

	g_print ("\n");
}

static void
load_cb (GtkSourceFileLoader *loader,
	 GAsyncResult        *result,
	 TestWidget          *self)
{
	GtkTextIter iter;
	GFile *location;
	GtkSourceLanguage *language = NULL;
	GError *error = NULL;

	gtk_source_file_loader_load_finish (loader, result, &error);

	if (error != NULL)
	{
		g_warning ("Error while loading the file: %s", error->message);
		g_clear_error (&error);
		g_clear_object (&self->file);
		goto end;
	}

	/* move cursor to the beginning */
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self->buffer), &iter);
	gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (self->buffer), &iter);
	gtk_widget_grab_focus (GTK_WIDGET (self->view));

	location = gtk_source_file_loader_get_location (loader);

	language = get_language (GTK_TEXT_BUFFER (self->buffer), location);
	gtk_source_buffer_set_language (self->buffer, language);

	if (language != NULL)
	{
		print_language_style_ids (language);
	}
	else
	{
		gchar *path = g_file_get_path (location);
		g_print ("No language found for file '%s'\n", path);
		g_free (path);
	}

end:
	g_object_unref (loader);
}

static void
open_file (TestWidget *self,
           GFile      *file)
{
	GtkSourceFileLoader *loader;

	g_clear_object (&self->file);
	self->file = gtk_source_file_new ();
	gtk_source_file_set_location (self->file, file);

	loader = gtk_source_file_loader_new (self->buffer,
					     self->file);

	remove_all_marks (self->buffer);

	gtk_source_file_loader_load_async (loader,
					   G_PRIORITY_DEFAULT,
					   NULL,
					   NULL, NULL, NULL,
					   (GAsyncReadyCallback) load_cb,
					   self);
}

static void
show_line_numbers_toggled_cb (TestWidget     *self,
			      GtkCheckButton *button)
{
	gboolean enabled = gtk_check_button_get_active (button);
	gtk_source_view_set_show_line_numbers (self->view, enabled);
}

static void
show_line_marks_toggled_cb (TestWidget     *self,
			    GtkCheckButton *button)
{
	gboolean enabled = gtk_check_button_get_active (button);
	gtk_source_view_set_show_line_marks (self->view, enabled);
}

static void
show_right_margin_toggled_cb (TestWidget     *self,
			      GtkCheckButton *button)
{
	gboolean enabled = gtk_check_button_get_active (button);
	gtk_source_view_set_show_right_margin (self->view, enabled);
}

static void
right_margin_position_value_changed_cb (TestWidget    *self,
					GtkSpinButton *button)
{
	gint position = gtk_spin_button_get_value_as_int (button);

	gtk_source_view_set_right_margin_position (self->view, position);
	gtk_source_view_set_right_margin_position (GTK_SOURCE_VIEW (self->map), position);
	gtk_widget_queue_resize (GTK_WIDGET (self->map));
}

static void
bottom_margin_value_changed_cb (TestWidget    *self,
			        GtkSpinButton *button)
{
	gint margin = gtk_spin_button_get_value_as_int (button);

	gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (self->view), margin);
}

static void
highlight_syntax_toggled_cb (TestWidget     *self,
			     GtkCheckButton *button)
{
	gboolean enabled = gtk_check_button_get_active (button);
	gtk_source_buffer_set_highlight_syntax (self->buffer, enabled);
}

static void
highlight_matching_bracket_toggled_cb (TestWidget     *self,
				       GtkCheckButton *button)
{
	gboolean enabled = gtk_check_button_get_active (button);
	gtk_source_buffer_set_highlight_matching_brackets (self->buffer, enabled);
}

static void
highlight_current_line_toggled_cb (TestWidget     *self,
				   GtkCheckButton *button)
{
	gboolean enabled = gtk_check_button_get_active (button);
	gtk_source_view_set_highlight_current_line (self->view, enabled);
}

static void
wrap_lines_toggled_cb (TestWidget     *self,
		       GtkCheckButton *button)
{
	gboolean enabled = gtk_check_button_get_active (button);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (self->view),
				     enabled ? GTK_WRAP_WORD_CHAR : GTK_WRAP_NONE);
}

static void
auto_indent_toggled_cb (TestWidget     *self,
			GtkCheckButton *button)
{
	gboolean enabled = gtk_check_button_get_active (button);
	gtk_source_view_set_auto_indent (self->view, enabled);
}

static void
indent_spaces_toggled_cb (TestWidget     *self,
			  GtkCheckButton *button)
{
	gboolean enabled = gtk_check_button_get_active (button);
	gtk_source_view_set_insert_spaces_instead_of_tabs (self->view, enabled);
}

static void
tab_width_value_changed_cb (TestWidget    *self,
			    GtkSpinButton *button)
{
	gint tab_width = gtk_spin_button_get_value_as_int (button);
	gtk_source_view_set_tab_width (self->view, tab_width);
}

static void
update_indent_width (TestWidget *self)
{
	gint indent_width = -1;

	if (gtk_check_button_get_active (self->indent_width_checkbutton))
	{
		indent_width = gtk_spin_button_get_value_as_int (self->indent_width_spinbutton);
	}

	gtk_source_view_set_indent_width (self->view, indent_width);
}

static void
smart_home_end_selected_notify_cb (TestWidget  *self,
				   GParamSpec  *pspec,
				   GtkDropDown *drop_down)
{
	GtkSourceSmartHomeEndType type;
	guint selected = gtk_drop_down_get_selected (drop_down);

	switch (selected)
	{
		case 0:
			type = GTK_SOURCE_SMART_HOME_END_DISABLED;
			break;

		case 1:
			type = GTK_SOURCE_SMART_HOME_END_BEFORE;
			break;

		case 2:
			type = GTK_SOURCE_SMART_HOME_END_AFTER;
			break;

		case 3:
			type = GTK_SOURCE_SMART_HOME_END_ALWAYS;
			break;

		default:
			type = GTK_SOURCE_SMART_HOME_END_DISABLED;
			break;
	}

	gtk_source_view_set_smart_home_end (self->view, type);
}

static void
backward_string_clicked_cb (TestWidget *self)
{
	GtkTextIter iter;
	GtkTextMark *insert;

	insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->buffer));

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->buffer),
	                                  &iter,
	                                  insert);

	if (gtk_source_buffer_iter_backward_to_context_class_toggle (self->buffer,
	                                                             &iter,
	                                                             "string"))
	{
		gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (self->buffer), &iter);
		gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (self->view), insert);
	}

	gtk_widget_grab_focus (GTK_WIDGET (self->view));
}

static void
forward_string_clicked_cb (TestWidget *self)
{
	GtkTextIter iter;
	GtkTextMark *insert;

	insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->buffer));

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->buffer),
	                                  &iter,
	                                  insert);

	if (gtk_source_buffer_iter_forward_to_context_class_toggle (self->buffer,
								    &iter,
								    "string"))
	{
		gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (self->buffer), &iter);
		gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (self->view), insert);
	}

	gtk_widget_grab_focus (GTK_WIDGET (self->view));
}

static void
on_file_dialog_response_cb (GObject      *source,
                            GAsyncResult *result,
                            gpointer      user_data)
{
	TestWidget *self = user_data;
	GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
	GError *error = NULL;
	GFile *file;

	g_assert (TEST_IS_WIDGET (self));
	g_assert (GTK_IS_FILE_DIALOG (dialog));

	file = gtk_file_dialog_open_finish (dialog, result, &error);

	if (file != NULL)
	{
		open_file (self, file);
		g_object_unref (file);
	}
}

static void
open_button_clicked_cb (TestWidget *self)
{
	GtkWidget *main_window;
	GtkFileDialog *dialog;

	main_window = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self->view)));

	dialog = gtk_file_dialog_new ();

	gtk_file_dialog_set_title (dialog, "Open file...");
	gtk_file_dialog_set_accept_label (dialog, "Open");

	gtk_file_dialog_open (dialog,
	                      GTK_WINDOW (main_window),
	                      NULL,
	                      on_file_dialog_response_cb,
	                      self);

	g_object_unref (dialog);
}

static void
markup_button_clicked_cb (TestWidget *self)
{
	GdkClipboard *clipboard;
	GtkTextIter start, end;
	char *markup;

	if (!gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (self->buffer), &start, &end))
	{
		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self->buffer), &start, &end);
	}

	markup = gtk_source_buffer_get_markup (self->buffer, &start, &end);

	clipboard = gdk_display_get_clipboard (gtk_widget_get_display(GTK_WIDGET(self)));
	gdk_clipboard_set_text (clipboard, markup);

	g_free (markup);
}


#define NON_BLOCKING_PAGINATION

#ifndef NON_BLOCKING_PAGINATION

static void
begin_print (GtkPrintOperation        *operation,
	     GtkPrintContext          *context,
	     GtkSourcePrintCompositor *compositor)
{
	gint n_pages;

	while (!gtk_source_print_compositor_paginate (compositor, context))
		;

	n_pages = gtk_source_print_compositor_get_n_pages (compositor);
	gtk_print_operation_set_n_pages (operation, n_pages);
}

#else

static gboolean
paginate (GtkPrintOperation        *operation,
	  GtkPrintContext          *context,
	  GtkSourcePrintCompositor *compositor)
{
	g_print ("Pagination progress: %.2f %%\n", gtk_source_print_compositor_get_pagination_progress (compositor) * 100.0);

	if (gtk_source_print_compositor_paginate (compositor, context))
	{
		gint n_pages;

		g_assert_cmpint (gtk_source_print_compositor_get_pagination_progress (compositor), ==, 1.0);
		g_print ("Pagination progress: %.2f %%\n", gtk_source_print_compositor_get_pagination_progress (compositor) * 100.0);

		n_pages = gtk_source_print_compositor_get_n_pages (compositor);
		gtk_print_operation_set_n_pages (operation, n_pages);

		return TRUE;
	}

	return FALSE;
}

#endif

#define ENABLE_CUSTOM_OVERLAY

static void
draw_page (GtkPrintOperation        *operation,
	   GtkPrintContext          *context,
	   gint                      page_nr,
	   GtkSourcePrintCompositor *compositor)
{
#ifdef ENABLE_CUSTOM_OVERLAY

	/* This part of the code shows how to add a custom overlay to the
	   printed text generated by GtkSourcePrintCompositor */

	cairo_t *cr;
	PangoLayout *layout;
	PangoFontDescription *desc;
	PangoRectangle rect;


	cr = gtk_print_context_get_cairo_context (context);

	cairo_save (cr);

	layout = gtk_print_context_create_pango_layout (context);

	pango_layout_set_text (layout, "Draft", -1);

	desc = pango_font_description_from_string ("Sans Bold 120");
	pango_layout_set_font_description (layout, desc);
	pango_font_description_free (desc);


	pango_layout_get_extents (layout, NULL, &rect);

  	cairo_move_to (cr,
  		       (gtk_print_context_get_width (context) - ((double) rect.width / (double) PANGO_SCALE)) / 2,
  		       (gtk_print_context_get_height (context) - ((double) rect.height / (double) PANGO_SCALE)) / 2);

	pango_cairo_layout_path (cr, layout);

  	/* Font Outline */
	cairo_set_source_rgba (cr, 0.85, 0.85, 0.85, 0.80);
	cairo_set_line_width (cr, 0.5);
	cairo_stroke_preserve (cr);

	/* Font Fill */
	cairo_set_source_rgba (cr, 0.8, 0.8, 0.8, 0.60);
	cairo_fill (cr);

	g_object_unref (layout);
	cairo_restore (cr);
#endif

	/* To print page_nr you only need to call the following function */
	gtk_source_print_compositor_draw_page (compositor, context, page_nr);
}

static void
end_print (GtkPrintOperation        *operation,
	   GtkPrintContext          *context,
	   GtkSourcePrintCompositor *compositor)
{
	g_object_unref (compositor);
}

#define LINE_NUMBERS_FONT_NAME	"Sans 8"
#define HEADER_FONT_NAME	"Sans 11"
#define FOOTER_FONT_NAME	"Sans 11"
#define BODY_FONT_NAME		"Monospace 9"

/*
#define SETUP_FROM_VIEW
*/

#undef SETUP_FROM_VIEW

static void
print_button_clicked_cb (TestWidget *self)
{
	gchar *basename = NULL;
	GtkSourcePrintCompositor *compositor;
	GtkPrintOperation *operation;

	if (self->file != NULL)
	{
		GFile *location;

		location = gtk_source_file_get_location (self->file);

		if (location != NULL)
		{
			basename = g_file_get_basename (location);
		}
	}

#ifdef SETUP_FROM_VIEW
	compositor = gtk_source_print_compositor_new_from_view (self->view);
#else
	compositor = gtk_source_print_compositor_new (self->buffer);

	gtk_source_print_compositor_set_tab_width (compositor,
						   gtk_source_view_get_tab_width (self->view));

	gtk_source_print_compositor_set_wrap_mode (compositor,
						   gtk_text_view_get_wrap_mode (GTK_TEXT_VIEW (self->view)));

	gtk_source_print_compositor_set_print_line_numbers (compositor, 1);

	gtk_source_print_compositor_set_body_font_name (compositor,
							BODY_FONT_NAME);

	/* To test line numbers font != text font */
	gtk_source_print_compositor_set_line_numbers_font_name (compositor,
								LINE_NUMBERS_FONT_NAME);

	gtk_source_print_compositor_set_header_format (compositor,
						       TRUE,
						       "Printed on %A",
						       "test-widget",
						       "%F");

	gtk_source_print_compositor_set_footer_format (compositor,
						       TRUE,
						       "%T",
						       basename,
						       "Page %N/%Q");

	gtk_source_print_compositor_set_print_header (compositor, TRUE);
	gtk_source_print_compositor_set_print_footer (compositor, TRUE);

	gtk_source_print_compositor_set_header_font_name (compositor,
							  HEADER_FONT_NAME);

	gtk_source_print_compositor_set_footer_font_name (compositor,
							  FOOTER_FONT_NAME);
#endif
	operation = gtk_print_operation_new ();

	gtk_print_operation_set_job_name (operation, basename);

	gtk_print_operation_set_show_progress (operation, TRUE);

#ifndef NON_BLOCKING_PAGINATION
  	g_signal_connect (G_OBJECT (operation), "begin-print",
			  G_CALLBACK (begin_print), compositor);
#else
  	g_signal_connect (G_OBJECT (operation), "paginate",
			  G_CALLBACK (paginate), compositor);
#endif
	g_signal_connect (G_OBJECT (operation), "draw-page",
			  G_CALLBACK (draw_page), compositor);
	g_signal_connect (G_OBJECT (operation), "end-print",
			  G_CALLBACK (end_print), compositor);

	gtk_print_operation_run (operation,
				 GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				 NULL, NULL);

	g_object_unref (operation);
	g_free (basename);
}

static void
update_cursor_position_info (TestWidget *self)
{
	gchar *msg;
	gint offset;
	gint line;
	guint column;
	GtkTextIter iter;
	gchar **classes;
	gchar **classes_ptr;
	GString *classes_str;
	GtkSourceLanguage *lang;
	const char *language = "none";

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->buffer),
					  &iter,
					  gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->buffer)));

	offset = gtk_text_iter_get_offset (&iter);
	line = gtk_text_iter_get_line (&iter) + 1;
	column = gtk_source_view_get_visual_column (self->view, &iter) + 1;

	classes = gtk_source_buffer_get_context_classes_at_iter (self->buffer, &iter);

	classes_str = g_string_new ("");

	for (classes_ptr = classes; classes_ptr != NULL && *classes_ptr != NULL; classes_ptr++)
	{
		if (classes_ptr != classes)
		{
			g_string_append (classes_str, ", ");
		}

		g_string_append_printf (classes_str, "%s", *classes_ptr);
	}

	g_strfreev (classes);

	if ((lang = gtk_source_buffer_get_language (self->buffer)))
		language = gtk_source_language_get_id (lang);

	msg = g_strdup_printf ("language: %s offset: %d, line: %d, column: %u, classes: %s",
	                       language,
	                       offset,
	                       line,
	                       column,
	                       classes_str->str);

	gtk_label_set_text (self->cursor_position_info, msg);

	g_free (msg);
	g_string_free (classes_str, TRUE);
}

static void
mark_set_cb (GtkTextBuffer *buffer,
	     GtkTextIter   *iter,
	     GtkTextMark   *mark,
	     TestWidget    *self)
{
	if (mark == gtk_text_buffer_get_insert (buffer))
	{
		update_cursor_position_info (self);
	}
}

static void
line_mark_activated_cb (GtkSourceGutter   *gutter,
                        const GtkTextIter *iter,
                        guint              button,
                        GdkModifierType    state,
                        gint               n_presses,
                        TestWidget        *self)
{
	GSList *mark_list;
	const gchar *mark_type;

	if ((state & GDK_SHIFT_MASK) != 0)
		mark_type = MARK_TYPE_2;
	else
		mark_type = MARK_TYPE_1;

	/* get the marks already in the line */
	mark_list = gtk_source_buffer_get_source_marks_at_line (self->buffer,
								gtk_text_iter_get_line (iter),
								mark_type);

	if (mark_list != NULL)
	{
		/* just take the first and delete it */
		gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (self->buffer),
					     GTK_TEXT_MARK (mark_list->data));
	}
	else
	{
		/* no mark found: create one */
		gtk_source_buffer_create_source_mark (self->buffer,
						      NULL,
						      mark_type,
						      iter);
	}

	g_slist_free (mark_list);
}

static void
bracket_matched_cb (GtkSourceBuffer           *buffer,
		    GtkTextIter               *iter,
		    GtkSourceBracketMatchType  state)
{
	GEnumClass *eclass;
	GEnumValue *evalue;

	eclass = G_ENUM_CLASS (g_type_class_ref (GTK_SOURCE_TYPE_BRACKET_MATCH_TYPE));
	evalue = g_enum_get_value (eclass, state);

	g_print ("Bracket match state: '%s'\n", evalue->value_nick);

	g_type_class_unref (eclass);

	if (state == GTK_SOURCE_BRACKET_MATCH_FOUND)
	{
		g_return_if_fail (iter != NULL);

		g_print ("Matched bracket: '%c' at row: %"G_GINT32_FORMAT", col: %"G_GINT32_FORMAT"\n",
		         gtk_text_iter_get_char (iter),
		         gtk_text_iter_get_line (iter) + 1,
		         gtk_text_iter_get_line_offset (iter) + 1);
	}
}

static gchar *
mark_tooltip_func (GtkSourceMarkAttributes *attrs,
                   GtkSourceMark           *mark,
                   GtkSourceView           *view)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gint line;
	gint column;

	buffer = gtk_text_mark_get_buffer (GTK_TEXT_MARK (mark));

	gtk_text_buffer_get_iter_at_mark (buffer, &iter, GTK_TEXT_MARK (mark));
	line = gtk_text_iter_get_line (&iter) + 1;
	column = gtk_text_iter_get_line_offset (&iter);

	if (g_strcmp0 (gtk_source_mark_get_category (mark), MARK_TYPE_1) == 0)
	{
		return g_strdup_printf ("Line: %d, Column: %d", line, column);
	}
	else
	{
		return g_strdup_printf ("<b>Line</b>: %d\n<i>Column</i>: %d", line, column);
	}
}

static void
add_source_mark_attributes (GtkSourceView *view)
{
	GdkRGBA color;
	GtkSourceMarkAttributes *attrs;

	attrs = gtk_source_mark_attributes_new ();

	gdk_rgba_parse (&color, "lightgreen");
	gtk_source_mark_attributes_set_background (attrs, &color);

	gtk_source_mark_attributes_set_icon_name (attrs, "list-add");

	g_signal_connect_object (attrs,
				 "query-tooltip-markup",
				 G_CALLBACK (mark_tooltip_func),
				 view,
				 0);

	gtk_source_view_set_mark_attributes (view, MARK_TYPE_1, attrs, 1);
	g_object_unref (attrs);

	attrs = gtk_source_mark_attributes_new ();

	gdk_rgba_parse (&color, "pink");
	gtk_source_mark_attributes_set_background (attrs, &color);

	gtk_source_mark_attributes_set_icon_name (attrs, "list-remove");

	g_signal_connect_object (attrs,
				 "query-tooltip-markup",
				 G_CALLBACK (mark_tooltip_func),
				 view,
				 0);

	gtk_source_view_set_mark_attributes (view, MARK_TYPE_2, attrs, 2);
	g_object_unref (attrs);
}

static void
background_pattern_selected_item_notify_cb (GtkDropDown *drop_down,
					    GParamSpec  *pspec,
					    TestWidget  *self)
{
	GObject *selected_item;
	const char *selected_item_str = NULL;

	selected_item = gtk_drop_down_get_selected_item (drop_down);
	if (selected_item != NULL)
	{
		selected_item_str = gtk_string_object_get_string (GTK_STRING_OBJECT (selected_item));
	}

	if (g_strcmp0 (selected_item_str, "Grid") == 0)
	{
		gtk_source_view_set_background_pattern (self->view,
		                                        GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID);
	}
	else
	{
		gtk_source_view_set_background_pattern (self->view,
		                                        GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE);
	}
}

static void
enable_snippets_toggled_cb (TestWidget     *self,
                            GtkCheckButton *button)
{
	gboolean enabled = gtk_check_button_get_active (button);
	gtk_source_view_set_enable_snippets (self->view, enabled);
}

static GtkSourceHoverProvider *
create_hover_provider (void)
{
	return g_object_new (TEST_TYPE_HOVER_PROVIDER, NULL);
}

static void
enable_hover_toggled_cb (TestWidget     *self,
                         GtkCheckButton *button)
{
	static GtkSourceHoverProvider *test_hover_provider;
	GtkSourceHover *hover = gtk_source_view_get_hover (self->view);
	gboolean enabled = gtk_check_button_get_active (button);

	if (test_hover_provider == NULL)
	{
		test_hover_provider = create_hover_provider ();
	}

	if (enabled)
		gtk_source_hover_add_provider (hover, test_hover_provider);
	else
		gtk_source_hover_remove_provider (hover, test_hover_provider);
}

static void
on_cursor_moved (TestWidget *self)
{
	GtkSourceAnnotation *annotation;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gint line;
	gchar *line_text;

	if (self->annotation_provider == NULL)
	{
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view));
	gtk_text_buffer_get_iter_at_mark (buffer,
	                                  &iter,
	                                  gtk_text_buffer_get_insert (buffer));

	line = gtk_text_iter_get_line (&iter);

	line_text = g_strdup_printf ("Line %d annotation", line + 1);

	annotation = gtk_source_annotation_new (line_text,
	                                        g_icon_new_for_string ("dialog-information-symbolic", NULL),
	                                        line,
	                                        GTK_SOURCE_ANNOTATION_STYLE_NONE);

	gtk_source_annotation_provider_remove_all (self->annotation_provider);

	gtk_source_annotation_provider_add_annotation (self->annotation_provider, annotation);

	g_object_unref (annotation);
	g_free (line_text);
}

static void
enable_annotations_toggled_cb (TestWidget     *self,
                               GtkCheckButton *button)
{
	GtkSourceAnnotations *annotations = gtk_source_view_get_annotations (self->view);
	gboolean enabled = gtk_check_button_get_active (button);

	if (self->annotation_provider == NULL)
	{
		self->annotation_provider = GTK_SOURCE_ANNOTATION_PROVIDER (g_object_new (TEST_TYPE_ANNOTATION_PROVIDER, NULL));
	}

	if (self->error_provider == NULL)
	{
		GtkSourceAnnotation *annotation;

		self->error_provider = GTK_SOURCE_ANNOTATION_PROVIDER (g_object_new (TEST_TYPE_ANNOTATION_PROVIDER, NULL));

		annotation = gtk_source_annotation_new ("Error Style!",
		                                        g_icon_new_for_string ("emblem-important-symbolic", NULL),
		                                        23,
		                                        GTK_SOURCE_ANNOTATION_STYLE_ERROR);

		gtk_source_annotation_provider_add_annotation (self->error_provider, annotation);
		g_object_unref (annotation);

		annotation = gtk_source_annotation_new ("Warning Style!",
		                                        g_icon_new_for_string ("dialog-warning-symbolic", NULL),
		                                        25,
		                                        GTK_SOURCE_ANNOTATION_STYLE_WARNING);

		gtk_source_annotation_provider_add_annotation (self->error_provider, annotation);
		g_object_unref (annotation);

		annotation = gtk_source_annotation_new ("Accent Style without an icon",
		                                        NULL,
		                                        21,
		                                        GTK_SOURCE_ANNOTATION_STYLE_ACCENT);

		gtk_source_annotation_provider_add_annotation (self->error_provider, annotation);
		g_object_unref (annotation);
	}

	if (enabled)
	{
		gtk_source_annotations_add_provider (annotations, self->annotation_provider);
		gtk_source_annotations_add_provider (annotations, self->error_provider);
	}
	else
	{
		gtk_source_annotations_remove_provider (annotations, self->annotation_provider);
		gtk_source_annotations_remove_provider (annotations, self->error_provider);
	}
}

static void
vim_checkbutton_toggled_cb (TestWidget     *self,
                            GtkCheckButton *button)
{
	g_assert (TEST_IS_WIDGET (self));
	g_assert (GTK_IS_CHECK_BUTTON (button));

	if (gtk_check_button_get_active (button))
	{
		if (!self->vim_controller)
		{
			self->vim_im_context = gtk_source_vim_im_context_new ();
			gtk_im_context_set_client_widget (self->vim_im_context,
			                                  GTK_WIDGET (self->view));
			g_object_bind_property (self->vim_im_context, "command-bar-text", self->command_bar, "label", 0);

			self->vim_controller = gtk_event_controller_key_new ();
			gtk_event_controller_set_propagation_phase (self->vim_controller, GTK_PHASE_CAPTURE);
			gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (self->vim_controller),
			                                         GTK_IM_CONTEXT (self->vim_im_context));
			gtk_widget_add_controller (GTK_WIDGET (self->view),
			                           self->vim_controller);
		}
	}
	else
	{
		if (self->vim_controller)
		{
			g_clear_object (&self->vim_im_context);
			gtk_widget_remove_controller (GTK_WIDGET (self->view),
			                              self->vim_controller);
			self->vim_controller = NULL;
		}
	}
}

static void
test_widget_dispose (GObject *object)
{
	TestWidget *self = TEST_WIDGET (object);

	g_clear_object (&self->buffer);
	g_clear_object (&self->file);

	G_OBJECT_CLASS (test_widget_parent_class)->dispose (object);
}

static void
test_widget_class_init (TestWidgetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = test_widget_dispose;

	gtk_widget_class_set_template_from_resource (widget_class,
						     "/org/gnome/gtksourceview/tests/ui/test-widget.ui");

	gtk_widget_class_bind_template_callback (widget_class, open_button_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, print_button_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, markup_button_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, highlight_syntax_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, highlight_matching_bracket_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, show_line_numbers_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, show_line_marks_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, show_right_margin_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, right_margin_position_value_changed_cb);
	gtk_widget_class_bind_template_callback (widget_class, bottom_margin_value_changed_cb);
	gtk_widget_class_bind_template_callback (widget_class, highlight_current_line_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, wrap_lines_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, auto_indent_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, indent_spaces_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, tab_width_value_changed_cb);
	gtk_widget_class_bind_template_callback (widget_class, backward_string_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, forward_string_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, smart_home_end_selected_notify_cb);
	gtk_widget_class_bind_template_callback (widget_class, enable_snippets_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, enable_hover_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, enable_annotations_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, vim_checkbutton_toggled_cb);

	gtk_widget_class_bind_template_child (widget_class, TestWidget, view);
	gtk_widget_class_bind_template_child (widget_class, TestWidget, map);
	gtk_widget_class_bind_template_child (widget_class, TestWidget, show_top_border_window_checkbutton);
	gtk_widget_class_bind_template_child (widget_class, TestWidget, show_map_checkbutton);
	gtk_widget_class_bind_template_child (widget_class, TestWidget, draw_spaces_checkbutton);
	gtk_widget_class_bind_template_child (widget_class, TestWidget, smart_backspace_checkbutton);
	gtk_widget_class_bind_template_child (widget_class, TestWidget, indent_width_checkbutton);
	gtk_widget_class_bind_template_child (widget_class, TestWidget, indent_width_spinbutton);
	gtk_widget_class_bind_template_child (widget_class, TestWidget, cursor_position_info);
	gtk_widget_class_bind_template_child (widget_class, TestWidget, chooser_button);
	gtk_widget_class_bind_template_child (widget_class, TestWidget, background_pattern);
	gtk_widget_class_bind_template_child (widget_class, TestWidget, scrolledwindow1);
	gtk_widget_class_bind_template_child (widget_class, TestWidget, vim_checkbutton);
	gtk_widget_class_bind_template_child (widget_class, TestWidget, command_bar);

	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_w, GDK_CONTROL_MASK, "window.close", NULL);
}

static void
show_top_border_window_toggled_cb (GtkCheckButton *checkbutton,
                                   TestWidget     *self)
{
	gint size;

	size = gtk_check_button_get_active (checkbutton) ? 20 : 0;

	if (self->top == NULL)
	{
		self->top = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_text_view_set_gutter (GTK_TEXT_VIEW (self->view),
		                          GTK_TEXT_WINDOW_TOP,
		                          GTK_WIDGET (self->top));
	}

	gtk_widget_set_size_request (self->top, -1, size);
}

static gboolean
boolean_to_scrollbar_policy (GBinding     *binding,
			     const GValue *from_value,
			     GValue       *to_value,
			     gpointer      user_data)
{
	if (g_value_get_boolean (from_value))
		g_value_set_enum (to_value, GTK_POLICY_EXTERNAL);
	else
		g_value_set_enum (to_value, GTK_POLICY_AUTOMATIC);
	return TRUE;
}

static void
test_widget_init (TestWidget *self)
{
	GtkSourceSpaceDrawer *space_drawer;
	GtkSourceStyleScheme *style_scheme;
	GFile *file;

	self = test_widget_get_instance_private (self);

	gtk_widget_init_template (GTK_WIDGET (self));

	self->buffer = GTK_SOURCE_BUFFER (
		gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view)));

	g_signal_connect_swapped (self->buffer,
	                          "notify::language",
	                          G_CALLBACK (update_cursor_position_info),
	                          self);

	g_object_ref (self->buffer);

	g_signal_connect (self->show_top_border_window_checkbutton,
			  "toggled",
			  G_CALLBACK (show_top_border_window_toggled_cb),
			  self);

	g_signal_connect_swapped (self->indent_width_checkbutton,
				  "toggled",
				  G_CALLBACK (update_indent_width),
				  self);

	g_signal_connect_swapped (self->indent_width_spinbutton,
				  "value-changed",
				  G_CALLBACK (update_indent_width),
				  self);

	g_signal_connect (self->buffer,
			  "mark-set",
			  G_CALLBACK (mark_set_cb),
			  self);

	g_signal_connect_swapped (self->buffer,
				  "changed",
				  G_CALLBACK (update_cursor_position_info),
				  self);

	g_signal_connect (self->buffer,
			  "bracket-matched",
			  G_CALLBACK (bracket_matched_cb),
			  NULL);

	add_source_mark_attributes (self->view);

	g_signal_connect (self->view,
			  "line-mark-activated",
			  G_CALLBACK (line_mark_activated_cb),
			  self);

	g_object_bind_property (self->chooser_button,
	                        "style-scheme",
	                        self->buffer,
	                        "style-scheme",
	                        G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

	style_scheme = gtk_source_style_scheme_manager_get_scheme (
		gtk_source_style_scheme_manager_get_default (),
		"Adwaita");
	gtk_source_buffer_set_style_scheme (self->buffer, style_scheme);

	g_object_bind_property (self->show_map_checkbutton,
	                        "active",
	                        self->map,
	                        "visible",
	                        G_BINDING_SYNC_CREATE);
	g_object_bind_property_full (self->show_map_checkbutton,
				     "active",
				     self->scrolledwindow1,
				     "vscrollbar-policy",
				     G_BINDING_SYNC_CREATE,
				     boolean_to_scrollbar_policy, NULL,
				     NULL, NULL);

	g_object_bind_property (self->smart_backspace_checkbutton,
	                        "active",
	                        self->view,
	                        "smart-backspace",
	                        G_BINDING_SYNC_CREATE);

	g_signal_connect (self->background_pattern,
	                  "notify::selected-item",
	                  G_CALLBACK (background_pattern_selected_item_notify_cb),
	                  self);

	space_drawer = gtk_source_view_get_space_drawer (self->view);
	g_object_bind_property (self->draw_spaces_checkbutton, "active",
				space_drawer, "enable-matrix",
				G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	g_signal_connect_swapped (self->buffer,
				"notify::cursor-position",
				G_CALLBACK (on_cursor_moved),
				self);

	g_print("connected!!\n");

	if (cmd_filename)
		file = g_file_new_for_commandline_arg (cmd_filename);
	else
		file = g_file_new_for_path (TOP_SRCDIR "/gtksourceview/gtksourcebuffer.c");

	open_file (self, file);
	g_object_unref (file);
}

static TestWidget *
test_widget_new (void)
{
	return g_object_new (test_widget_get_type (), NULL);
}

static gboolean
test_hover_provider_populate (GtkSourceHoverProvider  *provider,
                              GtkSourceHoverContext   *context,
                              GtkSourceHoverDisplay   *display,
                              GError                 **error)
{
	GtkTextIter begin, end;

	if (gtk_source_hover_context_get_bounds (context, &begin, &end))
	{
		gchar *text = gtk_text_iter_get_slice (&begin, &end);
		GtkWidget *label = gtk_label_new (text);
		gtk_source_hover_display_append (display, label);
		g_free (text);
	}

	return TRUE;
}

static void
hover_provider_iface_init (GtkSourceHoverProviderInterface *iface)
{
	iface->populate = test_hover_provider_populate;
}

static void
test_hover_provider_class_init (TestHoverProviderClass *klass)
{
}

static void
test_hover_provider_init (TestHoverProvider *self)
{
}

static void
test_annotation_provider_dispose (GObject *object)
{
	G_OBJECT_CLASS (test_annotation_provider_parent_class)->dispose (object);
}

static void
test_annotation_provider_populate_hover_async (GtkSourceAnnotationProvider  *self,
                                               GtkSourceAnnotation          *annotation,
                                               GtkSourceHoverDisplay        *display,
                                               GCancellable                 *cancellable,
                                               GAsyncReadyCallback           callback,
                                               gpointer                      user_data)
{
	GTask *task = NULL;

	g_assert (GTK_SOURCE_IS_ANNOTATION_PROVIDER (self));
	g_assert (GTK_SOURCE_IS_HOVER_DISPLAY (display));
	g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_source_tag (task, gtk_source_annotation_provider_populate_hover_async);

	gtk_source_hover_display_append (display,
	                                 g_object_new (GTK_TYPE_LABEL,
	                                               "label", gtk_source_annotation_get_description (annotation),
	                                               "margin-start", 12,
	                                               "margin-end", 12,
	                                               NULL));

	g_task_return_boolean (task, TRUE);

	g_object_unref (task);
}

static gboolean
test_annotation_provider_populate_hover_finish (GtkSourceAnnotationProvider  *self,
                                                GAsyncResult                 *result,
                                                GError                      **error)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION_PROVIDER (self), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
test_annotation_provider_class_init (TestAnnotationProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkSourceAnnotationProviderClass *provider_class = GTK_SOURCE_ANNOTATION_PROVIDER_CLASS (klass);

	object_class->dispose = test_annotation_provider_dispose;

	provider_class->populate_hover_async = test_annotation_provider_populate_hover_async;
	provider_class->populate_hover_finish = test_annotation_provider_populate_hover_finish;
}

static void
test_annotation_provider_init (TestAnnotationProvider *self)
{
}

static void
setup_search_paths (void)
{
	GtkSourceSnippetManager *snippets;
	GtkSourceStyleSchemeManager *styles;
	GtkSourceLanguageManager *languages;
	static const gchar *snippets_path[] = { TOP_SRCDIR"/data/snippets", NULL };
	static const gchar *langs_path[] = { TOP_SRCDIR"/data/language-specs", NULL };

	snippets = gtk_source_snippet_manager_get_default ();
	gtk_source_snippet_manager_set_search_path (snippets, snippets_path);

	/* Allow use of system styles, but prefer in-tree */
	styles = gtk_source_style_scheme_manager_get_default ();
	gtk_source_style_scheme_manager_prepend_search_path (styles, TOP_SRCDIR"/data/styles");

	languages = gtk_source_language_manager_get_default ();
	gtk_source_language_manager_set_search_path (languages, langs_path);
}

int
main (int argc, char *argv[])
{
	GtkWidget *window;
	TestWidget *test_widget;

	main_loop = g_main_loop_new (NULL, FALSE);

	if (argc == 2 && g_file_test (argv[1], G_FILE_TEST_IS_REGULAR))
	{
		cmd_filename = argv[1];
	}

	g_set_prgname ("test-widget");

	gtk_init ();
	gtk_source_init ();
	setup_search_paths ();

	window = gtk_window_new ();
	gtk_window_set_default_size (GTK_WINDOW (window), 900, 600);
	gtk_window_set_title (GTK_WINDOW (window), "GtkSourceView Test");

	g_signal_connect_swapped (window, "destroy", G_CALLBACK (g_main_loop_quit), main_loop);

	test_widget = test_widget_new ();
	gtk_window_set_child (GTK_WINDOW (window), GTK_WIDGET (test_widget));

	gtk_window_present (GTK_WINDOW (window));

	g_main_loop_run (main_loop);

	gtk_source_finalize ();

	return 0;
}
