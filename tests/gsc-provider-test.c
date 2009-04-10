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

static void	 gsc_provider_test_iface_init	(GtkSourceCompletionProviderIface *iface);

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

static GList* 
gsc_provider_test_real_get_proposals (GtkSourceCompletionProvider *base,
				      GtkSourceCompletionTrigger  *trigger)
{
	GList *list = NULL;
	GtkSourceCompletionItem *prop;
	
	prop = gtk_source_completion_item_new ("Proposal 1",
	                                       NULL,
	                                       "Info proposal 1");

	list = g_list_append (list, prop);
	prop = gtk_source_completion_item_new ("Proposal 2",
	                                       NULL,
	                                       "Info proposal 2");

	list = g_list_append (list, prop);
	prop = gtk_source_completion_item_new ("Proposal 3",
	                                       NULL,
	                                       "Info proposal 3");
	list = g_list_append (list, prop);
	
	/*Page 2*/
	/*prop = gtk_source_completion_proposal_new("Proposal 1,2",
				"Info proposal 1,2",
				NULL);
	gtk_source_completion_proposal_set_page_name(prop,"Page 2");
	list = g_list_append (list, prop);
	prop = gtk_source_completion_proposal_new("Proposal 2,2",
				"Info proposal 2,2",
				NULL);
	gtk_source_completion_proposal_set_page_name(prop,"Page 2");
	list = g_list_append (list, prop);
	prop = gtk_source_completion_proposal_new("Proposal 3,3",
				"Info proposal 3,3",
				NULL);
	gtk_source_completion_proposal_set_page_name(prop,"Page 3");
	list = g_list_append (list, prop);
	prop = gtk_source_completion_proposal_new("Proposal Fixed page",
				"Info proposal fixed",
				NULL);
	gtk_source_completion_proposal_set_page_name(prop,"Fixed");
	list = g_list_append (list, prop);*/
	return list;
}

static void 
gsc_provider_test_real_finish (GtkSourceCompletionProvider* base)
{

}

static void 
gsc_provider_test_finalize (GObject *object)
{
	G_OBJECT_CLASS (gsc_provider_test_parent_class)->finalize (object);
}


static void 
gsc_provider_test_class_init (GscProviderTestClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = gsc_provider_test_finalize;
}

static void
gsc_provider_test_iface_init (GtkSourceCompletionProviderIface *iface)
{
	iface->get_name = gsc_provider_test_real_get_name;
	iface->get_proposals = gsc_provider_test_real_get_proposals;
	iface->finish = gsc_provider_test_real_finish;
}


static void 
gsc_provider_test_init (GscProviderTest * self)
{
}

GscProviderTest *
gsc_provider_test_new ()
{
	return GSC_PROVIDER_TEST (g_object_new (GSC_TYPE_PROVIDER_TEST, NULL));
}

