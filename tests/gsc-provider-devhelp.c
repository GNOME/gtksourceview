#include "gsc-provider-devhelp.h"
#include <devhelp/dh-base.h>
#include <devhelp/dh-link.h>
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcecompletionitem.h>

#define GSC_PROVIDER_DEVHELP_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GSC_TYPE_PROVIDER_DEVHELP, GscProviderDevhelpPrivate))

struct _GscProviderDevhelpPrivate
{
	DhBase *dhbase;
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
gsc_provider_devhelp_get_proposals (GtkSourceCompletionProvider *provider)
{
	GscProviderDevhelp *devhelp = GSC_PROVIDER_DEVHELP (provider);
	
	g_list_foreach (devhelp->priv->proposals, (GFunc)g_object_ref, NULL);

	return g_list_copy (devhelp->priv->proposals);
}

static gboolean
gsc_provider_devhelp_filter_proposal (GtkSourceCompletionProvider *provider,
                                      GtkSourceCompletionProposal *proposal,
                                      const gchar                 *criteria)
{
	const gchar *item;
	
	item = gtk_source_completion_proposal_get_label (proposal);
	return g_str_has_prefix (item, criteria);
}

static gboolean
gsc_provider_devhelp_can_auto_complete (GtkSourceCompletionProvider *provider)
{
	return TRUE;
}

static void
gsc_provider_devhelp_iface_init (GtkSourceCompletionProviderIface *iface)
{
	iface->get_name = gsc_provider_devhelp_get_name;
	iface->get_proposals = gsc_provider_devhelp_get_proposals;
	iface->can_auto_complete = gsc_provider_devhelp_can_auto_complete;
	iface->filter_proposal = gsc_provider_devhelp_filter_proposal;
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

static gchar *
name_from_link (gpointer data)
{
	DhLink *link = (DhLink *)data;
	
	return link->name;
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
		
		ret = g_list_prepend (ret, gtk_source_completion_item_new (link->name, NULL, link->uri));
	}
	
	self->priv->proposals = g_list_reverse (ret);
}

GscProviderDevhelp*
gsc_provider_devhelp_new ()
{
	GscProviderDevhelp *ret = g_object_new (GSC_TYPE_PROVIDER_DEVHELP, NULL);
	
	return ret;
}
