#include "gsc-provider-devhelp.h"
#include <devhelp/dh-base.h>
#include <devhelp/dh-link.h>
#include <devhelp/dh-assistant-view.h>
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcecompletionitem.h>

#define GSC_PROVIDER_DEVHELP_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GSC_TYPE_PROVIDER_DEVHELP, GscProviderDevhelpPrivate))

#define PROCESS_BATCH 300
#define MAX_ITEMS 5000

typedef struct
{
	GObject parent;
	
	DhLink *link;
} GscDevhelpItem;

typedef struct
{
	GObjectClass parent_class;
} GscDevhelpItemClass;

struct _GscProviderDevhelpPrivate
{
	DhBase *dhbase;
	GtkWidget *view;
	GdkPixbuf *icon;

	GList *dhptr;
	GList *proposals;
	
	GtkSourceCompletionContext *context;
	GList *idleptr;
	guint idle_id;
	guint cancel_id;
	guint counter;
};

GType gsc_devhelp_item_get_type (void);

static void gsc_provider_devhelp_iface_init (GtkSourceCompletionProviderIface *iface);
static void gsc_devhelp_item_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (GscProviderDevhelp, 
			 gsc_provider_devhelp, 
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SOURCE_COMPLETION_PROVIDER,
				 		gsc_provider_devhelp_iface_init))

G_DEFINE_TYPE_WITH_CODE (GscDevhelpItem, 
			 gsc_devhelp_item, 
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SOURCE_COMPLETION_PROPOSAL,
			 			gsc_devhelp_item_iface_init))

static const gchar *
gsc_devhelp_item_get_text_impl (GtkSourceCompletionProposal *proposal)
{
	return dh_link_get_name (((GscDevhelpItem *)proposal)->link);
}

static void
gsc_devhelp_item_iface_init (gpointer g_iface, 
                             gpointer iface_data)
{
	GtkSourceCompletionProposalIface *iface = (GtkSourceCompletionProposalIface *)g_iface;
	
	/* Interface data getter implementations */
	iface->get_label = gsc_devhelp_item_get_text_impl;
	iface->get_text = gsc_devhelp_item_get_text_impl;
}

static void
gsc_devhelp_item_init (GscDevhelpItem *object)
{
}

static void
gsc_devhelp_item_class_init (GscDevhelpItemClass *klass)
{
}

static const gchar * 
gsc_provider_devhelp_get_name (GtkSourceCompletionProvider *self)
{
	return "Devhelp";
}

static void
population_finished (GscProviderDevhelp *devhelp)
{
	if (devhelp->priv->idle_id != 0)
	{
		g_source_remove (devhelp->priv->idle_id);
		devhelp->priv->idle_id = 0;
	}
	
	if (devhelp->priv->context != NULL)
	{
		if (devhelp->priv->cancel_id)
		{
			g_signal_handler_disconnect (devhelp->priv->context, devhelp->priv->cancel_id);
			devhelp->priv->cancel_id = 0;
		}
	
		g_object_unref (devhelp->priv->context);
		devhelp->priv->context = NULL;
	}
}

static void
fill_proposals (GscProviderDevhelp *devhelp)
{
	GList *item;
	GList *ret = NULL;

	if (devhelp->priv->dhbase == NULL)
	{
		devhelp->priv->dhbase = dh_base_new ();
		devhelp->priv->view = dh_assistant_view_new ();

		dh_assistant_view_set_base (DH_ASSISTANT_VIEW (devhelp->priv->view), 
		                            devhelp->priv->dhbase);
		                            
		gtk_widget_set_size_request (devhelp->priv->view, 400, 300);
	}
	
	for (item = dh_base_get_keywords (devhelp->priv->dhbase); item; item = g_list_next (item))
	{
		GscDevhelpItem *proposal = g_object_new (gsc_devhelp_item_get_type (), 
		                                         NULL);

		proposal->link = (DhLink *)item->data;
		ret = g_list_prepend (ret, proposal);			                
	}
	
	devhelp->priv->proposals = g_list_reverse (ret);
	devhelp->priv->idleptr = devhelp->priv->proposals;
}

static gboolean
is_word_char (gunichar ch)
{
	return g_unichar_isprint (ch) && (g_unichar_isalnum (ch) || ch == '_' || ch == ':');
}

