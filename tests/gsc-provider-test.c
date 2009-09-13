/* 
 *  gsc-provider-test.c - Type here a short description of your plugin
 *
 *  Copyright (C) 2008 - perriman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gsc-provider-test.h"
#include <gtksourceview/gtksourcecompletion.h>
#include <gtksourceview/gtksourcecompletionitem.h>

#define GSC_PROVIDER_TEST_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GSC_TYPE_PROVIDER_TEST, GscProviderTestPrivate))

static void	 gsc_provider_test_iface_init	(GtkSourceCompletionProviderIface *iface);

struct _GscProviderTestPrivate
{
	gchar *name;
	GdkPixbuf *icon;
	GdkPixbuf *proposal_icon;
	
	GList *proposals;
};

G_DEFINE_TYPE_WITH_CODE (GscProviderTest,
			 gsc_provider_test,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SOURCE_COMPLETION_PROVIDER,
				 		gsc_provider_test_iface_init))

static const gchar * 
gsc_provider_test_get_name (GtkSourceCompletionProvider *self)
{
	return GSC_PROVIDER_TEST (self)->priv->name;
}

static GdkPixbuf * 
gsc_provider_test_get_icon (GtkSourceCompletionProvider *self)
{
	return GSC_PROVIDER_TEST (self)->priv->icon;
}

static GList *
append_item (GList *list, const gchar *name, GdkPixbuf *icon, const gchar *info)
{
	GtkSourceCompletionItem *prop;
	
	prop = gtk_source_completion_item_new (name, name, icon, info);
	return g_list_append (list, prop);
}

static GList *
get_proposals (GscProviderTest *provider)
{
	if (provider->priv->proposals == NULL)
	{
		GList *list = NULL;
	
		list = append_item (list, "aaabbccc", provider->priv->icon, "Info proposal 1.1");
		list = append_item (list, "aaaddccc", provider->priv->icon, "Info proposal 1.2");
		list = append_item (list, "aabbddd", provider->priv->icon, "Info proposal 1.3");
		list = append_item (list, "bbddaa", provider->priv->icon, "Info proposal 1.3");
	
		provider->priv->proposals = list;
	}
	
	return provider->priv->proposals;
}

static gboolean
gsc_provider_test_match (GtkSourceCompletionProvider *provider,
                         GtkSourceCompletionContext  *context)
{
	return TRUE;
}

static gboolean
is_word_char (gunichar ch)
{
	return g_unichar_isprint (ch) && (g_unichar_isalnum (ch) || ch == g_utf8_get_char ("_"));
}

static gchar *
get_word_at_iter (GtkTextIter *iter)
{
	GtkTextIter start = *iter;
	gint line = gtk_text_iter_get_line (iter);
	
	if (!gtk_text_iter_ends_word (iter))
	{
		return NULL;
	}
	
	if (!gtk_text_iter_backward_char (&start))
	{
		return NULL;
	}
	
	while (line == gtk_text_iter_get_line (&start) && 
	       is_word_char (gtk_text_iter_get_char (&start)) &&
	       gtk_text_iter_backward_char (&start))
	;
	
	if (gtk_text_iter_equal (iter, &start))
	{
		return NULL;
	}
	
	return gtk_text_iter_get_text (&start, iter);
}

static void
gsc_provider_test_populate (GtkSourceCompletionProvider *provider,
                            GtkSourceCompletionContext  *context)
{
	GscProviderTest *base = GSC_PROVIDER_TEST (provider);
	GList *proposals = get_proposals (base);
	GList *ret = NULL;
	
	GtkTextIter iter;
	gchar *word;
	
	gtk_source_completion_context_get_iter (context, &iter);
	word = get_word_at_iter (&iter);
	
	if (word == NULL && !gtk_source_completion_context_get_default (context))
	{
		gtk_source_completion_context_add_proposals (context,
		                                             provider,
		                                             NULL,
		                                             TRUE);
		return;
	}
	
	/* Filter proposals */
	while (proposals)
	{
		GtkSourceCompletionProposal *item = GTK_SOURCE_COMPLETION_PROPOSAL (proposals->data);
		
		if (word == NULL || 
		    g_str_has_prefix (gtk_source_completion_proposal_get_text (item),
		                      word))
		{
			ret = g_list_prepend (ret, item);
		}

		proposals = g_list_next (proposals);
	}
	
	ret = g_list_reverse (ret);	
	gtk_source_completion_context_add_proposals (context, 
	                                             provider, 
	                                             ret,
	                                             TRUE);

	g_list_free (ret);
	g_free (word);
}

static void 
gsc_provider_test_finalize (GObject *object)
{
	GscProviderTest *provider = GSC_PROVIDER_TEST (object);
	
	g_free (provider->priv->name);
	
	if (provider->priv->icon != NULL)
	{
		g_object_unref (provider->priv->icon);
	}
	
	if (provider->priv->proposal_icon != NULL)
	{
		g_object_unref (provider->priv->proposal_icon);
	}

	G_OBJECT_CLASS (gsc_provider_test_parent_class)->finalize (object);
}

static void 
gsc_provider_test_class_init (GscProviderTestClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = gsc_provider_test_finalize;
	
	g_type_class_add_private (object_class, sizeof(GscProviderTestPrivate));
}

static void
gsc_provider_test_iface_init (GtkSourceCompletionProviderIface *iface)
{
	iface->get_name = gsc_provider_test_get_name;
	iface->get_icon = gsc_provider_test_get_icon;

	iface->populate = gsc_provider_test_populate;
	iface->match = gsc_provider_test_match;
}

static void 
gsc_provider_test_init (GscProviderTest * self)
{
	GtkIconTheme *theme;
	gint width;
	
	self->priv = GSC_PROVIDER_TEST_GET_PRIVATE (self);
	
	theme = gtk_icon_theme_get_default ();

	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, NULL);
	self->priv->proposal_icon = gtk_icon_theme_load_icon (theme,
	                                                      GTK_STOCK_YES,
	                                                      width,
	                                                      GTK_ICON_LOOKUP_USE_BUILTIN,
	                                                      NULL);
}

GscProviderTest *
gsc_provider_test_new (const gchar *name,
                       GdkPixbuf   *icon)
{
	GscProviderTest *ret = g_object_new (GSC_TYPE_PROVIDER_TEST, NULL);
	
	ret->priv->name = g_strdup (name);
	
	if (icon != NULL)
	{
		ret->priv->icon = g_object_ref (icon);
	}

	return ret;
}
