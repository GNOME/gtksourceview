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
	GtkSourceCompletionPage *page;
};

G_DEFINE_TYPE_WITH_CODE (GscProviderTest,
			 gsc_provider_test,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SOURCE_COMPLETION_PROVIDER,
				 		gsc_provider_test_iface_init))

static const gchar * 
gsc_provider_test_real_get_name (GtkSourceCompletionProvider *self)
{
	return GSC_PROVIDER_TEST_NAME;
}

static GList *
append_item (GList *list, const gchar *name, GdkPixbuf *icon, const gchar *info, GtkSourceCompletionPage *page)
{
	GtkSourceCompletionItem *prop;
	
	prop = gtk_source_completion_item_new (name, icon, info);
	g_object_set_data (G_OBJECT (prop), "GscProviderTestPage", page);

	return g_list_append (list, prop);
}

static GList *
gsc_provider_test_real_get_proposals (GtkSourceCompletionProvider *base,
				      GtkSourceCompletionTrigger  *trigger)
{
	GscProviderTest *self = GSC_PROVIDER_TEST (base);
	GList *list = NULL;
	
	list = append_item (list, "Proposal 1.1", NULL, "Info proposal 1.1", NULL);
	list = append_item (list, "Proposal 1.2", NULL, "Info proposal 1.2", NULL);
	list = append_item (list, "Proposal 1.3", NULL, "Info proposal 1.3", NULL);
	
	list = append_item (list, "Proposal 2.1", NULL, "Info proposal 2.1", self->priv->page);
	list = append_item (list, "Proposal 2.2", NULL, "Info proposal 2.2", self->priv->page);
	list = append_item (list, "Proposal 2.3", NULL, "Info proposal 2.3", self->priv->page);

	return list;
}

static void 
gsc_provider_test_finalize (GObject *object)
{
	G_OBJECT_CLASS (gsc_provider_test_parent_class)->finalize (object);
}


static void 
gsc_provider_test_class_init (GscProviderTestClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = gsc_provider_test_finalize;
	
	g_type_class_add_private (object_class, sizeof(GscProviderTestPrivate));
}

static GtkSourceCompletionPage *
gsc_provider_test_real_get_page (GtkSourceCompletionProvider *provider,
                                 GtkSourceCompletionProposal *proposal)
{
	return g_object_get_data (G_OBJECT (proposal), "GscProviderTestPage");
}

static void
gsc_provider_test_iface_init (GtkSourceCompletionProviderIface *iface)
{
	iface->get_name = gsc_provider_test_real_get_name;
	iface->get_proposals = gsc_provider_test_real_get_proposals;
	iface->get_page = gsc_provider_test_real_get_page;
}

static void 
gsc_provider_test_init (GscProviderTest * self)
{
	self->priv = GSC_PROVIDER_TEST_GET_PRIVATE (self);
}

GscProviderTest *
gsc_provider_test_new (GtkSourceCompletionPage *page)
{
	GscProviderTest *ret = g_object_new (GSC_TYPE_PROVIDER_TEST, NULL);
	ret->priv->page = page;
	
	return ret;
}
