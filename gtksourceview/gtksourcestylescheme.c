/*  gtksourcestylescheme.c
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gtksourceview-i18n.h"

#include "gtksourcestylescheme.h"


static void gtk_source_style_scheme_base_init (gpointer g_class);

GType
gtk_source_style_scheme_get_type (void)
{
	static GType source_style_scheme_type = 0;

	if (!source_style_scheme_type)
	{
		static const GTypeInfo source_style_scheme_info =
		{
			sizeof (GtkSourceStyleSchemeClass),  	/* class_size */
			gtk_source_style_scheme_base_init,	/* base_init */
			NULL,			    		/* base_finalize */
		};
		
		source_style_scheme_type = g_type_register_static (G_TYPE_INTERFACE, 
								   "GtkSourceStyleScheme",
					      			   &source_style_scheme_info, 
								   0);
	}

	return source_style_scheme_type;
}

static void
gtk_source_style_scheme_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized)
    	{
		g_signal_new ("style_changed",
			      G_TYPE_FROM_CLASS (g_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GtkSourceStyleSchemeClass, style_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

      		initialized = TRUE;
    	}
}

/**
 * gtk_source_style_scheme_get_tag_style:
 * @scheme: a #GtkSourceStyleScheme.
 * @style_name: the name of a style.
 *
 * Gets the tag associated with the given @style_name in the style @scheme.
 *
 * Return value: a #GtkSourceTagStyle.
 **/
GtkSourceTagStyle *
gtk_source_style_scheme_get_tag_style (GtkSourceStyleScheme *scheme,
				       const gchar          *style_name)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);
	g_return_val_if_fail (style_name != NULL, NULL);

	return GTK_SOURCE_STYLE_SCHEME_GET_CLASS (scheme)->get_tag_style (scheme, style_name);
}

/**
 * gtk_source_style_scheme_get_name:
 * @scheme: a #GtkSourceStyleScheme.
 *
 * Gets the name of the given style @scheme.
 *
 * Return value: the name of the style scheme.
 **/
const gchar *
gtk_source_style_scheme_get_name (GtkSourceStyleScheme *scheme)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);

	return GTK_SOURCE_STYLE_SCHEME_GET_CLASS (scheme)->get_name (scheme);
}

/**
 * gtk_source_style_scheme_get_style_names:
 * @scheme: a #GtkSourceStyleScheme.
 *
 * Gets a list of style names in the given style @scheme.
 *
 * Return value: a list of 
 **/
GSList *
gtk_source_style_scheme_get_style_names (GtkSourceStyleScheme *scheme)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);

	return GTK_SOURCE_STYLE_SCHEME_GET_CLASS (scheme)->get_style_names (scheme);
}


/* Default Style Scheme */

#define GTK_TYPE_SOURCE_DEFAULT_STYLE_SCHEME            (gtk_source_default_style_scheme_get_type ())
#define GTK_SOURCE_DEFAULT_STYLE_SCHEME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_DEFAULT_STYLE_SCHEME, GtkSourceDefaultStyleScheme))
#define GTK_SOURCE_DEFAULT_STYLE_SCHEME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_DEFAULT_STYLE_SCHEME, GtkSourceDefaultStyleSchemeClass))
#define GTK_IS_SOURCE_DEFAULT_STYLE_SCHEME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_DEFAULT_STYLE_SCHEME))
#define GTK_IS_SOURCE_DEFAULT_STYLE_SCHEME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_DEFAULT_STYLE_SCHEME))
#define GTK_SOURCE_DEFAULT_STYLE_SCHEME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_DEFAULT_STYLE_SCHEME, GtkSourceDefaultStyleSchemeClass))


typedef struct _GtkSourceDefaultStyleScheme     	GtkSourceDefaultStyleScheme;
typedef struct _GtkSourceDefaultStyleSchemeClass	GtkSourceDefaultStyleSchemeClass;

