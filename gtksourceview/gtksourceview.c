/* gtksourceview
*  Copyright (C) 2001
*  Mikael Hermansson<tyan@linux.se>
*  Chris Phelps <chicane@reninet.com>
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
*  You should have received a copy of the GNU Library General Public License*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <pango/pango-tabs.h>

#include "gtksourceview.h"

#define GUTTER_PAD 4
#define GUTTER_PIXMAP 16
#define TAB_STOP 4

enum
{
  UNDO,
  REDO,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static void gtk_source_view_class_init (GtkSourceViewClass *klass);
static void gtk_source_view_init (GtkSourceView *view);
static void gtk_source_view_finalize(GObject *object);
static void hash_remove_func(gpointer key, gpointer value, gpointer user_data);

static void gtk_source_view_real_undo(GtkSourceView *view);
static void gtk_source_view_real_redo(GtkSourceView *view);
static void gtk_source_view_init (GtkSourceView *text);
static void gtk_source_view_class_init (GtkSourceViewClass *text);
static gint gtk_source_view_expose (GtkWidget *widget, GdkEventExpose *ev);
static gint gtk_source_view_key_press (GtkWidget *widget, GdkEventKey *ev);

static gint get_tab_stop_width(GtkWidget *widget, gint tab_stop);

static gint get_lines (GtkTextView  *text_view, gint first_y, gint last_y, GArray *buffer_coords, gint *countp);
static gboolean foreach_character(gunichar ch, gpointer data);
static gint find_correct_bracket(GtkTextBuffer *buf, GtkTextIter *iter);
static gint recompute_gutter_width(GtkTextView *text_view);
static void check_line_change_insert_cb(GtkTextBuffer *buffer, GtkTextIter *iter, const gchar *text, gint len, GtkTextView *view);
static void check_line_change_delete_cb(GtkTextBuffer *buffer, GtkTextIter *start, GtkTextIter *end, GtkTextView *view);
static void check_line_change_delete_cb_after(GtkTextBuffer *buffer, GtkTextIter *start, GtkTextIter *end, GtkTextView *view);

static GObjectClass *parent_class = NULL;

GType
gtk_source_view_get_type (void)
{
    static GType our_type = 0;
    if (our_type == 0)
    {
        static const GTypeInfo our_info =
        {
          sizeof (GtkSourceViewClass),
          (GBaseInitFunc) NULL,
          (GBaseFinalizeFunc) NULL,
          (GClassInitFunc) gtk_source_view_class_init,
          NULL, /* class_finalize */
          NULL, /* class_data */
          sizeof (GtkSourceView),
          0, /* n_preallocs */
          (GInstanceInitFunc) gtk_source_view_init
        };
        our_type = g_type_register_static (GTK_TYPE_TEXT_VIEW, "GtkSourceView", &our_info, 0);
    }
    return our_type;
}

static void 
gtk_source_view_class_init(GtkSourceViewClass *klass)
{
    GObjectClass *object_class;
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GtkBindingSet *binding_set;
 
    object_class = (GObjectClass*) klass;
    object_class->finalize = gtk_source_view_finalize;
    parent_class = g_type_class_peek_parent (klass);

    widget_class->expose_event = gtk_source_view_expose;
  
    klass->undo = gtk_source_view_real_undo;
    klass->redo = gtk_source_view_real_redo;

    signals[UNDO] =
      gtk_signal_new ("undo",
                      GTK_RUN_LAST | GTK_RUN_ACTION,
                      GTK_CLASS_TYPE (object_class),
                      GTK_SIGNAL_OFFSET (GtkSourceViewClass, undo),
                      gtk_marshal_VOID__VOID,
                      GTK_TYPE_NONE, 0);
    signals[REDO] =
      gtk_signal_new ("redo",
                      GTK_RUN_LAST | GTK_RUN_ACTION,
                      GTK_CLASS_TYPE (object_class),
                      GTK_SIGNAL_OFFSET (GtkSourceViewClass, redo),
                      gtk_marshal_VOID__VOID,
                      GTK_TYPE_NONE, 0);

    binding_set = gtk_binding_set_by_class(klass);
    gtk_binding_entry_add_signal (binding_set, GDK_z, GDK_CONTROL_MASK, "undo", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_r, GDK_CONTROL_MASK, "redo", 0);
}

static void
gtk_source_view_init(GtkSourceView *view)
{
    PangoTabArray *tabs;

    view->show_line_numbers = TRUE;
    view->show_line_pixmaps = TRUE;
    view->line_number_space = 0;
    view->pixmap_cache = g_hash_table_new(g_str_hash, g_str_equal);
}

