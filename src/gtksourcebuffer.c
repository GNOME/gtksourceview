/*  gtksourcebuffer.c
 *  Copyright (C) 1999,2000,2001 by:
 *  Mikael Hermansson <tyan@linux.se>
 *  Chris Phelps <reninet.com>
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
#include <time.h>
#include <gtk/gtk.h>

#include "gtksourcebuffer.h"

#ifndef UNDO_MAX
#define UNDO_MAX 5
#endif

static void gtk_source_buffer_class_init(GtkSourceBufferClass *_class);
static void gtk_source_buffer_init(GtkSourceBuffer *_class);
static void gtk_source_buffer_finalize(GObject *object);
static void hash_remove_func(gpointer key, gpointer value, gpointer user_data);

static void move_cursor(GtkTextBuffer *buf, GtkTextIter *iter, GtkTextMark *mark, gpointer data);
static void property_text_insert(GtkTextBuffer *buf, GtkTextIter *iter, const gchar *txt, gint len);
static void property_text_insert_after(GtkTextBuffer *buf, GtkTextIter *iter, const gchar *txt, gint len);
static void property_text_remove(GtkTextBuffer *buf, GtkTextIter *iter, GtkTextIter *end);
static void property_text_remove_after(GtkTextBuffer *buf, GtkTextIter *iter, GtkTextIter *end);
static void begin_user_action(GtkTextBuffer *buf);
static void end_user_action(GtkTextBuffer *buf);

static void check_embedded(GtkSourceBuffer *sbuf, GtkTextIter *start, GtkTextIter *end);
static void check_syntax(GtkSourceBuffer *sbuf, GtkTextIter *start, GtkTextIter *end);
static void check_pattern(GtkSourceBuffer *sbuf, const char *txt, gint length, GtkTextIter *iter);
static gint get_syntax_end(const char *text, gint pos, Regex *reg, GtkSourceBufferMatch *m);

static void check_brackets(GtkTextBuffer *buf,GtkTextIter *end);

static gint get_tag_start(GtkTextTag *tag, GtkTextIter *iter);
static gint get_tag_end(GtkTextTag *tag, GtkTextIter *iter);
static GtkSyntaxTag* is_syntax_tag(GList *list, GtkTextIter *iter);

static void gtk_source_buffer_undo_insert(GtkSourceBuffer *buf, gint type, const GtkTextIter *start_iter, const GtkTextIter *end_iter);
static gboolean gtk_source_buffer_remove_undo_entry(GtkSourceBuffer *buf, guint level);

static void get_tags_func(GtkTextTag *tag, gpointer data);

static GtkTextBufferClass *parent_class = NULL;

GType
gtk_source_buffer_get_type(void)
{
    static GType our_type=0;
    if(!our_type)
    {
        static const GTypeInfo our_info =
        {
            sizeof (GtkSourceBufferClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) gtk_source_buffer_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof (GtkSourceBuffer),
            0, /* n_preallocs */
            (GInstanceInitFunc) gtk_source_buffer_init
        };
        our_type = g_type_register_static (GTK_TYPE_TEXT_BUFFER, "GtkSourceBuffer", &our_info, 0);
	}
	return our_type;
}

static void
gtk_source_buffer_class_init(GtkSourceBufferClass *_class)
{
    GObjectClass *object_class;
    GtkWidgetClass *widget_class;
  
    object_class = (GObjectClass*) _class;
    object_class->finalize = gtk_source_buffer_finalize;
    widget_class = (GtkWidgetClass*) _class;
    parent_class = gtk_type_class(GTK_TYPE_TEXT_BUFFER);
}

static void 
gtk_source_buffer_init(GtkSourceBuffer *text)
{
    text->check_brackets = FALSE;
    text->highlight = TRUE;
  
    text->mark = NULL;
    text->undo_redo = NULL;
    text->undo_level = 0;
    text->undo_max = UNDO_MAX;
    text->undo_redo_processing = FALSE;

    g_signal_connect_closure(G_OBJECT(text),"delete_range", g_cclosure_new((GCallback)property_text_remove,NULL,NULL), FALSE);
    g_signal_connect_closure(G_OBJECT(text),"delete_range", g_cclosure_new((GCallback)property_text_remove_after,NULL,NULL), TRUE);
    g_signal_connect_closure(G_OBJECT(text),"insert_text", g_cclosure_new((GCallback)property_text_insert,NULL,NULL), FALSE);
    g_signal_connect_closure(G_OBJECT(text),"insert_text", g_cclosure_new((GCallback)property_text_insert_after,NULL,NULL), TRUE);
    g_signal_connect_closure(G_OBJECT(text),"begin_user_action", g_cclosure_new((GCallback)begin_user_action,NULL,NULL), FALSE);	
    g_signal_connect_closure(G_OBJECT(text),"end_user_action", g_cclosure_new((GCallback)end_user_action,NULL,NULL), FALSE);	
    g_signal_connect_closure(G_OBJECT(text),"mark_set", g_cclosure_new((GCallback)move_cursor, NULL, NULL), TRUE);
    text->line_markers = g_hash_table_new(NULL, NULL);
}

GtkTextBuffer *
gtk_source_buffer_new(GtkTextTagTable *table)
{
    GObject *text;
    text = g_object_new(GTK_TYPE_SOURCE_BUFFER, NULL);

    if (table)
    {
        GTK_TEXT_BUFFER(text)->tag_table = table;
        g_object_ref (G_OBJECT (GTK_TEXT_BUFFER(text)->tag_table));
    }
    else
    {
        GTK_TEXT_BUFFER(text)->tag_table = gtk_text_tag_table_new();
    }
    GTK_SOURCE_BUFFER(text)->bracket_match_tag = gtk_text_tag_new("bracket-match");
    g_object_set(G_OBJECT(GTK_SOURCE_BUFFER(text)->bracket_match_tag), "foreground_gdk", "blue", NULL);
    g_object_set(G_OBJECT(GTK_SOURCE_BUFFER(text)->bracket_match_tag), "background_gdk", "gray", NULL);
    gtk_text_tag_table_add(GTK_TEXT_BUFFER(text)->tag_table, GTK_SOURCE_BUFFER(text)->bracket_match_tag);
    return GTK_TEXT_BUFFER(text);
}

