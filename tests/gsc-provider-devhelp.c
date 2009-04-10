#include "gsc-provider-devhelp.h"
#include <devhelp/dh-base.h>
#include <devhelp/dh-link.h>
#include "gsc-utils-test.h"
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcecompletionitem.h>

#define GSC_PROVIDER_DEVHELP_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GSC_TYPE_PROVIDER_DEVHELP, GscProviderDevhelpPrivate))

struct _GscProviderDevhelpPrivate
{
	GCompletion *completion;
	GtkSourceView *view;
};

static void gsc_provider_devhelp_iface_init (GtkSourceCompletionProviderIface *iface);

G_DEFINE_TYPE_WITH_CODE (GscProviderDevhelp, 
			 gsc_provider_devhelp, 
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SOURCE_COMPLETION_PROVIDER,
				 		gsc_provider_devhelp_iface_init))

#define GSC_PROVIDER_DEVHELP_NAME "GscProviderDevhelp"

static const gchar * 
gsc_provider_devhelp_real_get_name (GtkSourceCompletionProvider *self)
{
	return GSC_PROVIDER_DEVHELP_NAME;
}

static GList * 
gsc_provider_devhelp_real_get_proposals (GtkSourceCompletionProvider *base,
				         GtkSourceCompletionTrigger  *trigger)
{
	GscProviderDevhelp *devhelp = GSC_PROVIDER_DEVHELP(base);
	
	gchar *word = gsc_get_last_word (GTK_TEXT_VIEW (devhelp->priv->view));
	
	GList *items = g_completion_complete (devhelp->priv->completion, word, NULL);
	GList *ret = NULL;
	
	for (; items; items = g_list_next(items))
	{
		DhLink *link = (DhLink *)items->data;
		ret = g_list_prepend(ret, gtk_source_completion_item_new (link->name, NULL, link->uri));
	}

	g_free(word);
	
	return g_list_reverse (ret);
}

static void
gsc_provider_devhelp_iface_init (GtkSourceCompletionProviderIface *iface)
{
	iface->get_name = gsc_provider_devhelp_real_get_name;
	iface->get_proposals = gsc_provider_devhelp_real_get_proposals;
}

static void
gsc_provider_devhelp_finalize (GObject *object)
{
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
	self->priv = GSC_PROVIDER_DEVHELP_GET_PRIVATE (self);
	
	DhBase *base = dh_base_new ();
	GList *keywords = dh_base_get_keywords (base);
	
	self->priv->completion = g_completion_new (name_from_link);
	g_completion_add_items (self->priv->completion, keywords);
	
	g_list_free (keywords);
	g_object_unref (base);
}

GscProviderDevhelp*
gsc_provider_devhelp_new (GtkSourceView *view)
{
	GscProviderDevhelp *ret = g_object_new (GSC_TYPE_PROVIDER_DEVHELP, NULL);
	ret->priv->view = view;
	
	return ret;
}
