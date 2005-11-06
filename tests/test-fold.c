/*  test-fold.c
 *
 *  Copyright (C) 2005 - Jeroen Zwartepoorte <jeroen.zwartepoorte@gmail.com>
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

#include <string.h>
#include <gtk/gtk.h>
#include <gtksourceview/gtksourceview.h>

static const char *text =
	"Test case 1\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15";

static GtkSourceFold *
add_fold (GtkSourceBuffer *buffer,
	  int start_line,
	  int end_line)
{
	GtkTextIter begin, end;

	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer),
					  &begin, start_line);
	gtk_text_iter_forward_to_line_end (&begin);
	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer),
					  &end, end_line + 1);
	return gtk_source_buffer_add_fold (buffer, &begin, &end);
}

static void
reset_buffer (GtkSourceBuffer *buffer)
{
	GtkTextIter start, end;
	
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &start, &end);
	
	gtk_text_buffer_insert_at_cursor (GTK_TEXT_BUFFER (buffer),
					  text, strlen (text));
}

static void
test1 (GtkSourceBuffer *buffer)
{
	g_assert (add_fold (buffer, 5, 10) != NULL);
	g_assert (add_fold (buffer, 6, 8) != NULL);
}

static void
test2 (GtkSourceBuffer *buffer)
{
	g_assert (add_fold (buffer, 5, 10) != NULL);
	g_assert (add_fold (buffer, 6, 7) != NULL);
	g_assert (add_fold (buffer, 8, 9) != NULL);
}

static void
test3 (GtkSourceBuffer *buffer)
{
	g_assert (add_fold (buffer, 5, 10) != NULL);
	/* fails, intersects at begin. */
	g_assert (add_fold (buffer, 4, 6) == NULL);
}

static void
test4 (GtkSourceBuffer *buffer)
{
	g_assert (add_fold (buffer, 5, 10) != NULL);
	/* fails, same beginpoint. */
	g_assert (add_fold (buffer, 5, 6) == NULL);
}

static void
test5 (GtkSourceBuffer *buffer)
{
	g_assert (add_fold (buffer, 5, 10) != NULL);
	/* fails, intersects at end. */
	g_assert (add_fold (buffer, 7, 11) == NULL);
}

static void
test6 (GtkSourceBuffer *buffer)
{
	g_assert (add_fold (buffer, 5, 10) != NULL);
	/* fails, same endpoint. */
	g_assert (add_fold (buffer, 7, 10) == NULL);
}

static void
test7 (GtkSourceBuffer *buffer)
{
	g_assert (add_fold (buffer, 5, 10) != NULL);
	g_assert (add_fold (buffer, 3, 12) != NULL);
}

static void
prepare8 (GtkSourceBuffer *buffer)
{
	reset_buffer (buffer);
	g_assert (add_fold (buffer, 5, 10) != NULL);
	g_assert (add_fold (buffer, 3, 12) != NULL);
}

static void
test8 (GtkSourceBuffer *buffer)
{
	/* test1 */
	prepare8 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	g_assert (add_fold (buffer, 6, 8) != NULL);

	/* test2 */
	prepare8 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	g_assert (add_fold (buffer, 6, 7) != NULL);
	g_assert (add_fold (buffer, 8, 9) != NULL);
	
	/* test3 */
	prepare8 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, intersects at begin. */
	g_assert (add_fold (buffer, 4, 6) == NULL);

	/* test4 */
	prepare8 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, same beginpoint. */
	g_assert (add_fold (buffer, 5, 6) == NULL);

	/* test5 */
	prepare8 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, intersects at end. */
	g_assert (add_fold (buffer, 7, 11) == NULL);

	/* test6 */
	prepare8 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, same endpoint. */
	g_assert (add_fold (buffer, 7, 10) == NULL);
}

static void
prepare9 (GtkSourceBuffer *buffer)
{
	reset_buffer (buffer);
	g_assert (add_fold (buffer, 0, 2) != NULL);
	g_assert (add_fold (buffer, 5, 10) != NULL);
}