GtkWidget *
gtk_source_view_new()
{
    GtkWidget *widget;
    GtkTextBuffer *buffer;

    buffer = gtk_source_buffer_new(NULL);
    widget = gtk_source_view_new_with_buffer(GTK_SOURCE_BUFFER(buffer));
    return(widget);
}

GtkWidget *
gtk_source_view_new_with_buffer(GtkSourceBuffer *buffer)
{
    GtkWidget *view;
    view = g_object_new(gtk_source_view_get_type(), NULL);
    gtk_text_view_set_buffer(GTK_TEXT_VIEW(view), GTK_TEXT_BUFFER(buffer));
    g_signal_connect_closure(G_OBJECT(buffer),"insert_text", g_cclosure_new((GCallback)check_line_change_insert_cb, (gpointer)view, NULL), TRUE);
    g_signal_connect_closure(G_OBJECT(buffer),"delete_range", g_cclosure_new((GCallback)check_line_change_delete_cb, (gpointer)view, NULL), FALSE);
    g_signal_connect_closure(G_OBJECT(buffer),"delete_range", g_cclosure_new((GCallback)check_line_change_delete_cb_after, (gpointer)view, NULL), TRUE);
    return(view);
}

static void
gtk_source_view_finalize(GObject *object)
{
    GtkSourceView *view;

    g_return_if_fail(object != NULL);
    g_return_if_fail(GTK_IS_SOURCE_VIEW(object));

    view = GTK_SOURCE_VIEW(object);
    if(view->pixmap_cache)
    {
        g_hash_table_foreach_remove(view->pixmap_cache, (GHRFunc)hash_remove_func, NULL);
        g_hash_table_destroy(view->pixmap_cache);
    }
}

static void
hash_remove_func(gpointer key, gpointer value, gpointer user_data)
{
    g_object_unref(G_OBJECT(value));
}

void 
gtk_source_view_real_undo(GtkSourceView *view)
{
    g_return_if_fail(GTK_IS_SOURCE_VIEW(view));

    gtk_source_buffer_undo(GTK_SOURCE_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view))));
}

void
gtk_source_view_real_redo(GtkSourceView *view)
{
    g_return_if_fail(GTK_IS_SOURCE_VIEW(view));

    gtk_source_buffer_redo(GTK_SOURCE_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view))));
}

