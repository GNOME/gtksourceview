/* test-view-tabs.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gtksourceview/gtksource.h>

static void
test_tab_width (gconstpointer data)
{
	GtkSourceView *view;
	PangoFontDescription *font;
	PangoLayout *layout;
	PangoContext *context;
	gboolean round_positions = GPOINTER_TO_INT (data);
	int space_width;

	view = g_object_ref_sink (GTK_SOURCE_VIEW (gtk_source_view_new ()));
	context = gtk_widget_get_pango_context (GTK_WIDGET (view));
	font = pango_font_description_from_string ("Monospace");
	pango_font_description_set_absolute_size (font, 13.5 * PANGO_SCALE);
	pango_context_set_font_description (context, font);
	pango_context_set_round_glyph_positions (context, round_positions);

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), " ");
	pango_layout_get_size (layout, &space_width, NULL);
	g_assert_cmpint (space_width, >, 0);
	if (!round_positions)
	{
		g_assert_cmpint (space_width % PANGO_SCALE, !=, 0);
	}

	/* Change widths in both directions, including returning to the default.
	 * Tabs at an exact column boundary must advance to the next stop, even
	 * when that boundary is not on an integer pixel.
	 */
	for (guint step = 0; step < 64; step++)
	{
		PangoTabArray *tabs;
		guint width = step < 32 ? step + 1 : 64 - step;

		gtk_source_view_set_tab_width (view, width);
		g_assert_cmpuint (gtk_source_view_get_tab_width (view), ==, width);
		tabs = gtk_text_view_get_tabs (GTK_TEXT_VIEW (view));
		g_assert_nonnull (tabs);
		pango_layout_set_tabs (layout, tabs);

		for (guint prefix = 0; prefix <= 2 * width + 1; prefix++)
		{
			char *digits = g_strnfill (prefix, '1');
			char *text = g_strconcat (digits, "\t\tX", NULL);

			pango_layout_set_text (layout, text, -1);

			for (guint n_tabs = 1; n_tabs <= 2; n_tabs++)
			{
				PangoRectangle pos;
				guint column = (prefix / width + n_tabs) * width;

				pango_layout_index_to_pos (layout, prefix + n_tabs, &pos);
				g_test_message ("width=%u prefix=%u tabs=%u", width, prefix, n_tabs);
				g_assert_cmpint (pos.x, ==, column * space_width);
			}

			g_free (digits);
			g_free (text);
		}

		pango_tab_array_free (tabs);
	}

	g_object_unref (layout);
	pango_font_description_free (font);
	g_object_unref (view);
}

static void
test_right_margin (void)
{
	PangoLayout *layout;
	GtkWidget *window;
	GtkWidget *view;
	GtkSourceBuffer *buffer;
	GtkSourceStyleScheme *scheme;
	const guint columns[] = { 5, 20, 8 };

	window = gtk_window_new ();
	view = gtk_source_view_new ();
	gtk_window_set_default_size (GTK_WINDOW (window), 640, 240);
	gtk_window_set_child (GTK_WINDOW (window), view);
	gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 7);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	scheme = gtk_source_style_scheme_manager_get_scheme (gtk_source_style_scheme_manager_get_default (),
	                                                     "classic");
	g_assert_nonnull (scheme);
	gtk_source_buffer_set_style_scheme (buffer, scheme);
	gtk_source_view_set_show_right_margin (GTK_SOURCE_VIEW (view), TRUE);

	gtk_window_present (GTK_WINDOW (window));
	gtk_test_widget_wait_for_draw (window);
	layout = gtk_widget_create_pango_layout (view, NULL);

	for (guint i = 0; i < G_N_ELEMENTS (columns); i++)
	{
		char *text = g_strnfill (columns[i], '_');
		GskRenderNode *node;
		GskRenderNode *margin;
		GtkSnapshot *snapshot;
		GdkRectangle visible;
		graphene_rect_t bounds;
		int width;

		pango_layout_set_text (layout, text, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);
		gtk_source_view_set_right_margin_position (GTK_SOURCE_VIEW (view), columns[i]);
		gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &visible);

		snapshot = gtk_snapshot_new ();
		GTK_WIDGET_GET_CLASS (view)->snapshot (view, snapshot);
		node = gtk_snapshot_free_to_node (snapshot);
		g_assert_nonnull (node);

		/* The margin is drawn before the text view's contents. */
		margin = node;
		if (gsk_render_node_get_node_type (margin) == GSK_CONTAINER_NODE)
		{
			margin = gsk_container_node_get_child (margin, 0);
		}
		g_assert_cmpint (gsk_render_node_get_node_type (margin), ==, GSK_COLOR_NODE);
		gsk_render_node_get_bounds (margin, &bounds);
		g_assert_cmpfloat (bounds.origin.x, ==, width + 7 - visible.x);
		g_assert_cmpfloat (bounds.size.width, ==, 1);
		g_assert_cmpfloat (bounds.size.height, ==, visible.height);

		gsk_render_node_unref (node);
		g_free (text);
	}

	g_object_unref (layout);
	gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int   argc,
      char *argv[])
{
	int ret;

	/* We only inspect render nodes, so avoid initializing GPU drivers. */
	g_setenv ("GSK_RENDERER", "cairo", TRUE);

	gtk_test_init (&argc, &argv);
	gtk_source_init ();

	g_test_add_data_func ("/view/tab-width/fractional", GINT_TO_POINTER (FALSE), test_tab_width);
	g_test_add_data_func ("/view/tab-width/rounded", GINT_TO_POINTER (TRUE), test_tab_width);
	g_test_add_func ("/view/right-margin/pixels", test_right_margin);

	ret = g_test_run ();

	gtk_source_finalize ();

	return ret;
}