static void
gtk_source_buffer_finalize(GObject *object)
{
    GtkSourceBuffer *buffer;

    g_return_if_fail(object != NULL);
    g_return_if_fail(GTK_IS_SOURCE_BUFFER(object));

    buffer = GTK_SOURCE_BUFFER(object);
    if(buffer->line_markers)
    {
        g_hash_table_foreach_remove(buffer->line_markers, (GHRFunc)hash_remove_func, NULL);
        g_hash_table_destroy(buffer->line_markers);
    }
}

static void
hash_remove_func(gpointer key, gpointer value, gpointer user_data)
{
    GList *iter = NULL;

    if(value)
    {
        for(iter = (GList *)value; iter; iter = iter->next)
        {
            g_print("Removing marker %s for line %d\n", (gchar *)iter->data, GPOINTER_TO_INT(key));
            if(iter->data) g_free(iter->data);
        }
        g_list_free((GList *)value);
    }
}

void
gtk_source_buffer_attach_to_view(GtkSourceBuffer *buffer, GtkTextView *view)
{
    GtkWidget *widget = GTK_WIDGET(view);

    if(buffer->bracket_match_tag)
        gtk_text_tag_table_remove(GTK_TEXT_BUFFER(buffer)->tag_table, buffer->bracket_match_tag);
    buffer->bracket_match_tag = gtk_text_tag_new("bracket-match");
    g_object_set(G_OBJECT(buffer->bracket_match_tag), "foreground_gdk", &widget->style->fg[GTK_STATE_SELECTED], NULL);
    g_object_set(G_OBJECT(buffer->bracket_match_tag), "background_gdk", &widget->style->bg[GTK_STATE_SELECTED], NULL);
    gtk_text_tag_table_add(GTK_TEXT_BUFFER(buffer)->tag_table, buffer->bracket_match_tag);
}

gint
get_tag_start(GtkTextTag *tag, GtkTextIter *iter)
{
    gint count = 0;
    do
    {
      count++;
      if (gtk_text_iter_begins_tag(iter,tag))  break;
    } while (gtk_text_iter_backward_char(iter));
    g_print("tag is started at offset %d\n",gtk_text_iter_get_offset(iter));
    return count;
}

gint
get_tag_end(GtkTextTag *tag, GtkTextIter *iter)
{
    gint count = 0;
    do
    {
      count++;
      if (gtk_text_iter_ends_tag(iter,tag)) break;
    } while (gtk_text_iter_forward_char(iter));
    g_print("tag is started at offset %d\n",gtk_text_iter_get_offset(iter));
    return count;
}

static void
move_cursor(GtkTextBuffer *buf, GtkTextIter *iter, GtkTextMark *m, gpointer data)
{
  GtkSourceBuffer *sbuf = GTK_SOURCE_BUFFER(buf);
  GtkTextIter iter1;
  GtkTextIter iter2;

  if(m != gtk_text_buffer_get_insert(buf))
    return ;

  if (sbuf->mark)
  {
    gtk_text_buffer_get_iter_at_mark(buf, &iter1, sbuf->mark);
    iter2 = iter1;
    gtk_text_iter_forward_char(&iter2);
    gtk_text_buffer_remove_tag(buf, sbuf->bracket_match_tag, &iter1, &iter2);
  }
  if(gtk_source_buffer_iter_has_syntax_tag(iter)) return;

  if(gtk_source_buffer_find_bracket_match(iter))
  {
    if(!sbuf->mark)
      sbuf->mark = gtk_text_buffer_create_mark(buf, NULL, iter, FALSE);
    else  
      gtk_text_buffer_move_mark(buf,sbuf->mark, iter);

    iter2 = *iter;
    gtk_text_iter_forward_char(&iter2);
    gtk_text_buffer_apply_tag(buf, sbuf->bracket_match_tag, iter, &iter2);
  }  
}

static void
property_text_insert(GtkTextBuffer *buf, GtkTextIter *curpos, const gchar *txt, gint len)
{
    GtkSyntaxTag *tag;
    GtkTextIter start, end;
    GtkSourceBuffer *sbuf;

    g_return_if_fail(buf != NULL);
    g_return_if_fail(GTK_IS_SOURCE_BUFFER(buf));

    sbuf = GTK_SOURCE_BUFFER(buf);  
    if(!sbuf->highlight) return;

    start = *curpos;
    end = *curpos;
    gtk_text_iter_forward_chars(&end,len);

    /* we have connected AFTER insert thats why iterator points to end */
    /* we need to update startiter to point to txt startpos instead :-) */

    if(sbuf->syntax_items)
    { 
        tag = gtk_source_buffer_iter_has_syntax_tag(&start);
        if (!tag)
        {
            /* no syntax found we refresh from */ 
            /*start of first instered line to end of last inserted line */
            gtk_text_iter_set_line_offset(&start, 0);
            gtk_text_iter_forward_line(&end);
        }
        else
        {
            gint scount;
            gint ecount;
            scount = get_tag_start(GTK_TEXT_TAG(tag), &start);
            ecount = get_tag_end(GTK_TEXT_TAG(tag), &end);
        }
    }
    else
    {        
        gtk_text_buffer_get_bounds(buf, &start, &end);
    }

    gtk_text_buffer_remove_all_tags(buf, &start, &end);
    sbuf->refresh_start = gtk_text_iter_get_offset(&start);
    sbuf->refresh_length = gtk_text_iter_get_offset(&end) - sbuf->refresh_start;
    if(sbuf->refresh_length < len)
        sbuf->refresh_length = len;
}

static void
property_text_insert_after(GtkTextBuffer *buf, GtkTextIter *iter, const gchar *txt, gint len)
{
    GtkTextIter undostart = *iter;
    GtkTextIter start = *iter; 
    GtkTextIter end = *iter; 
    GtkSourceBuffer *sbuf;

    sbuf = GTK_SOURCE_BUFFER(buf);  
    gtk_source_buffer_undo_insert(sbuf, UNDO_TYPE_REMOVE_RANGE, &undostart, iter);
    if(!sbuf->highlight) return;
    if (!sbuf->refresh_length) return;

    gtk_text_iter_set_offset(&start, sbuf->refresh_start);
    end = start;
    gtk_text_iter_backward_chars(&undostart, len);
    gtk_text_iter_forward_chars(&end, sbuf->refresh_length);
    sbuf->refresh_start = 0;
    sbuf->refresh_length = 0;
    check_embedded(sbuf, &start, &end);  
}