static void
test9 (GtkSourceBuffer *buffer)
{
	/* test1 */
	prepare9 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	g_assert (add_fold (buffer, 6, 8) != NULL);

	/* test2 */
	prepare9 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	g_assert (add_fold (buffer, 6, 7) != NULL);
	g_assert (add_fold (buffer, 8, 9) != NULL);
	
	/* test3 */
	prepare9 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, intersects at begin. */
	g_assert (add_fold (buffer, 4, 6) == NULL);

	/* test4 */
	prepare9 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, same beginpoint. */
	g_assert (add_fold (buffer, 5, 6) == NULL);

	/* test5 */
	prepare9 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, intersects at end. */
	g_assert (add_fold (buffer, 7, 11) == NULL);

	/* test6 */
	prepare9 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, same endpoint. */
	g_assert (add_fold (buffer, 7, 10) == NULL);
	
	/* test7 */
	prepare9 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	g_assert (add_fold (buffer, 3, 12) != NULL);
}

static void
prepare10 (GtkSourceBuffer *buffer)
{
	reset_buffer (buffer);
	g_assert (add_fold (buffer, 5, 10) != NULL);
	g_assert (add_fold (buffer, 13, 14) != NULL);
}

static void
test10 (GtkSourceBuffer *buffer)
{
	/* test1 */
	prepare10 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	g_assert (add_fold (buffer, 6, 8) != NULL);

	/* test2 */
	prepare10 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	g_assert (add_fold (buffer, 6, 7) != NULL);
	g_assert (add_fold (buffer, 8, 9) != NULL);
	
	/* test3 */
	prepare10 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, intersects at begin. */
	g_assert (add_fold (buffer, 4, 6) == NULL);

	/* test4 */
	prepare10 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, same beginpoint. */
	g_assert (add_fold (buffer, 5, 6) == NULL);

	/* test5 */
	prepare10 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, intersects at end. */
	g_assert (add_fold (buffer, 7, 11) == NULL);

	/* test6 */
	prepare10 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, same endpoint. */
	g_assert (add_fold (buffer, 7, 10) == NULL);
	
	/* test7 */
	prepare10 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	g_assert (add_fold (buffer, 3, 12) != NULL);
}

static void
prepare11 (GtkSourceBuffer *buffer)
{
	reset_buffer (buffer);
	g_assert (add_fold (buffer, 0, 2) != NULL);
	g_assert (add_fold (buffer, 5, 10) != NULL);
	g_assert (add_fold (buffer, 13, 14) != NULL);
}

static void
test11 (GtkSourceBuffer *buffer)
{
	/* test1 */
	prepare11 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	g_assert (add_fold (buffer, 6, 8) != NULL);

	/* test2 */
	prepare11 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	g_assert (add_fold (buffer, 6, 7) != NULL);
	g_assert (add_fold (buffer, 8, 9) != NULL);
	
	/* test3 */
	prepare11 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, intersects at begin. */
	g_assert (add_fold (buffer, 4, 6) == NULL);

	/* test4 */
	prepare11 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, same beginpoint. */
	g_assert (add_fold (buffer, 5, 6) == NULL);

	/* test5 */
	prepare11 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, intersects at end. */
	g_assert (add_fold (buffer, 7, 11) == NULL);

	/* test6 */
	prepare11 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	/* fails, same endpoint. */
	g_assert (add_fold (buffer, 7, 10) == NULL);
	
	/* test7 */
	prepare11 (buffer);
	/* fails, fold already exists. */
	g_assert (add_fold (buffer, 5, 10) == NULL);
	g_assert (add_fold (buffer, 3, 12) != NULL);
}

static void
run_tests (GtkSourceBuffer *buffer)
{
	g_message ("Starting test...");
	
	reset_buffer (buffer);
	test1 (buffer);
	reset_buffer (buffer);
	test2 (buffer);
	reset_buffer (buffer);
	test3 (buffer);
	reset_buffer (buffer);
	test4 (buffer);
	reset_buffer (buffer);
	test5 (buffer);
	reset_buffer (buffer);
	test6 (buffer);
	reset_buffer (buffer);
	test7 (buffer);
	reset_buffer (buffer);
	test8 (buffer);
	reset_buffer (buffer);
	test9 (buffer);
	reset_buffer (buffer);
	test10 (buffer);
	reset_buffer (buffer);
	test11 (buffer);
	
	g_message ("Test finished succesfully!");
}

int
main (int argc, char *argv[])
{
	GtkSourceBuffer *buffer;
	
	gtk_init (&argc, &argv);
	
	buffer = gtk_source_buffer_new (NULL);

	run_tests (buffer);

	return 0;
}