gint 
gtk_source_view_expose(GtkWidget *widget, GdkEventExpose *ev)
{
    GdkWindow *win=0;
    GArray *pixels=0;
    gint pos=0;
    gint y1 = 0;
    gint y2 = 0;
    gint i = 0;
    gint num = 0;
    gint count = 0;
    GtkTextView *tw = GTK_TEXT_VIEW(widget);
    GtkSourceView *view = GTK_SOURCE_VIEW(widget);
    PangoLayout *layout = NULL;
    GdkPixbuf *pixbuf;
    gchar *str = NULL;
    gint lines = 0;
    gint window_width = 0;
    gint text_width = 0;
    const gchar *marker;

    win = gtk_text_view_get_window (tw, GTK_TEXT_WINDOW_LEFT);
    if(ev->window != win || !GTK_SOURCE_VIEW(tw)->show_line_numbers)
        return (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, ev);

    if(view->show_line_numbers || view->show_line_pixmaps)
    {
        pixels = g_array_new (FALSE, FALSE, sizeof (gint));
        y1 = ev->area.y;
        y2 = y1 + ev->area.height;
        /* get the extents of the line printing */
        gtk_text_view_window_to_buffer_coords (tw, GTK_TEXT_WINDOW_LEFT, 0, y1, NULL, &y1);
        gtk_text_view_window_to_buffer_coords (tw, GTK_TEXT_WINDOW_LEFT, 0, y2, NULL, &y2);

        if(view->show_line_numbers)
        {
            layout = gtk_widget_create_pango_layout (GTK_WIDGET(tw), "");
            text_width = view->line_number_space - (GUTTER_PAD * 2);
            pango_layout_set_width(layout, text_width);
            pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
        }
        for(num = get_lines(tw, y1, y2, pixels, &count); i < count; i++, num++)
        {
            gtk_text_view_buffer_to_window_coords (tw, GTK_TEXT_WINDOW_LEFT, 0, g_array_index (pixels, gint, i), NULL, &pos);
            if(view->show_line_numbers)
            {
                str = g_strdup_printf ("%d", num + 1);
                pango_layout_set_text(layout, str, -1);
                gdk_draw_layout_with_colors(win, widget->style->fg_gc [widget->state], text_width + GUTTER_PAD, pos, layout, &GTK_WIDGET(tw)->style->fg[GTK_STATE_NORMAL], &GTK_WIDGET(tw)->style->bg[GTK_STATE_NORMAL]);
                g_free (str);
            }
            if(view->show_line_pixmaps)
            {
                gint marker_count = 0;
                GList *list = NULL;
                GList *iter = NULL;

                if((marker_count = gtk_source_buffer_line_has_markers(GTK_SOURCE_BUFFER(tw->buffer), num + 1)))
                {
                    gboolean kill_pixbuf = FALSE;

                    list = (GList *)gtk_source_buffer_line_get_markers(GTK_SOURCE_BUFFER(tw->buffer), num + 1);
                    if(marker_count > 1)
                    {
                        GdkPixbuf *composite;

                        list = g_list_copy(list);
                        list = g_list_reverse(list);
                        pixbuf = gtk_source_view_get_pixbuf(GTK_SOURCE_VIEW(tw), (const gchar *)list->data);
                        if(pixbuf)
                        {
                            pixbuf = gdk_pixbuf_copy(pixbuf);
                            g_object_ref(pixbuf);
                            kill_pixbuf = TRUE;
                            for(iter = list->next; iter; iter = iter->next)
                            {
                                if((composite = gtk_source_view_get_pixbuf(GTK_SOURCE_VIEW(tw), (const gchar *)iter->data)))
                                {
                                    gint width;
                                    gint height;
                                    gint comp_width;
                                    gint comp_height;
                                    width = gdk_pixbuf_get_width(pixbuf),
                                    height = gdk_pixbuf_get_height(pixbuf),
                                    comp_width = gdk_pixbuf_get_width(composite),
                                    comp_height = gdk_pixbuf_get_height(composite),
                                    gdk_pixbuf_composite((const GdkPixbuf *)composite,
                                                         pixbuf,
                                                         0, 0,
                                                         width, height,
                                                         0, 0,
                                                         width/comp_width, height/comp_height,
                                                         GDK_INTERP_BILINEAR,
                                                         127);
                                }
                            }
                            g_print("\n");
                        }
                        g_list_free(list);
                    }
                    else
                    {
                        pixbuf = gtk_source_view_get_pixbuf(GTK_SOURCE_VIEW(tw), (const gchar *)list->data);
                    }

                    if(pixbuf && gdk_pixbuf_get_has_alpha(pixbuf))
                        gdk_pixbuf_render_to_drawable_alpha(pixbuf, GDK_DRAWABLE(win), 0, 0,
                                                            text_width + (GUTTER_PAD * 2), pos,
                                                            gdk_pixbuf_get_width(pixbuf),
                                                            gdk_pixbuf_get_height(pixbuf),
                                                            GDK_PIXBUF_ALPHA_BILEVEL,
                                                            127,
                                                            GDK_RGB_DITHER_NORMAL,
                                                            0, 0);
                     else if(pixbuf)
                        gdk_pixbuf_render_to_drawable(pixbuf, GDK_DRAWABLE(win),
                                                      GTK_WIDGET(tw)->style->bg_gc[GTK_STATE_NORMAL],
                                                      0, 0, text_width + (GUTTER_PAD * 2), pos,
                                                      gdk_pixbuf_get_width(pixbuf),
                                                      gdk_pixbuf_get_height(pixbuf),
                                                      GDK_RGB_DITHER_NORMAL,
                                                      0, 0);
                    if(kill_pixbuf) g_object_unref(pixbuf);
                }
            }
        }
        g_array_free (pixels, TRUE); 
        if(view->show_line_numbers) g_object_unref(G_OBJECT (layout));
        return(TRUE);
    }
    return(FALSE);
}

void 
gtk_source_view_set_show_line_numbers(GtkSourceView *view, gboolean show)
{
    g_return_if_fail(view != NULL);
    g_return_if_fail (GTK_IS_SOURCE_VIEW(view));

    view->show_line_numbers = show;
    if(!view->show_line_numbers && !view->show_line_pixmaps)
    {
        gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(view), GTK_TEXT_WINDOW_LEFT, 0);
    }
    else
    {
        recompute_gutter_width(GTK_TEXT_VIEW(view));
    }
}

gboolean
gtk_source_view_get_show_line_numbers(GtkSourceView *view)
{
    g_return_val_if_fail(view != NULL, FALSE);
    g_return_val_if_fail(GTK_IS_SOURCE_VIEW(view), FALSE);

    return(view->show_line_numbers);
}