static void
property_text_remove(GtkTextBuffer *buf, GtkTextIter *iter, GtkTextIter *iter2)
{
    GtkTextIter start = *iter;
    GtkTextIter end = *iter2;
    GtkSyntaxTag *tag;
    GtkSourceBuffer *sbuf;

    sbuf = GTK_SOURCE_BUFFER(buf);

    gtk_source_buffer_undo_insert(sbuf, UNDO_TYPE_INSERT_TEXT, iter, iter2);
    if(!sbuf->highlight) return;

    /* we need to work on this */
    /* first check if iter holds a tag */
    /* if true then iterate backward until tag change */
    /* then iterate iter2 forward until tag change */
    /* remove tags and update from new start and end iters */
    /* if no tags, we iterate from line begin to iter2 line end */
    /* also check if syntax tag if true then ignore and let widget take care of it*/

    start = *iter;
    end = *iter2;

    if(sbuf->syntax_items)
    { 
        tag = gtk_source_buffer_iter_has_syntax_tag(&start);
        if (!tag)
        {
            /* no syntax found, so we refresh from */
            /* start of first instered line to end of last inserted line */
            gtk_text_iter_set_line_offset(&start,0);
            end = start;
            gtk_text_iter_forward_line(&end);
            if(gtk_text_iter_get_offset(&end) < gtk_text_iter_get_offset(iter2))
                end = *iter2;
        }
        else
        {
            gint scount;
            gint ecount;
            scount = get_tag_start(GTK_TEXT_TAG(tag), &start);
            ecount = get_tag_end(GTK_TEXT_TAG(tag), &end);
            if(scount > tag->reg_start.len && ecount > tag->reg_end.len) return;
        }
    }

    sbuf->refresh_start = gtk_text_iter_get_offset(&start);
    sbuf->refresh_length = gtk_text_iter_get_offset(&end) - sbuf->refresh_start;
    gtk_text_buffer_remove_all_tags(buf, &start, &end);
}

static void
property_text_remove_after(GtkTextBuffer *buf, GtkTextIter *iter, GtkTextIter *iter2)
{
    GtkTextIter start=*iter; 
    GtkTextIter end=*iter2; 
    GtkSourceBuffer *sbuf;

    sbuf = GTK_SOURCE_BUFFER(buf);
    if(!sbuf->highlight) return;
    if(!sbuf->refresh_length) return;

    gtk_text_iter_set_offset(&start, sbuf->refresh_start); 
    end = start;
    gtk_text_iter_forward_chars(&end, sbuf->refresh_length); 

    sbuf->refresh_start = 0;
    sbuf->refresh_length = 0;
    check_embedded(sbuf, &start, &end);  
}


/*
   we need to optimize this crap, as it is pretty slow
   if the start and end interval is to big (which it usually is)
   The most important thing is to use the included
   regular expression library because it's massively
   faster than what is coming with distributions right
   now, especially RedHat. Probably 4 to 8 times faster
   even though they are theoretically the same version (12)
*/

static void
check_embedded(GtkSourceBuffer *sbuf, GtkTextIter *iter1, GtkTextIter *iter2)
{
    GtkTextBuffer *buf;
    GList *list;
    gchar *text;
    GtkTextIter start_iter;
    GtkTextIter cur_iter;
    GtkTextIter end_iter;
    GtkTextIter iterrealend;
    GtkEmbeddedTag *tag;
    gint length = 0;
    gint len = 0;
    gint nlen = 0;
    gint i = 0;
    gint j = 0;
    gboolean found = FALSE;

    buf = GTK_TEXT_BUFFER(sbuf);
    list = gtk_source_buffer_get_embedded_entries(sbuf);
    if(!list)
    {
        check_syntax(sbuf, iter1, iter2);
        return;
    }

    g_print("Rehighlighting from %d to %d\n", gtk_text_iter_get_offset(iter1), gtk_text_iter_get_offset(iter2));

    text = gtk_text_buffer_get_slice(buf, iter1, iter2, TRUE);
    length = strlen(text);

    start_iter = *iter1;  

    for(i = 0; i < length; i++)
    {
       for(list = gtk_source_buffer_get_embedded_entries(sbuf); list; list = list->next)
       {
           tag = GTK_EMBEDDED_TAG(list->data);
           if((len = gtk_source_buffer_regex_match(text, i, length, &tag->reg_outside)) > 0)
           {
               /*
                  we have found the outside regex inside the current buffer
                  now we have to go about detecting if there is anything *inside*
                  of out new range that should be highlighted
               */
               g_print("Embedded range found at position %d with length %d.\n", i, len);
               for(j = i; j < (i+len); j++)
               {
                   if((nlen = gtk_source_buffer_regex_match(text, j, j + len, &tag->reg_inside)) > 0)
                   {
                        end_iter = cur_iter;
                        gtk_text_iter_forward_chars(&end_iter, nlen);
                        g_print("Embedded item found at position %d with length %d.\n", j, nlen);
                        gtk_text_buffer_apply_tag(buf, GTK_TEXT_TAG(tag), &cur_iter, &end_iter);
                        // we're about to j++ so make sure we compensate now
                        j += (nlen-1);
                        // dont compensate here tho
                        gtk_text_iter_forward_chars(&cur_iter, nlen);
                   }
                   else
                   {
                       gtk_text_iter_forward_char(&cur_iter);
                   }
               }
           }
           gtk_text_iter_forward_char(&start_iter);
       }
   }
   g_free(text);
   check_syntax(sbuf, iter1, iter2);
}