struct _GtkSourceDefaultStyleScheme
{
	 GObject 	 parent_instance;

	 GHashTable	*styles;
};

struct _GtkSourceDefaultStyleSchemeClass
{
	GObjectClass 	 parent_class;
};

static GObjectClass *parent_class = NULL;

static GType  gtk_source_default_style_scheme_get_type		(void);

static void   gtk_source_default_style_scheme_class_init	(GtkSourceDefaultStyleSchemeClass *klass);
static void   gtk_source_default_style_scheme_IFace_init	(GtkSourceStyleSchemeClass     	 *iface);
static void   gtk_source_default_style_scheme_init		(GtkSourceDefaultStyleScheme	 *scheme);
static void   gtk_source_default_style_scheme_finalize		(GObject			 *object);

static GtkSourceTagStyle *gtk_source_default_style_scheme_get_tag_style 
								(GtkSourceStyleScheme *scheme,
								 const gchar          *style_name);
static const gchar *gtk_source_default_style_scheme_get_name 	(GtkSourceStyleScheme *scheme);
static GSList *gtk_source_default_style_scheme_get_style_names  (GtkSourceStyleScheme *scheme);


static GType
gtk_source_default_style_scheme_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo info =
      		{
			sizeof (GtkSourceDefaultStyleSchemeClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gtk_source_default_style_scheme_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GtkSourceDefaultStyleScheme),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gtk_source_default_style_scheme_init,
		};
      
		static const GInterfaceInfo iface_info =
		{
			(GInterfaceInitFunc) gtk_source_default_style_scheme_IFace_init, /* interface_init */
			NULL,			                         /* interface_finalize */
			NULL			                         /* interface_data */
		};

		type = g_type_register_static (G_TYPE_OBJECT, 
					       "GtkSourceDefaultStyleScheme",
					       &info, 
					       0);

		g_type_add_interface_static (type,
					     GTK_TYPE_SOURCE_STYLE_SCHEME,
					     &iface_info);
	}
	
	return type;
}

static void
gtk_source_default_style_scheme_class_init (GtkSourceDefaultStyleSchemeClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize = gtk_source_default_style_scheme_finalize;
}

static void   
gtk_source_default_style_scheme_IFace_init (GtkSourceStyleSchemeClass *iface)
{
	iface->get_tag_style 	= gtk_source_default_style_scheme_get_tag_style;
	iface->get_name		= gtk_source_default_style_scheme_get_name;
	iface->get_style_names  = gtk_source_default_style_scheme_get_style_names;
}

static GtkSourceTagStyle *
new_tag_style (gchar* foreground,
	       gchar* background,
	       gboolean bold,
	       gboolean italic)
{
	GtkSourceTagStyle *ts;

	ts = gtk_source_tag_style_new ();

	gdk_color_parse (foreground, &ts->foreground);
	ts->mask |= GTK_SOURCE_TAG_STYLE_USE_FOREGROUND;

	if (background != NULL)
	{
		gdk_color_parse (background, &ts->background);
		ts->mask |= GTK_SOURCE_TAG_STYLE_USE_BACKGROUND;
	}
	
	ts->italic	= italic;
	ts->bold	= bold;
	ts->is_default	= TRUE;

	return ts;
}


