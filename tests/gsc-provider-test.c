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
#include <gtksourceview/gtksourcecompletionitem.h>

#define GSC_PROVIDER_TEST_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GSC_TYPE_PROVIDER_TEST, GscProviderTestPrivate))

static void	 gsc_provider_test_iface_init	(GtkSourceCompletionProviderIface *iface);

struct _GscProviderTestPrivate
{
	gchar *name;
	GdkPixbuf *icon;
	GdkPixbuf *proposal_icon;
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
	
	prop = gtk_source_completion_item_new (name, icon, info);
	return g_list_append (list, prop);
}

static GList *
gsc_provider_test_get_proposals (GtkSourceCompletionProvider *base)
{
	GscProviderTest *provider = GSC_PROVIDER_TEST (base);
	GList *list = NULL;
	
	list = append_item (list, "aa", provider->priv->proposal_icon, "Info proposal 1.1");
	list = append_item (list, "ab", provider->priv->proposal_icon, "Info proposal 1.2");
	list = append_item (list, "bc", provider->priv->proposal_icon, "Info proposal 1.3");
	list = append_item (list, "bd", provider->priv->proposal_icon, "Info proposal 1.3");
	
	return list;
}

static gboolean
gsc_provider_test_filter_proposal (GtkSourceCompletionProvider *provider,
                                   GtkSourceCompletionProposal *proposal,
                                   const gchar                 *criteria)
{
	const gchar *label;
	
	label = gtk_source_completion_proposal_get_label (proposal);
	return g_str_has_prefix (label, criteria);
}

static gboolean
gsc_provider_test_get_interactive (GtkSourceCompletionProvider *provider)
{
	return TRUE;
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

	iface->get_proposals = gsc_provider_test_get_proposals;
	iface->filter_proposal = gsc_provider_test_filter_proposal;
	iface->get_interactive = gsc_provider_test_get_interactive;
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
