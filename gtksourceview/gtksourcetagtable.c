/*  gtksourcetagtable.c
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

#include "gtksourcetagtable.h"

#include "gtksourceview-marshal.h"
#include "gtksourcetag.h"

struct _GtkSourceTagTablePrivate
{
	gulong tag_added_id;
	gulong tag_removed_id;
	gulong tag_changed_id;
};

enum
{
	CHANGED,
	LAST_SIGNAL
};

static GtkTextTagTableClass *parent_class = NULL;

static void  gtk_source_tag_table_class_init		(GtkSourceTagTableClass *klass);
static void  gtk_source_tag_table_instance_init	(GtkSourceTagTable *lm);
static void	 gtk_source_tag_table_finalize		(GObject 			*object);

static guint signals[LAST_SIGNAL] = { 0 };

GType
gtk_source_tag_table_get_type (void)
{
	static GType tag_table_type = 0;

  	if (tag_table_type == 0)
    	{
      		static const GTypeInfo our_info =
      		{
        		sizeof (GtkSourceTagTableClass),
        		NULL,		/* base_init */
        		NULL,		/* base_finalize */
        		(GClassInitFunc) gtk_source_tag_table_class_init,
        		NULL,           /* class_finalize */
        		NULL,           /* class_data */
        		sizeof (GtkSourceTagTable),
        		0,              /* n_preallocs */
        		(GInstanceInitFunc) gtk_source_tag_table_instance_init
      		};

      		tag_table_type = g_type_register_static (GTK_TYPE_TEXT_TAG_TABLE,
                					    "GtkSourceTagTable",
							    &our_info,
							    0);
    	}

	return tag_table_type;
}

static void
gtk_source_tag_table_class_init (GtkSourceTagTableClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class		= g_type_class_peek_parent (klass);
	object_class->finalize	= gtk_source_tag_table_finalize;

	signals[CHANGED] =
		g_signal_new ("changed",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (GtkSourceTagTableClass, changed),
				NULL,
				NULL,
				gtksourceview_marshal_VOID__VOID,
				G_TYPE_NONE,
				0);
}

static void 
tag_added_or_removed_cb (GtkTextTagTable *table, GtkTextTag *tag)
{
	g_signal_emit (table, signals[CHANGED], 0);
}

static void 
tag_changed_cb (GtkTextTagTable *table, GtkTextTag *tag, gboolean size_changed)
{
	g_signal_emit (table, signals[CHANGED], 0);
}

static void
gtk_source_tag_table_instance_init (GtkSourceTagTable *tt)
{

	tt->priv = g_new0 (GtkSourceTagTablePrivate, 1);

	tt->priv->tag_added_id =
		g_signal_connect (G_OBJECT (tt),
				  "tag_added",
				  G_CALLBACK (tag_added_or_removed_cb),
				  NULL);

	tt->priv->tag_removed_id =
		g_signal_connect (G_OBJECT (tt),
				  "tag_removed",
				  G_CALLBACK (tag_added_or_removed_cb),
				  NULL);

	tt->priv->tag_changed_id =
		g_signal_connect (G_OBJECT (tt),
				  "tag_changed",
				  G_CALLBACK (tag_changed_cb),
				  NULL);			  
}

static void
gtk_source_tag_table_finalize (GObject *object)
{
	GtkSourceTagTable *tt = GTK_SOURCE_TAG_TABLE (object);
	
	g_free (tt->priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/**
 * gtk_source_tag_table_new:
 * 
 * Creates a new #GtkSourceTagTable. The table contains no tags by
 * default.
 * 
 * Return value: a new #GtkSourceTagTable
 **/

GtkSourceTagTable *
gtk_source_tag_table_new (void)
{
	GtkSourceTagTable *table;

	table = g_object_new (GTK_TYPE_SOURCE_TAG_TABLE, NULL);

	return table;
	
}

static void
block_signals (GtkSourceTagTable *tt)
{
	g_signal_handler_block (tt, tt->priv->tag_added_id); 
	g_signal_handler_block (tt, tt->priv->tag_removed_id); 
	g_signal_handler_block (tt, tt->priv->tag_changed_id); 
}

static void
unblock_signals (GtkSourceTagTable *tt)
{
	g_signal_handler_unblock (tt, tt->priv->tag_added_id); 
	g_signal_handler_unblock (tt, tt->priv->tag_removed_id); 
	g_signal_handler_unblock (tt, tt->priv->tag_changed_id); 
}

/**
 * gtk_source_tag_table_add_tags:
 * @table: a #GtkSourceTagTable.
 * @tags: a #GSList containing #GtkTextTag objects.
 *
 * Adds a list of tag to the table. The added tags are assigned the highest priority
 * in the table.
 *
 * If a tag is already present in table or has the same name as an already-added tag,
 * then it is not added to the table.
 **/
void 
gtk_source_tag_table_add_tags (GtkSourceTagTable *table, const GSList *tags)
{
	gint old_size;
	
	g_return_if_fail (GTK_IS_SOURCE_TAG_TABLE (table));

	old_size = gtk_text_tag_table_get_size (GTK_TEXT_TAG_TABLE (table));

	block_signals (table);
	
	while (tags != NULL)
	{
		gtk_text_tag_table_add (GTK_TEXT_TAG_TABLE (table), GTK_TEXT_TAG (tags->data));

		tags = g_slist_next (tags);
	}

	unblock_signals (table);

	if (gtk_text_tag_table_get_size (GTK_TEXT_TAG_TABLE (table)) != old_size)
	{
		g_signal_emit (table, signals[CHANGED], 0);
	}
}

static void
foreach_remove_tag (GtkTextTag *tag, gpointer data)
{
	GSList **tags = data;

	if (GTK_IS_SOURCE_TAG (tag))
		*tags = g_slist_prepend (*tags, tag);
}

/**
 * gtk_source_tag_table_remove_source_tags:
 * @table: a #GtkSourceTagTable.
 * 
 * Removes all the source tags from the table. This will remove the table's
 * reference to the tags, so be careful - tags will end
 * up destroyed if you don't have a reference to them.
 **/
void 
gtk_source_tag_table_remove_source_tags (GtkSourceTagTable *table)
{
	GSList *tags = NULL;
	GSList *l;
	gint old_size;

	g_return_if_fail (GTK_IS_SOURCE_TAG_TABLE (table));

	old_size = gtk_text_tag_table_get_size (GTK_TEXT_TAG_TABLE (table));

	block_signals (table);

	gtk_text_tag_table_foreach (GTK_TEXT_TAG_TABLE (table), foreach_remove_tag, &tags);

	l = tags;
	while (l)
	{
		gtk_text_tag_table_remove (GTK_TEXT_TAG_TABLE (table),
					   GTK_TEXT_TAG (l->data));
		l = g_slist_next (l);
	}
	g_slist_free (tags);

	unblock_signals (table);

	if (gtk_text_tag_table_get_size (GTK_TEXT_TAG_TABLE (table)) != old_size)
		g_signal_emit (table, signals[CHANGED], 0);
}