static void
check_syntax(GtkSourceBuffer *sbuf, GtkTextIter *iter1, GtkTextIter *iter2)
{
    GtkTextBuffer *buf;
    GList *list;
    gchar *txt;
    GtkTextIter curiter;
    GtkTextIter curiter2;
    GtkTextIter iterrealend;
    GtkSyntaxTag *tag;
    gint s = 0;
    gint pos = 0;
    gint oldpos = 0;
    gint z = 0;
    gint len = 0;
    gint offset = 0;
    gboolean found = FALSE;
    GtkSourceBufferMatch m;

    buf = GTK_TEXT_BUFFER(sbuf);

    gtk_text_buffer_get_end_iter(buf, &iterrealend);
    txt = gtk_text_buffer_get_slice(buf, iter1, &iterrealend, TRUE);

    list = gtk_source_buffer_get_syntax_entries(sbuf);
    if(!list)
    {
        /* FIXME: Here we may need to do some modifications
           to properly check the pattern entries when no
           syntax entries exist. However, at this point it
           is my contention that it cannot be dont properly
           check_pattern(sbuf, txt, len - pos, &curiter); */
        return;
    }

    curiter =* iter1;  
    curiter2 = curiter;
    offset = gtk_text_iter_get_offset(iter1);
    len = gtk_text_iter_get_offset(iter2);
    len -= offset;

    while(pos < len)
    {
        /* check for any of the syntax highlights */
        s = gtk_source_buffer_regex_search(txt, pos, &sbuf->reg_syntax_all, TRUE, &m);
        if(s < 0 ||  s > len)	break ; /* not found */
        /* if there is text segments before syntax, check pattern too... */
        if (pos < s) check_pattern(sbuf, &txt[pos], s - pos, &curiter);
        pos = m.endpos;
        gtk_text_iter_forward_chars(&curiter, m.endpos - oldpos);
        oldpos = pos;

        for(list = gtk_source_buffer_get_syntax_entries(sbuf); list; list = list->next)
        {
            tag = GTK_SYNTAX_TAG(list->data);
            if(gtk_source_buffer_regex_match(txt, s, len, &tag->reg_start)  > 0 && txt[s-1] != '\\')
            {
                if((z = get_syntax_end(txt, pos, &tag->reg_end, &m)) >= 0) pos = m.endpos;			
                else if(z == 0) continue;
                else pos = gtk_text_buffer_get_char_count(buf) - offset; 
                /* g_print("applied syntax tag [%s] at startpos %d endpos %d\n",sentry->key,s,i);*/
                gtk_text_iter_set_offset(&curiter, s+offset);
                curiter2 = curiter;
                gtk_text_iter_forward_chars(&curiter2, pos - s);

                /* make sure ALL tags between syntax start/end are removed */
                /* we only remove if syntax start/end is more than endpos */
                if(s > len+offset || pos > len+offset)
                {
                    g_print("remove all tags between %d and %d\n",s,pos);
                    gtk_text_buffer_remove_all_tags(buf, &curiter, &curiter2);
                }
                gtk_text_buffer_apply_tag(buf, GTK_TEXT_TAG(tag), &curiter, &curiter2);
                curiter = curiter2;        
                found = TRUE;
                break;
            }
            else if(txt[s - 1] == '\\') found = TRUE;
        }
        if(!found)
        {
            pos++;
            gtk_text_iter_forward_chars(&curiter, 1);
        }
    }
    if(pos < len) check_pattern(sbuf, &txt[pos], len - pos, &curiter);
    if(txt) g_free(txt);
}

static void
check_pattern(GtkSourceBuffer *sbuf, const char *txt, gint length, GtkTextIter *iter)
{
    GtkTextBuffer *buf;
    GList *list = NULL;
    GtkTextIter start_iter;
    GtkTextIter end_iter;
    GtkPatternTag *tag = NULL;
    gint max_length = 0;
    gint i = 0;
    gint len = 0;

    buf = GTK_TEXT_BUFFER(sbuf);
    list = gtk_source_buffer_get_pattern_entries(sbuf);
    if(!list) return;

    max_length = gtk_text_buffer_get_char_count(buf);
    if(len > max_length) len = strlen(txt);

    start_iter = *iter;

    for(i = 0; i < length; i++)
    {
        for(list = gtk_source_buffer_get_pattern_entries(sbuf); list; list = list->next)
        {
            tag = GTK_PATTERN_TAG(list->data);      
            if((len = gtk_source_buffer_regex_match(txt, i, length, &tag->reg_pattern) ) > 0)
            {
                end_iter = start_iter;
                gtk_text_iter_forward_chars(&end_iter, len);
                /* g_print("applied pattern tag [] at startpos %d endpos %d\n", pos, endpos + pos);*/
                gtk_text_buffer_apply_tag(buf, GTK_TEXT_TAG(tag), &start_iter, &end_iter);
                // compensate for the i++ that will happen in a moment by using len-1
                i += (len-1);
                gtk_text_iter_forward_chars(&start_iter, len-1);
            }
        }
        gtk_text_iter_forward_char(&start_iter);
    }  
}

static gint 
get_syntax_end(const char *txt, gint pos, Regex *reg, GtkSourceBufferMatch *m)
{
    int ret = pos;
    do
    {
        ret=gtk_source_buffer_regex_search(txt,m->endpos,reg,TRUE,m);
        if(ret < 0) return(-1);
    } while(m->endpos && txt[m->endpos-2] == '\\' );
    return(ret);
}

void
gtk_source_buffer_set_check_brackets(GtkSourceBuffer *buf, gboolean set)
{
    g_return_if_fail(buf != NULL);
    g_return_if_fail(GTK_IS_SOURCE_BUFFER(buf));

    buf->check_brackets = set;
}

void
gtk_source_buffer_set_highlight(GtkSourceBuffer *buf, gboolean set)
{
    GtkTextIter iter1;
    GtkTextIter iter2;

    g_return_if_fail(buf != NULL);
    g_return_if_fail(GTK_IS_SOURCE_BUFFER(buf));
    
    buf->highlight = set;
    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(buf), &iter1, &iter2);
    if(buf->highlight)
    {
        check_embedded(buf, &iter1, &iter2);    
    }
    else
    {
        gtk_text_buffer_remove_all_tags(GTK_TEXT_BUFFER(buf), &iter1, &iter2);
    }
}

/* regex_match -- tries to match regex at the 'pos' position in the
 * text. It returns the number of chars matched, or -1 if no match.
 * Warning!  The number matched can be 0, if the regex matches the
 * empty string.  The reason for workin on GtkSCText is the same as in
 * regex_search. */
gint
gtk_source_buffer_regex_match (const char *txt, gint pos,gint stop, Regex *regex)
{
    g_return_val_if_fail (regex != NULL, -1);

    return re_match (&regex->buf, txt, stop, pos, &regex->reg);
}

gint
gtk_source_buffer_regex_search (const char *txt, gint pos, Regex *regex, gboolean forward, GtkSourceBufferMatch *m)
{
    gint len;
    g_return_val_if_fail (regex != NULL, -1);
    g_return_val_if_fail (m != NULL, -1);

    len = strlen(txt);  
    m->startpos = re_search (&regex->buf,txt,len, pos, (forward ? len - pos : -pos), &regex->reg);
    if(m->startpos > -1) m->endpos = regex->reg.end[0];
    return m->startpos;    
}

