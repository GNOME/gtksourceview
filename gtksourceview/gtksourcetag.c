/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *  gtksourcetag.c
 *
 *  Copyright (C) 2001
 *  Mikael Hermansson<tyan@linux.se>
 *  Chris Phelps <chicane@reninet.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gtksourceview-i18n.h"

#include "gtksourcetag.h"

struct _GtkSourceTag 
{
	GtkTextTag		 parent_instance;

	gchar			*id;
	GtkSourceTagStyle	*style;
};

struct _GtkSourceTagClass 
{
	GtkTextTagClass		 parent_class; 
};

static GtkTextTagClass          *parent_class = NULL;

static void		 gtk_source_tag_init 		(GtkSourceTag       *text_tag);
static void		 gtk_source_tag_class_init 	(GtkSourceTagClass  *text_tag);
static void		 gtk_source_tag_finalize	(GObject            *object);

static void 		 gtk_source_tag_set_property	(GObject            *object,
							 guint               prop_id,
							 const GValue       *value,
							 GParamSpec         *pspec);
static void 		 gtk_source_tag_get_property 	(GObject            *object,
							 guint               prop_id,
							 GValue             *value,
							 GParamSpec         *pspec);

enum {
	PROP_0,
	PROP_ID,
	PROP_TAG_STYLE
};

/* Source tag */

GType
gtk_source_tag_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0) {
		static const GTypeInfo our_info = {
			sizeof (GtkSourceTagClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gtk_source_tag_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (GtkSourceTag),
			0,	/* n_preallocs */
			(GInstanceInitFunc) gtk_source_tag_init
		};

		our_type =
		    g_type_register_static (GTK_TYPE_TEXT_TAG,
					    "GtkSourceTag", &our_info, 0);
	}

	return our_type;
}