void 
gtk_source_view_set_show_line_pixmaps(GtkSourceView *view, gboolean show)
{
    g_return_if_fail(view != NULL);
    g_return_if_fail (GTK_IS_SOURCE_VIEW(view));

    view->show_line_pixmaps = show;
    g_print("show pixmaps is now %d\n", view->show_line_pixmaps);
    if(!view->show_line_numbers && !view->show_line_pixmaps)
    {
        gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(view), GTK_TEXT_WINDOW_LEFT, 0);
    }
    else
    {
        recompute_gutter_width(GTK_TEXT_VIEW(view));
    }
}

gboolean
gtk_source_view_get_show_line_pixmaps(GtkSourceView *view)
{
    g_return_val_if_fail(view != NULL, FALSE);
    g_return_val_if_fail(GTK_IS_SOURCE_VIEW(view), FALSE);

    return(view->show_line_pixmaps);
}

void
gtk_source_view_set_tab_stop(GtkSourceView *view, gint tab_stop)
{
    GtkTextView *text_view;
    PangoTabArray *tabs;

    g_return_if_fail(GTK_IS_SOURCE_VIEW(view));

    view->tab_stop = tab_stop;
    tabs = pango_tab_array_new(1, TRUE);
    pango_tab_array_set_tab(tabs, 0, PANGO_TAB_LEFT, get_tab_stop_width(GTK_WIDGET(view), tab_stop));
    gtk_text_view_set_tabs(GTK_TEXT_VIEW(view), tabs);
    pango_tab_array_free(tabs);
}

/*
 * This is a pretty important function...we call it when the tab_stop is changed,
 * And when the font is changed.
 * NOTE: You must change this with the default font for now...
 * It may be a good idea to set the tab_width for each GtkTextTag as well
 * based on the font that we set at creation time
 * something like style_cache_set_tabs_from_font or the like.
 * Now, this *may* not be necessary because most tabs wont be inside of another highlight,
 * except for things like multi-line comments (which will usually have an italic font which
 * may or may not be a different size than the standard one), or if some random language
 * definition decides that it would be spiffy to have a bg color for "start of line" whitespace
 * "^\(\t\| \)+" would probably do the trick for that.
 */

static gint
get_tab_stop_width(GtkWidget *widget, gint tab_stop)
{
    gchar *tab_string = NULL;
    int counter = 0;
    int tab_width = 0;

    tab_string = g_new0(char, tab_stop + 1);
    while(counter < tab_stop)
    {
        tab_string[counter] = ' ';
        counter++;
    }
    if(widget->style->private_font)
        tab_width = gdk_string_width(widget->style->private_font, tab_string);
    else
        tab_width = 8 * tab_stop;
    return(tab_width);
}

gint
gtk_source_view_get_tab_stop(GtkSourceView *view)
{
    PangoTabArray *tabs;
    PangoTabAlign alignment;

    g_return_val_if_fail(GTK_IS_SOURCE_VIEW(view), FALSE);

    return(view->tab_stop);
}

gint
gtk_source_view_get_tab_stop_width(GtkSourceView *view)
{
    PangoTabArray *tabs;
    PangoTabAlign alignment;
    gint tabstop;

    g_return_val_if_fail(GTK_IS_SOURCE_VIEW(view), FALSE);

    tabs = gtk_text_view_get_tabs(GTK_TEXT_VIEW(view));
    pango_tab_array_get_tab(tabs, 0, &alignment, &tabstop);
    return(tabstop);
}

static gint
get_lines (GtkTextView  *text_view, gint first_y, gint last_y, GArray *buffer_coords, gint *countp)
{
    GtkTextIter iter;
    gint count;
    gint size;  
    gint num;

    g_array_set_size(buffer_coords, 0);
  
    /* Get iter at first y */
    gtk_text_view_get_line_at_y (text_view, &iter, first_y, NULL);
    /*
       For each iter, get its location and add it to the arrays.
       Stop when we pass last_y
    */
    count = 0;
    size = 0;

    num = gtk_text_iter_get_line(&iter);
    do
    {
        gint y;
        gint height;
        gtk_text_view_get_line_yrange(text_view, &iter, &y, &height);
        g_array_append_val (buffer_coords, y);
        count++;
        if ((y + height) >= last_y) break;
        gtk_text_iter_forward_line (&iter);
    } while(!gtk_text_iter_is_end (&iter));
    *countp = count;
    return(num);
}

gboolean foreach_character (gunichar ch, gpointer data)
{
    return(ch == GPOINTER_TO_INT(data));
}

