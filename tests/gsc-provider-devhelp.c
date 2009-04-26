#include "gsc-provider-devhelp.h"
#include <devhelp/dh-base.h>
#include <devhelp/dh-link.h>
#include <devhelp/dh-assistant-view.h>
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcecompletionitem.h>

#define GSC_PROVIDER_DEVHELP_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GSC_TYPE_PROVIDER_DEVHELP, GscProviderDevhelpPrivate))

struct _GscProviderDevhelpPrivate
{
	DhBase *dhbase;
	GtkWidget *view;

	GList *proposals;
};

static void gsc_provider_devhelp_iface_init (GtkSourceCompletionProviderIface *iface);

G_DEFINE_TYPE_WITH_CODE (GscProviderDevhelp, 
			 gsc_provider_devhelp, 
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SOURCE_COMPLETION_PROVIDER,
				 		gsc_provider_devhelp_iface_init))

static const gchar * 
gsc_provider_devhelp_get_name (GtkSourceCompletionProvider *self)
{
	return "Devhelp";
}

static GList * 
gsc_provider_devhelp_get_proposals (GtkSourceCompletionProvider *provider,
                                    GtkTextIter                 *iter)
{
	GscProviderDevhelp *devhelp = GSC_PROVIDER_DEVHELP (provider);
	
	g_list_foreach (devhelp->priv->proposals, (GFunc)g_object_ref, NULL);

	return g_list_copy (devhelp->priv->proposals);
}

static gboolean
gsc_provider_devhelp_filter_proposal (GtkSourceCompletionProvider *provider,
                                      GtkSourceCompletionProposal *proposal,
                                      GtkTextIter                 *iter,
                                      const gchar                 *criteria)
{
	const gchar *item;
	
	item = gtk_source_completion_proposal_get_label (proposal);
	return g_str_has_prefix (item, criteria);
}

static gboolean
gsc_provider_devhelp_get_interactive(GtkSourceCompletionProvider *provider)
{
	return TRUE;
}

static GtkWidget *
gsc_provider_devhelp_get_info_widget (GtkSourceCompletionProvider *provider,
                                      GtkSourceCompletionProposal *proposal)
{
	return GSC_PROVIDER_DEVHELP (provider)->priv->view;
}

static void
gsc_provider_devhelp_update_info (GtkSourceCompletionProvider *provider,
                                  GtkSourceCompletionProposal *proposal,
                                  GtkSourceCompletionInfo     *info)
{
	GscProviderDevhelp *self = GSC_PROVIDER_DEVHELP (provider);
	const gchar *uri;
	
	uri = gtk_source_completion_proposal_get_label (proposal);

	dh_assistant_view_search (DH_ASSISTANT_VIEW (self->priv->view), uri);
}

static void
gsc_provider_devhelp_iface_init (GtkSourceCompletionProviderIface *iface)
{
	iface->get_name = gsc_provider_devhelp_get_name;
	iface->get_proposals = gsc_provider_devhelp_get_proposals;
	iface->get_interactive = gsc_provider_devhelp_get_interactive;
	iface->filter_proposal = gsc_provider_devhelp_filter_proposal;
	
	iface->get_info_widget = gsc_provider_devhelp_get_info_widget;
	iface->update_info = gsc_provider_devhelp_update_info;
}

static void
gsc_provider_devhelp_finalize (GObject *object)
{
	GscProviderDevhelp *provider = GSC_PROVIDER_DEVHELP (object);
	
	g_object_unref (provider->priv->dhbase);
	g_list_foreach (provider->priv->proposals, (GFunc)g_object_unref, NULL);

	G_OBJECT_CLASS (gsc_provider_devhelp_parent_class)->finalize (object);
}

static void
gsc_provider_devhelp_class_init (GscProviderDevhelpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = gsc_provider_devhelp_finalize;

	g_type_class_add_private (object_class, sizeof(GscProviderDevhelpPrivate));
}

static void
gsc_provider_devhelp_init (GscProviderDevhelp *self)
{
	GList *item;
	GList *ret;
	
	self->priv = GSC_PROVIDER_DEVHELP_GET_PRIVATE (self);
	
	self->priv->dhbase = dh_base_new ();
	
	ret = NULL;
	
	for (item = dh_base_get_keywords (self->priv->dhbase); item; item = g_list_next (item))
	{
		DhLink *link = (DhLink *)item->data;
		
		ret = g_list_prepend (ret, gtk_source_completion_item_new (dh_link_get_name (link),
									   dh_link_get_name (link),
									   NULL,
									   dh_link_get_uri (link)));
	}
	
	
	self->priv->view = dh_assistant_view_new ();
	dh_assistant_view_set_base (DH_ASSISTANT_VIEW (self->priv->view), self->priv->dhbase);
	
	gtk_widget_set_size_request (self->priv->view, 400, 300);

	self->priv->proposals = g_list_reverse (ret);
}

GscProviderDevhelp*
gsc_provider_devhelp_new ()
{
	GscProviderDevhelp *ret = g_object_new (GSC_TYPE_PROVIDER_DEVHELP, NULL);
	
	return ret;
}