gboolean 
gtk_source_buffer_find_bracket_match(GtkTextIter *orig)
{
   GtkTextIter iter = *orig;
   gunichar base_char = 0;
   gunichar search_char = 0;
   gunichar cur_char = 0;
   gint addition = -1;
   gint counter = 0;
   gboolean found = FALSE;


    gtk_text_iter_backward_char(&iter);
   cur_char = gtk_text_iter_get_char(&iter);

   base_char = cur_char;
   switch((int)base_char)
   {
      case '{': addition = 1;
                search_char = '}';
                break;
      case '(': addition = 1;
                search_char = ')';
                break;
      case '[': addition = 1;
                search_char = ']';
                break;
      case '<': addition = 1;
                search_char = '>';
                break;
      case '}': addition = -1;
                search_char = '{';
                break;
      case ')': addition = -1;
                search_char = '(';
                break;
      case ']': addition = -1;
                search_char = '[';
                break;
      case '>': addition = -1;
                search_char = '<';
                break;
      default : addition = 0;
                break;
   }
   if(!addition) return(FALSE);

   do
   {
      gtk_text_iter_forward_chars(&iter,addition);

       cur_char = gtk_text_iter_get_char(&iter);
      if(cur_char == search_char && !counter)
      {
         found = TRUE;
         break;
      }
      if(cur_char == base_char)
         counter++;
      else if(cur_char == search_char)
         counter--;
   }
   while(!gtk_text_iter_is_end(&iter) && !gtk_text_iter_is_start(&iter));

   if(found) *orig=iter;

   return(found);
}


/***************************************************************************/
/*                       undo implementation                              */
/***************************************************************************/

void
begin_user_action(GtkTextBuffer *buf)
{
    GtkSourceBuffer *sbuf; 
    g_return_if_fail(GTK_IS_SOURCE_BUFFER(buf));
    //g_return_if_fail(!GTK_SOURCE_BUFFER(buf)->undo_redo_processing);

    sbuf = GTK_SOURCE_BUFFER(buf); 
    //g_print("undo array init (begin)\n");
}

void
end_user_action(GtkTextBuffer *buf)
{
    GtkSourceBuffer *sbuf; 
    g_return_if_fail(GTK_IS_SOURCE_BUFFER(buf));
    //g_return_if_fail(GTK_SOURCE_BUFFER(buf)->undo_redo_processing);

    sbuf=GTK_SOURCE_BUFFER(buf); 
    //g_print("undo array end\n");
}

gboolean
gtk_source_buffer_undo(GtkSourceBuffer *buf)
{
    GtkSourceBufferUndoEntry *entry;
    GtkTextIter start_iter;
    GtkTextIter end_iter;
    gint length;
    gchar *text;

    g_return_val_if_fail(buf != NULL, FALSE);
    g_return_val_if_fail(GTK_IS_SOURCE_BUFFER(buf), FALSE);

    length = g_list_length(buf->undo_redo);
    if ((length < 1) || (length == buf->undo_level)) return (FALSE);
    buf->undo_redo_processing = TRUE;
    entry = g_list_nth_data(buf->undo_redo, buf->undo_level);
    if(entry->type == UNDO_TYPE_INSERT_TEXT)
    {
        //g_print("Undo: Inserting chars: %s.\n", (gchar *)entry->data);
        gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buf), &start_iter, entry->offset);
        gtk_text_buffer_insert(GTK_TEXT_BUFFER(buf), &start_iter, (const gchar *)entry->data, entry->length);
        g_free(entry->data);
        entry->data = NULL; 
        entry->type = UNDO_TYPE_REMOVE_RANGE;
    }
    else if(entry->type == UNDO_TYPE_REMOVE_RANGE)
    {
        gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buf), &start_iter, entry->offset);
        gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buf), &end_iter, entry->offset + entry->length);
        text = gtk_text_buffer_get_slice(GTK_TEXT_BUFFER(buf), &start_iter, &end_iter, TRUE);
        gtk_text_buffer_delete(GTK_TEXT_BUFFER(buf), &start_iter, &end_iter);
        entry->data = (gpointer)text;
        entry->type = UNDO_TYPE_INSERT_TEXT;
        //g_print("Undo: Removed chars: %s (%d-%d)\n", text, entry->offset, entry->offset + entry->length);
    }
    buf->undo_level++;
    buf->undo_redo_processing = FALSE;
    return (TRUE);
}

gboolean
gtk_source_buffer_redo(GtkSourceBuffer *buf)
{
    GtkSourceBufferUndoEntry *entry;
    GtkTextIter start_iter;
    GtkTextIter end_iter;
    gint length;
    gchar *text;

    g_return_val_if_fail(text != NULL, FALSE);
    g_return_val_if_fail(GTK_IS_SOURCE_BUFFER(buf), FALSE);

    length = g_list_length(buf->undo_redo);
    if ((length < 1) || (buf->undo_level < 1)) return (FALSE);
    buf->undo_redo_processing = TRUE;
    buf->undo_level--;
    entry = g_list_nth_data(buf->undo_redo, buf->undo_level);
    if(entry->type == UNDO_TYPE_INSERT_TEXT)
    {
        //g_print("Redo: Inserting chars: %s.\n", (gchar *)entry->data);
        gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buf), &start_iter, entry->offset);
        gtk_text_buffer_insert(GTK_TEXT_BUFFER(buf), &start_iter, (const gchar *)entry->data, entry->length);
        g_free(entry->data);
        entry->data = NULL; 
        entry->type = UNDO_TYPE_REMOVE_RANGE;
    }
    else if(entry->type == UNDO_TYPE_REMOVE_RANGE)
    {
        gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buf), &start_iter, entry->offset);
        gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buf), &end_iter, entry->offset + entry->length);
        text = gtk_text_buffer_get_slice(GTK_TEXT_BUFFER(buf), &start_iter, &end_iter, TRUE);
        gtk_text_buffer_delete(GTK_TEXT_BUFFER(buf), &start_iter, &end_iter);
        entry->data = (gpointer)text;
        entry->type = UNDO_TYPE_INSERT_TEXT;
        //g_print("Redo: Removed chars: %s (%d-%d)\n", text, entry->offset, entry->offset + entry->length);
    }
    buf->undo_redo_processing = FALSE;
    return (TRUE);
}

gboolean
gtk_source_buffer_undo_is_empty (GtkSourceBuffer *buf)
{
    gboolean empty = TRUE;

    g_return_val_if_fail(buf != NULL, TRUE);
    g_return_val_if_fail(GTK_IS_SOURCE_BUFFER(buf), TRUE);

    if((g_list_length (buf->undo_redo) > 0) && (buf->undo_level < g_list_length(buf->undo_redo)))
    {
        empty = FALSE;
    }
    return(empty);
}