static gboolean
find_correct_bracket(GtkTextBuffer *buf,GtkTextIter *cur)
{
    gint dummy = 0;
    gint e = 0;
    GtkTextIter iter1=*cur;

    do
    {
        if(gtk_text_iter_get_char(&iter1) == '{') 
        {
            dummy--;
            e++;
        }
        else if(gtk_text_iter_get_char(&iter1) == '}')
        {    
            dummy++;
            e--;
        }
        if(!dummy && e > 1)
            return e;
        gtk_text_iter_backward_char(&iter1);
    } while(!gtk_text_iter_is_start(&iter1));
    return e >= 1 ? e : 0;
}

static gint 
recompute_gutter_width(GtkTextView *text_view)
{
    GtkSourceView *view = GTK_SOURCE_VIEW(text_view);
    gchar *str = NULL;
    gint lines = 0;
    gint text_width = 0;
    gint old_width;

    if(view->show_line_numbers)
    {
        lines = gtk_text_buffer_get_line_count(text_view->buffer);
        lines--;
        str = g_strdup_printf ("%d", lines);
        text_width += gdk_string_width(gtk_style_get_font(GTK_WIDGET(text_view)->style), str);
        g_free(str);
        text_width += GUTTER_PAD * 2;
    }
    view->line_number_space = text_width;

    if(view->show_line_pixmaps)
    {
        text_width += text_width ? 0 : GUTTER_PAD;
        text_width += GUTTER_PIXMAP + GUTTER_PAD;
    }

    old_width = text_view->left_window ? gtk_text_view_get_border_window_size(text_view, GTK_TEXT_WINDOW_LEFT) : 0;
    if(text_width != old_width)
    {
        gtk_text_view_set_border_window_size(text_view, GTK_TEXT_WINDOW_LEFT, text_width);
        gtk_widget_queue_draw(GTK_WIDGET(text_view));
    }
    return(text_width);
}

static void
check_line_change_insert_cb(GtkTextBuffer *buffer, GtkTextIter *iter, const gchar *text, gint len, GtkTextView *view)
{
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));

    if(GTK_SOURCE_VIEW(view)->show_line_numbers && text && strchr(text, '\n'))
    {
        recompute_gutter_width(view);
    }
}

static void
check_line_change_delete_cb(GtkTextBuffer *buffer, GtkTextIter *start, GtkTextIter *end, GtkTextView *view)
{
    gchar *slice;

    g_return_if_fail(buffer != NULL);
    g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
    GTK_SOURCE_VIEW(view)->delete = gtk_text_iter_get_slice(start, end);    
}

static void
check_line_change_delete_cb_after(GtkTextBuffer *buffer, GtkTextIter *start, GtkTextIter *end, GtkTextView *view)
{
    gchar *slice;
    slice = GTK_SOURCE_VIEW(view)->delete;
    if(GTK_SOURCE_VIEW(view)->show_line_numbers && slice && strchr(slice, '\n'))
    {
        recompute_gutter_width(view);
    }
    if(slice) g_free(slice);
    GTK_SOURCE_VIEW(view)->delete = NULL;
}

gboolean
gtk_source_view_add_pixbuf(GtkSourceView *view, const gchar *key, GdkPixbuf *pixbuf, gboolean overwrite)
{
    GtkTextBuffer *buffer = NULL;
    gint line_count = 0;
    gpointer data = NULL;
    gboolean replaced = FALSE;

    g_return_val_if_fail(view != NULL, FALSE);
    g_return_val_if_fail(GTK_IS_SOURCE_VIEW(view), FALSE);

    data = g_hash_table_lookup(view->pixmap_cache, key);
    if(data && !overwrite) return(FALSE);

    if(data)
    {
        g_hash_table_remove(view->pixmap_cache, key);
        g_object_unref(G_OBJECT(data));
        replaced = TRUE;
    }
    if(pixbuf && GDK_IS_PIXBUF(pixbuf))
    {
        gint width;
        gint height;

        width = gdk_pixbuf_get_width(pixbuf);
        height = gdk_pixbuf_get_height(pixbuf);
        if(width > GUTTER_PIXMAP || height > GUTTER_PIXMAP)
        {
            if(width > GUTTER_PIXMAP) width = GUTTER_PIXMAP;
            if(height > GUTTER_PIXMAP) height = GUTTER_PIXMAP;
            pixbuf = gdk_pixbuf_scale_simple(pixbuf, width, height, GDK_INTERP_BILINEAR);
        }
        g_object_ref(G_OBJECT(pixbuf));
        g_hash_table_insert(view->pixmap_cache, (gchar *)key, (gpointer)pixbuf);
    }
    return(replaced);
}

GdkPixbuf *
gtk_source_view_get_pixbuf(GtkSourceView *view, const gchar *key)
{
   return(g_hash_table_lookup(view->pixmap_cache, key));
}