static void
gtk_source_default_style_scheme_init (GtkSourceDefaultStyleScheme *scheme)
{
	GtkSourceTagStyle *ts;

	scheme->styles = g_hash_table_new_full ((GHashFunc)g_str_hash,
				                (GEqualFunc)g_str_equal,
					        (GDestroyNotify)g_free,
					        (GDestroyNotify)gtk_source_tag_style_free);

	ts = new_tag_style ("#FF00FF", NULL, FALSE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("Base-N Integer")),
			     ts);

	ts = new_tag_style ("#FF00FF", NULL, FALSE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("Character")),
			     ts);

	ts = new_tag_style ("#0000FF", NULL, FALSE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("Comment")),
			     ts);

	ts = new_tag_style ("#2E8B57", NULL, TRUE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("Data Type")),
			     ts);

	ts = new_tag_style ("#008A8C", NULL, FALSE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("Function")),
			     ts);

	ts = new_tag_style ("#FF00FF", NULL, FALSE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("Decimal")),
			     ts);

	ts = new_tag_style ("#FF00FF", NULL, FALSE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("Floating Point")),
			     ts);

	ts = new_tag_style ("#A52A2A", NULL, TRUE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("Keyword")),
			     ts);
	
	ts = new_tag_style ("#A020F0", NULL, FALSE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("Preprocessor")),
			     ts);

	ts = new_tag_style ("#FF00FF", NULL, FALSE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("String")),
			     ts);

	ts = new_tag_style ("#FFFFFF", "#FF0000", FALSE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("Specials")),
			     ts);
 

	/* "Others" is DEPRECATED, it has been replaced by "Data Type" */
	ts = new_tag_style ("#2E8B57", NULL, TRUE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("Others")),
			     ts);

	ts = new_tag_style ("#008B8B", NULL, FALSE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("Others 2")),
			     ts);

	ts = new_tag_style ("#6A5ACD", NULL, FALSE, FALSE);
	g_hash_table_insert (scheme->styles, 
			     g_strdup (N_("Others 3")),
			     ts);
}

static void 
gtk_source_default_style_scheme_finalize (GObject *object)
{
	GtkSourceDefaultStyleScheme *scheme = GTK_SOURCE_DEFAULT_STYLE_SCHEME (object);
	
	g_hash_table_destroy (scheme->styles);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GtkSourceTagStyle *
gtk_source_default_style_scheme_get_tag_style (GtkSourceStyleScheme *scheme,
					       const gchar          *style_name)
{
	GtkSourceDefaultStyleScheme *ds;
	const gpointer *style;

	g_return_val_if_fail (GTK_IS_SOURCE_DEFAULT_STYLE_SCHEME (scheme), NULL);
	g_return_val_if_fail (style_name != NULL, NULL);

	ds = GTK_SOURCE_DEFAULT_STYLE_SCHEME (scheme);

	style = g_hash_table_lookup (ds->styles, style_name);

	return (style != NULL) ? gtk_source_tag_style_copy ((GtkSourceTagStyle *)style) : NULL;
}

static const gchar *
gtk_source_default_style_scheme_get_name (GtkSourceStyleScheme *scheme)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);

	return _("Default");
}

static void
add_style_name (gpointer key, gpointer value, gpointer user_data)
{
	GSList **l = user_data;

	*l = g_slist_append (*l, g_strdup (key));
}

static GSList *
gtk_source_default_style_scheme_get_style_names (GtkSourceStyleScheme *scheme)
{
	GtkSourceDefaultStyleScheme *ds;
	GSList *l = NULL;

	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);

	ds = GTK_SOURCE_DEFAULT_STYLE_SCHEME (scheme);

	g_hash_table_foreach (ds->styles, add_style_name, &l);

	return l;
}

/* Default style scheme */

static GtkSourceStyleScheme* default_style_scheme = NULL;

/**
 * gtk_source_style_scheme_get_default:
 *
 * Gets the default style scheme.
 *
 * Return value: a #GtkSourceStyleScheme.
 **/
GtkSourceStyleScheme *
gtk_source_style_scheme_get_default (void)
{
	if (default_style_scheme == NULL)
	{
		GtkSourceStyleScheme **ptr = &default_style_scheme;
		
		default_style_scheme = g_object_new (GTK_TYPE_SOURCE_DEFAULT_STYLE_SCHEME,
						     NULL);
		g_object_add_weak_pointer (G_OBJECT (default_style_scheme),
					   (gpointer *) ptr);
	}
	else
		g_object_ref (default_style_scheme);

	return default_style_scheme;
}