gboolean
gtk_source_buffer_redo_is_empty(GtkSourceBuffer *buf)
{
    gboolean empty = TRUE;

    g_return_val_if_fail(buf != NULL, TRUE);
    g_return_val_if_fail(GTK_IS_SOURCE_BUFFER(buf), TRUE);

    if((g_list_length(buf->undo_redo) > 0) && (buf->undo_level > 0))
    {
        empty = FALSE;
    }
    return(empty);
}

gint
gtk_source_buffer_undo_get_max(GtkSourceBuffer *buf)
{
    g_return_val_if_fail(buf != NULL, 0);
    g_return_val_if_fail(GTK_IS_SOURCE_BUFFER(buf), 0);

    return(buf->undo_max);
}

gboolean
gtk_source_buffer_undo_set_max(GtkSourceBuffer *buf, gint max)
{
    gboolean discarded = FALSE;

    g_return_val_if_fail(buf != NULL, FALSE);
    g_return_val_if_fail(GTK_IS_SOURCE_BUFFER(buf), FALSE);

    if (max < 0) return(FALSE);

    if (max >= buf->undo_max)
    {
        buf->undo_max = max;
    }
    else
    {
        while(g_list_length(buf->undo_redo) > max)
        {
            gtk_source_buffer_remove_undo_entry(buf, g_list_length(buf->undo_redo) - 1);
        }
        buf->undo_max = max;
        discarded = TRUE;
    }
    return(discarded);
}

void
gtk_source_buffer_undo_clear_all(GtkSourceBuffer *buf)
{
    g_return_if_fail(buf != NULL);
    g_return_if_fail(GTK_IS_SOURCE_BUFFER(buf));

    if (buf->undo_redo)
    {
        while(g_list_length(buf->undo_redo) > 0)
        {
            gtk_source_buffer_remove_undo_entry(buf, g_list_length(buf->undo_redo) - 1);
        }
        g_list_free(buf->undo_redo);
        buf->undo_redo = NULL;
        buf->undo_level = 0;
    }
}

static void
gtk_source_buffer_undo_insert(GtkSourceBuffer *buf, gint type, const GtkTextIter *start_iter, const GtkTextIter *end_iter)
{
    GtkSourceBufferUndoEntry *entry;
    gint remove;

    g_return_if_fail(buf != NULL);
    g_return_if_fail(GTK_IS_SOURCE_BUFFER(buf));

    if(buf->undo_redo_processing) return;

    if (buf->undo_level > 0)
    {
        for(remove = 0; remove < buf->undo_level; remove++)
        {
            gtk_source_buffer_remove_undo_entry(buf, 0);
        }
    }

    entry = g_new (GtkSourceBufferUndoEntry, 1);
    entry->type = type;
    if (type == UNDO_TYPE_INSERT_TEXT)
    {
        entry->data = (gpointer)gtk_text_buffer_get_slice(GTK_TEXT_BUFFER(buf), start_iter, end_iter, TRUE);
    }
    else if (type == UNDO_TYPE_REMOVE_RANGE)
    {
        entry->data = NULL;
    }
    entry->offset = gtk_text_iter_get_offset(start_iter);
    entry->length = gtk_text_iter_get_offset(end_iter) - gtk_text_iter_get_offset(start_iter);
    buf->undo_redo = g_list_prepend(buf->undo_redo, (gpointer)entry);
    if(g_list_length(buf->undo_redo) > buf->undo_max) gtk_source_buffer_remove_undo_entry(buf, g_list_length(buf->undo_redo) - 1);
    buf->undo_level = 0;
}

static gboolean
gtk_source_buffer_remove_undo_entry(GtkSourceBuffer *buf, guint level)
{
    GtkSourceBufferUndoEntry *entry;
    gboolean removed = FALSE;
    g_return_val_if_fail(buf != NULL, FALSE);
    g_return_val_if_fail(buf->undo_redo, FALSE);

    entry = g_list_nth_data(buf->undo_redo, level);
    if(entry)
    {
        if(entry->data) g_free(entry->data);
        buf->undo_redo = g_list_remove(buf->undo_redo, entry);
        g_free(entry);
        removed = TRUE;
    }
    return (removed);
}

GList *
gtk_source_buffer_get_regex_tags(GtkSourceBuffer *buf)
{
    GtkTextTagTable *table = NULL;
    GList *list = NULL;

    g_return_val_if_fail(GTK_IS_SOURCE_BUFFER (buf), NULL);

    table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER(buf));
    gtk_text_tag_table_foreach(table, get_tags_func, &list);
    list = g_list_first(list);
    return list;
}

void
gtk_source_buffer_purge_regex_tags(GtkSourceBuffer *buf)
{
    GtkTextTagTable *table = NULL;
    GList *list = NULL;
    GList *cur = NULL;
    GtkTextIter iter1;
    GtkTextIter iter2;
    gchar *name;

    g_return_if_fail(GTK_IS_SOURCE_BUFFER(buf));

    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(buf), &iter1, &iter2);
    gtk_text_buffer_remove_all_tags(GTK_TEXT_BUFFER(buf), &iter1, &iter2);

    table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER(buf));
    list = gtk_source_buffer_get_regex_tags(buf);
    for(cur = list; cur; cur = cur->next)
    {
        g_object_get(G_OBJECT(cur->data), "name", &name, NULL);
        g_print("remove tag with name %s\n", name);
        g_free(name);
        gtk_text_tag_table_remove(table, GTK_TEXT_TAG(cur->data));
    }
    g_list_free(list);
    if(buf->syntax_items)
    {
        g_list_free(buf->syntax_items);
        buf->syntax_items = NULL;
    }
    if(buf->pattern_items)
    {
        g_list_free(buf->pattern_items);
        buf->pattern_items = NULL;
    }
    if(buf->embedded_items)
    {
        g_list_free(buf->embedded_items);
        buf->embedded_items = NULL;
    }
}

static void
get_tags_func(GtkTextTag *tag, gpointer data)
{
    GList** list = (GList **)data;
    if(GTK_IS_SYNTAX_TAG(tag) || GTK_IS_PATTERN_TAG(tag) || GTK_IS_EMBEDDED_TAG(tag))
        *list = g_list_append(*list, (gpointer)tag);
}