static gchar *
get_word_at_iter (GtkTextIter *iter)
{
	GtkTextIter start = *iter;
	gint line = gtk_text_iter_get_line (iter);
	
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

static gboolean
add_in_idle (GscProviderDevhelp *devhelp)
{
	guint idx = 0;
	GList *ret = NULL;
	GtkTextIter iter;
	gchar *word;
	gboolean finished;
	
	if (devhelp->priv->proposals == NULL)
	{
		fill_proposals (devhelp);
	}

	gtk_source_completion_context_get_iter (devhelp->priv->context,
	                                        &iter);
	word = get_word_at_iter (&iter);
	
	if (word == NULL)
	{
		gtk_source_completion_context_add_proposals (devhelp->priv->context,
	                                                 GTK_SOURCE_COMPLETION_PROVIDER (devhelp),
	                                                 NULL,
	                                                 TRUE);
		population_finished (devhelp);
		return FALSE;
	}
	
	while (idx < PROCESS_BATCH && devhelp->priv->idleptr != NULL)
	{
		GtkSourceCompletionProposal *proposal;
		
		proposal = GTK_SOURCE_COMPLETION_PROPOSAL (devhelp->priv->idleptr->data);
		
		if (g_str_has_prefix (gtk_source_completion_proposal_get_text (proposal),
		                      word))
		{
			ret = g_list_prepend (ret, proposal);
			
			if (++devhelp->priv->counter >= MAX_ITEMS)
			{
				break;
			}
		}
		
		++idx;
		devhelp->priv->idleptr = g_list_next (devhelp->priv->idleptr);
	}
	
	finished = devhelp->priv->idleptr == NULL || devhelp->priv->counter >= MAX_ITEMS;
	ret = g_list_reverse (ret);
	
	gtk_source_completion_context_add_proposals (devhelp->priv->context,
	                                             GTK_SOURCE_COMPLETION_PROVIDER (devhelp),
	                                             ret,
	                                             finished);
	
	if (finished)
	{
		population_finished (devhelp);
	}
	
	g_free (word);

	return !finished;
}

static void
gsc_provider_devhelp_populate (GtkSourceCompletionProvider *provider,
                               GtkSourceCompletionContext  *context)
{
	GscProviderDevhelp *devhelp = GSC_PROVIDER_DEVHELP (provider);
	
	devhelp->priv->cancel_id = 
		g_signal_connect_swapped (context, 
			                      "cancelled", 
			                      G_CALLBACK (population_finished), 
			                      provider);
	
	devhelp->priv->counter = 0;
	devhelp->priv->idleptr = devhelp->priv->proposals;
	devhelp->priv->context = g_object_ref (context);
	devhelp->priv->idle_id = g_idle_add ((GSourceFunc)add_in_idle,
	                                     devhelp);
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
	const gchar *uri = dh_link_get_name (((GscDevhelpItem *)proposal)->link);

	dh_assistant_view_search (DH_ASSISTANT_VIEW (self->priv->view), uri);
}

static GdkPixbuf *
gsc_provider_devhelp_get_icon (GtkSourceCompletionProvider *provider)
{
	return GSC_PROVIDER_DEVHELP (provider)->priv->icon;
}

static void
gsc_provider_devhelp_iface_init (GtkSourceCompletionProviderIface *iface)
{
	iface->get_name = gsc_provider_devhelp_get_name;
	iface->populate = gsc_provider_devhelp_populate;
	
	iface->get_info_widget = gsc_provider_devhelp_get_info_widget;
	iface->update_info = gsc_provider_devhelp_update_info;
	
	iface->get_icon = gsc_provider_devhelp_get_icon;
}

static void
gsc_provider_devhelp_finalize (GObject *object)
{
	GscProviderDevhelp *provider = GSC_PROVIDER_DEVHELP (object);
	
	
	
	g_object_unref (provider->priv->dhbase);
	g_list_foreach (provider->priv->proposals, (GFunc)g_object_unref, NULL);
	
	if (provider->priv->icon)
	{
		g_object_unref (provider->priv->icon);
	}
	
	G_OBJECT_CLASS (gsc_provider_devhelp_parent_class)->finalize (object);
}

static void
gsc_provider_devhelp_dispose (GObject *object)
{
	population_finished (GSC_PROVIDER_DEVHELP (object));
	
	G_OBJECT_CLASS (gsc_provider_devhelp_parent_class)->dispose (object);
}

static void
gsc_provider_devhelp_class_init (GscProviderDevhelpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = gsc_provider_devhelp_finalize;
	object_class->dispose = gsc_provider_devhelp_dispose;

	g_type_class_add_private (object_class, sizeof(GscProviderDevhelpPrivate));
}

static void
gsc_provider_devhelp_init (GscProviderDevhelp *self)
{
	self->priv = GSC_PROVIDER_DEVHELP_GET_PRIVATE (self);
	
	self->priv->icon = gdk_pixbuf_new_from_file ("/usr/share/icons/hicolor/16x16/apps/devhelp.png", NULL);
}

GscProviderDevhelp *
gsc_provider_devhelp_new ()
{
	GscProviderDevhelp *ret = g_object_new (GSC_TYPE_PROVIDER_DEVHELP, NULL);
	
	return ret;
}