static void
gtk_source_tag_class_init (GtkSourceTagClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize	= gtk_source_tag_finalize;

	object_class->set_property = gtk_source_tag_set_property;
	object_class->get_property = gtk_source_tag_get_property;
  
	/* Construct */
	g_object_class_install_property (object_class,
        	                         PROP_ID,
                                   	 g_param_spec_string ("id",
                                                        _("Tag ID"),
                                                        _("ID used to refer to the source tag"),
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
        	                         PROP_TAG_STYLE,
                                   	 g_param_spec_boxed ("tag_style",
                                                       _("Tag style"),
                                                       _("The style associated with the source tag"),
                                                       GTK_TYPE_SOURCE_TAG_STYLE,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));

}

static void
gtk_source_tag_init (GtkSourceTag *text_tag)
{
	text_tag->id = NULL;
	text_tag->style = NULL;
}

static void
gtk_source_tag_finalize (GObject *object)
{
	GtkSourceTag *tag;

	tag = GTK_SOURCE_TAG (object);
	
	g_free (tag->style);
	g_free (tag->id);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gtk_source_tag_get_style:
 * @tag: a #GtkSourceTag.
 * 
 * Gets the style associated with the given @tag.
 *
 * Return value: a #GtkSourceTagStyle if found, or %NULL if not found. 
 **/
GtkSourceTagStyle *
gtk_source_tag_get_style (GtkSourceTag *tag)
{
	g_return_val_if_fail (GTK_IS_SOURCE_TAG (tag), NULL);

	if (tag->style != NULL)
		return gtk_source_tag_style_copy (tag->style);
	else
		return NULL;
}

/**
 * gtk_source_tag_set_style:
 * @tag: a #GtkSourceTag.
 * @style: a #GtkSourceTagStyle.
 * 
 * Associates a given @style with the given @tag.
 **/
void 
gtk_source_tag_set_style (GtkSourceTag *tag, const GtkSourceTagStyle *style)
{
	GValue foreground = { 0, };
	GValue background = { 0, };

	g_return_if_fail (GTK_IS_SOURCE_TAG (tag));
	g_return_if_fail (style != NULL);

	/* Foreground color */
	g_value_init (&foreground, GDK_TYPE_COLOR);
	
	if ((style->mask & GTK_SOURCE_TAG_STYLE_USE_FOREGROUND) != 0)
		g_value_set_boxed (&foreground, &style->foreground);
	else
		g_value_set_boxed (&foreground, NULL);
	
	g_object_set_property (G_OBJECT (tag), "foreground_gdk", &foreground);

	/* Background color */
	g_value_init (&background, GDK_TYPE_COLOR);

	if ((style->mask & GTK_SOURCE_TAG_STYLE_USE_BACKGROUND) != 0)
		g_value_set_boxed (&background, &style->background);
	else
		g_value_set_boxed (&background, NULL);

	g_object_set_property (G_OBJECT (tag), "background_gdk", &background);
		
	g_object_set (G_OBJECT (tag), 
		      "style", style->italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL,
		      "weight", style->bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		      "strikethrough", style->strikethrough,
		      "underline", style->underline ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE,
		      NULL);

	g_free (tag->style);

	tag->style = g_new0 (GtkSourceTagStyle, 1);

	*tag->style = *style;

	g_object_notify (G_OBJECT (tag), "tag_style");
}

static void
gtk_source_tag_set_property (GObject            *object,
			     guint               prop_id,
			     const GValue       *value,
			     GParamSpec         *pspec)
{
	GtkSourceTag *tag;

	g_return_if_fail (GTK_IS_SOURCE_TAG (object));

	tag = GTK_SOURCE_TAG (object);

	switch (prop_id)
	{
		case PROP_ID:
			g_return_if_fail (tag->id == NULL);
			tag->id = g_strdup (g_value_get_string (value));
			break;

		case PROP_TAG_STYLE:
			{
				const GtkSourceTagStyle *style;

				style = g_value_get_boxed (value);

				if (style != NULL)
					gtk_source_tag_set_style (tag, style);
			}
				
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    	}
}

static void
gtk_source_tag_get_property (GObject      *object,
                             guint         prop_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
	GtkSourceTag *tag;

	g_return_if_fail (GTK_IS_SOURCE_TAG (object));

	tag = GTK_SOURCE_TAG (object);

	switch (prop_id)
	{
		case PROP_ID:
			g_value_set_string (value, tag->id);
			break;

		case PROP_TAG_STYLE:
			{
				GtkSourceTagStyle *style;
				
				style = gtk_source_tag_get_style (tag);

				g_value_set_boxed (value, style);

				if (style != NULL)
					gtk_source_tag_style_free (style);
				
				break;
			}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

gchar *
gtk_source_tag_get_id (GtkSourceTag *tag)
{
	g_return_val_if_fail (GTK_IS_SOURCE_TAG (tag), NULL);
	g_return_val_if_fail (tag->id != NULL, NULL);

	return g_strdup (tag->id);
}

GtkTextTag *
gtk_source_tag_new (const gchar *id,
		    const gchar *name)
{
	GtkSourceTag *tag;

	g_return_val_if_fail (id != NULL, NULL);

	tag = GTK_SOURCE_TAG (g_object_new (GTK_TYPE_SOURCE_TAG, 
					    "id", id,
					    "name", name,
					    NULL));
	
	return GTK_TEXT_TAG (tag);
}

/* GtkSourceTagStyle functions ------------- */

/**
 * gtk_source_tag_style_get_type:
 * 
 * Retrieves the GType object which is associated with the
 * #GtkSourceTagStyle class.
 * 
 * Return value: the GType associated with #GtkSourceTagStyle.
 **/
GType 
gtk_source_tag_style_get_type (void)
{
	static GType our_type = 0;

	if (!our_type)
		our_type = g_boxed_type_register_static (
			"GtkSourceTagStyle",
			(GBoxedCopyFunc) gtk_source_tag_style_copy,
			(GBoxedFreeFunc) gtk_source_tag_style_free);

	return our_type;
} 

/**
 * gtk_source_tag_style_new:
 * 
 * Creates a new tag style object.
 * 
 * Return value: a new #GtkSourceTagStyle.
 **/
GtkSourceTagStyle *
gtk_source_tag_style_new (void)
{
	GtkSourceTagStyle *style;

	style = g_new0 (GtkSourceTagStyle, 1);

	return style;
}

/**
 * gtk_source_tag_style_copy:
 * @style: a #GtkSourceTagStyle.
 * 
 * Makes a copy of the given @style.
 * 
 * Return value: a new #GtkSourceTagStyle.
 **/
GtkSourceTagStyle *
gtk_source_tag_style_copy (const GtkSourceTagStyle *style)
{
	GtkSourceTagStyle *new_style;

	g_return_val_if_fail (style != NULL, NULL);
	
	new_style = gtk_source_tag_style_new ();
	*new_style = *style;

	return new_style;
}


/**
 * gtk_source_tag_style_free:
 * @style: a #GtkSourceTagStyle.
 * 
 * Frees the resources allocated by the given @style.
 * 
 **/
void 
gtk_source_tag_style_free (GtkSourceTagStyle *style)
{
	g_return_if_fail (style != NULL);
	
	g_free (style);
}