void
gtk_source_buffer_install_regex_tags(GtkSourceBuffer *buf, GList *entries)
{
    GtkTextTag *tag = NULL;
    GtkSyntaxTag *stag = NULL;
    GtkPatternTag *ptag = NULL;
    GtkEmbeddedTag *etag = NULL;
    GList *cur = NULL;
    gchar *name;    

    for(cur = entries; cur; cur = cur->next)
    {
        g_object_get(G_OBJECT(cur->data), "name", &name, NULL);
        if(name)
        {
            tag = gtk_text_tag_table_lookup(GTK_TEXT_BUFFER(buf)->tag_table, name);
            if(tag)
            {
                g_print("A tag with the name %s already exists...removing.\n", name);
                gtk_text_tag_table_remove(GTK_TEXT_BUFFER(buf)->tag_table, tag);
            }
        }
        if(GTK_IS_SYNTAX_TAG(cur->data))
        {
            buf->syntax_items = g_list_append(buf->syntax_items, cur->data);
            gtk_text_tag_table_add(GTK_TEXT_BUFFER(buf)->tag_table, GTK_TEXT_TAG(cur->data));
        }
        else if(GTK_IS_PATTERN_TAG(cur->data))
        {
            buf->pattern_items = g_list_append(buf->pattern_items, cur->data);
            gtk_text_tag_table_add(GTK_TEXT_BUFFER(buf)->tag_table, GTK_TEXT_TAG(cur->data));
        }
        else if(GTK_IS_EMBEDDED_TAG(cur->data))
        {
            buf->embedded_items = g_list_append(buf->embedded_items, cur->data);
            gtk_text_tag_table_add(GTK_TEXT_BUFFER(buf)->tag_table, GTK_TEXT_TAG(cur->data));
        }
        if(name) g_free(name);
    }
    if(buf->syntax_items)
    {
        gtk_source_buffer_sync_syntax_regex(buf);
    }
}

void
gtk_source_buffer_sync_syntax_regex(GtkSourceBuffer *buf)
{
    GString *str;
    GList *cur;
    GtkSyntaxTag *tag;

    str = g_string_new("");
    cur = buf->syntax_items; 
    while(cur)
    {
        if(!GTK_IS_SYNTAX_TAG(cur->data))
        {
            g_warning("Serious error...there is a member in the syntax_items list that is not a syntax tag,\n");
            return;
        }
        tag = GTK_SYNTAX_TAG(cur->data);
        g_string_append(str, tag->start);
        cur = cur->next;
        if(cur) g_string_append(str,"\\|");
    }
    gtk_source_compile_regex(str->str, &buf->reg_syntax_all);
    g_string_free(str, TRUE);        
}

GtkSyntaxTag *
gtk_source_buffer_iter_has_syntax_tag(GtkTextIter *iter)
{
    GSList *list = gtk_text_iter_get_tags(iter);
  
    while(list)
    {
        if (GTK_IS_SYNTAX_TAG(list->data))    
            return GTK_SYNTAX_TAG(list->data);
        list = g_slist_next (list);
    }
    return NULL;
}

GList *
gtk_source_buffer_get_syntax_entries(GtkSourceBuffer *buf)
{
    return(buf->syntax_items);
}

GList *
gtk_source_buffer_get_pattern_entries(GtkSourceBuffer *buf)
{
    return(buf->pattern_items);
}

GList *
gtk_source_buffer_get_embedded_entries(GtkSourceBuffer *buf)
{
    return(buf->embedded_items);
}


/* gtk_source_buffer_convert_to_html */
/* converts the hihlighted sourcebuffer to an html document */


gchar *
gtk_source_buffer_convert_to_html(GtkSourceBuffer *buf, const gchar *htmltitle)
{
    char txt[3];
    GtkTextIter iter;
    gboolean font = FALSE;
    gboolean bold = FALSE;
    gboolean italic = FALSE;
    gboolean underline = FALSE;   
    GString *str=NULL;
    GSList *list=NULL;
    GtkTextTag *tag=NULL;
    GdkColor *col = NULL;
    GValue *value;
    gunichar c = 0;
    txt[1] = 0;

    g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buf), NULL);

    gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buf), &iter, 0);

    str = g_string_new("<html>\n");
    g_string_append(str,"<head>\n");
    g_string_sprintfa(str,"<title>%s</title>\n",htmltitle ? htmltitle : "GtkSourceView converter");
    g_string_append(str,"</head>\n"); 
    g_string_append(str,"<body bgcolor=white>\n"); 
    g_string_append(str,"<pre>"); 


    while(!gtk_text_iter_is_end(&iter))
    {
        c = gtk_text_iter_get_char(&iter);
        if(!tag)
        {
            list = gtk_text_iter_get_toggled_tags(&iter, TRUE);
            if(list && g_slist_last(list)->data)
            {
                tag = GTK_TEXT_TAG(g_slist_last(list)->data);
                g_slist_free(list);
            }
            if(tag && !gtk_text_iter_ends_tag(&iter,tag))
            {
                GValue val1 = {0, };
                GValue val2 = {0, };
                GValue val3 = {0, };
                value = &val1;
                g_value_init(value, GDK_TYPE_COLOR);
                g_object_get_property(G_OBJECT(tag),"foreground_gdk", value);
                col = g_value_get_boxed(value);
                if(col)
                {
                    g_string_sprintfa(str,"<font color=#%02X%02X%02X>", col->red/256, col->green/256, col->blue/256);
                    font = TRUE;
                }
                value = &val2;
                g_value_init(value, G_TYPE_INT);
                g_object_get_property(G_OBJECT(tag), "weight", value);
                if(g_value_get_int(value) == PANGO_WEIGHT_BOLD)
                {
                    g_string_append(str,"<b>");
                    bold=TRUE;
                }
                
                value = &val3;
                g_value_init(value, PANGO_TYPE_STYLE);
                g_object_get_property(G_OBJECT(tag), "style", value);
                if(g_value_get_enum(value) == PANGO_STYLE_ITALIC)
                {
                    g_string_append(str,"<i>");
                    italic=TRUE;
                }
                
            }
        }

        if(c == '<') g_string_append(str,"&lt");    
        else if(c == '>') g_string_append(str,"&gt");    
        else  
        {
            txt[0] = c;
            g_string_append(str, txt);
        }

        gtk_text_iter_forward_char(&iter);
        if(tag && gtk_text_iter_ends_tag(&iter,tag))
        {
            if(bold) g_string_append(str,"</b>");
            if(italic) g_string_append(str,"</i>");
            if(underline) g_string_append(str,"</u>");
            if(font) g_string_append(str,"</font>");
            tag = NULL;
            bold = italic = underline = font = FALSE;
        }
    }
    g_string_append(str,"</pre>"); 
    g_string_append(str,"</body>");
    g_string_append(str,"</html>");
    return g_string_free(str, FALSE);
}

void
gtk_source_buffer_line_set_marker(GtkSourceBuffer *buffer, gint line, const gchar *marker)
{
    gint line_count = 0;
    GList *new_list = NULL;

    g_return_if_fail(buffer != NULL);
    g_return_if_fail(GTK_IS_SOURCE_BUFFER(buffer));

    line_count = gtk_text_buffer_get_line_count(GTK_TEXT_BUFFER(buffer));
    if(line > line_count) return;
    gtk_source_buffer_line_remove_markers(buffer, line);
    if(marker)
    {
        new_list = g_list_append(new_list, (gpointer)g_strdup(marker));
        g_hash_table_insert(buffer->line_markers, GINT_TO_POINTER(line), (gpointer)new_list);
    }
}

/*
   Add a marker to a line.
   If the list doesnt already exist, it will call set_marker (above)
   If the list does exist, the new marker will be prepended.
   If the marker already exists, it will be removed from its current
   order and then prepended.
*/
void
gtk_source_buffer_line_add_marker(GtkSourceBuffer *buffer, gint line, const gchar *marker)
{
    gint line_count = 0;
    GList *list = NULL;
    GList *iter = NULL;
    gchar *new_marker;

    g_return_if_fail(buffer != NULL);
    g_return_if_fail(GTK_IS_SOURCE_BUFFER(buffer));

    line_count = gtk_text_buffer_get_line_count(GTK_TEXT_BUFFER(buffer));
    if(line > line_count) return;
    list = (GList *)g_hash_table_lookup(buffer->line_markers, GINT_TO_POINTER(line));
    if(list && marker)
    {
        for(iter = list; iter; iter = iter->next)
        {
            if(iter->data && !strcmp(marker, (gchar *)iter->data))
            {
                list = g_list_remove(list, (gpointer)iter->data);
                g_free(iter->data);
                break;
            }
        }
        g_hash_table_remove(buffer->line_markers, GINT_TO_POINTER(line));
        list = g_list_prepend(list, (gpointer)g_strdup(marker));
        g_hash_table_insert(buffer->line_markers, GINT_TO_POINTER(line), (gpointer)list);
    }
    else if(marker)
    {
        gtk_source_buffer_line_set_marker(buffer, line, marker);
    }
}

gint
gtk_source_buffer_line_has_markers(GtkSourceBuffer *buffer, gint line)
{
    gpointer data;
    gint count = 0;

    g_return_val_if_fail(buffer != NULL, 0);
    g_return_val_if_fail(GTK_IS_SOURCE_BUFFER(buffer), 0);

    data = g_hash_table_lookup(buffer->line_markers, GINT_TO_POINTER(line));
    if(data) count = g_list_length((GList *)data);
    return(count);
}

const GList *
gtk_source_buffer_line_get_markers(GtkSourceBuffer *buffer, gint line)
{
    g_return_val_if_fail(buffer != NULL, NULL);
    g_return_val_if_fail(GTK_IS_SOURCE_BUFFER(buffer), NULL);

    return((const GList *)g_hash_table_lookup(buffer->line_markers, GINT_TO_POINTER(line)));
}

gint
gtk_source_buffer_line_remove_markers(GtkSourceBuffer *buffer, gint line)
{
    GList *list = NULL;
    GList *iter = NULL;
    gint remove_count = 0;
    gint line_count = 0;

    g_return_val_if_fail(buffer != NULL, 0);
    g_return_val_if_fail(GTK_IS_SOURCE_BUFFER(buffer), 0);

    line_count = gtk_text_buffer_get_line_count(GTK_TEXT_BUFFER(buffer));
    if(line > line_count) return(0);
    list = (GList *)g_hash_table_lookup(buffer->line_markers, GINT_TO_POINTER(line));
    if(list)
    {
        for(iter = list; iter; iter = iter->next)
        {
            if(iter->data) g_free(iter->data);
            remove_count++;
        }
        g_hash_table_remove(buffer->line_markers, GINT_TO_POINTER(line));
        g_list_free(list);
    }
    return(remove_count);
}

gboolean
gtk_source_buffer_line_remove_marker(GtkSourceBuffer *buffer, gint line, const gchar *marker)
{
    GList *list = NULL;
    GList *iter = NULL;
    gint line_count = 0;
    gboolean removed = FALSE;

    g_return_val_if_fail(buffer != NULL, 0);
    g_return_val_if_fail(GTK_IS_SOURCE_BUFFER(buffer), 0);

    line_count = gtk_text_buffer_get_line_count(GTK_TEXT_BUFFER(buffer));
    if(line > line_count) return(0);
    list = (GList *)g_hash_table_lookup(buffer->line_markers, GINT_TO_POINTER(line));
    if(list)
    {
        for(iter = list; iter; iter = iter->next)
        {
            if(iter->data && !strcmp(marker, (gchar *)iter->data))
            {
                g_hash_table_remove(buffer->line_markers, GINT_TO_POINTER(line));
                list = g_list_remove(list, (gpointer)iter->data);
                g_hash_table_insert(buffer->line_markers, GINT_TO_POINTER(line), (gpointer)list);
                removed = TRUE;
                break;
            }
        }
        g_hash_table_remove(buffer->line_markers, GINT_TO_POINTER(line));
        g_list_free(list);
    }
    return(removed);
}

gint
gtk_source_buffer_remove_all_markers(GtkSourceBuffer *buffer, gint line_start, gint line_end)
{
    GList *data = NULL;
    GList *iter = NULL;
    gint remove_count = 0;
    gint line_count;
    gint counter;

    g_return_val_if_fail(buffer != NULL, 0);
    g_return_val_if_fail(GTK_IS_SOURCE_BUFFER(buffer), 0);

    line_count = gtk_text_buffer_get_line_count(GTK_TEXT_BUFFER(buffer));
    line_start = line_start < 0 ? 0 : line_start;
    line_end = line_end > line_count ? line_count : line_end;
    for(counter = line_start; counter <= line_end; counter++)
    {
        remove_count += gtk_source_buffer_line_remove_markers(buffer, counter);
    }
    return(remove_count);
}
