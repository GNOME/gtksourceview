#include "gsc-provider-devhelp.h"
#include <devhelp/dh-base.h>
#include <devhelp/dh-link.h>
#include <devhelp/dh-assistant-view.h>
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcecompletionitem.h>
#include <string.h>

#define GSC_PROVIDER_DEVHELP_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GSC_TYPE_PROVIDER_DEVHELP, GscProviderDevhelpPrivate))

#define POPULATE_BATCH 500
#define PROCESS_BATCH 300
#define MAX_ITEMS 5000

typedef struct
{
	GObject parent;
	
	DhLink *link;
	gchar *word;
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
	
	GtkTextMark *completion_mark;
	gchar *word;
	gint word_len;

	GSequenceIter *dhptr;
	GSequence *proposals;
	GSequenceIter *populate_iter;
	
	GtkSourceCompletionContext *context;
	guint idle_id;
	guint cancel_id;
	guint counter;
	
	GList *populateptr;
	guint idle_populate_id;
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
gsc_devhelp_item_finalize (GObject *object)
{
	g_free (((GscDevhelpItem *)object)->word);
}

static void
gsc_devhelp_item_class_init (GscDevhelpItemClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = gsc_devhelp_item_finalize;
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
	
	if (devhelp->priv->idle_populate_id != 0)
	{
		g_source_remove (devhelp->priv->idle_populate_id);
		devhelp->priv->idle_populate_id = 0;
	}
	
	if (devhelp->priv->completion_mark)
	{
		gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (devhelp->priv->completion_mark),
		                             devhelp->priv->completion_mark);
		devhelp->priv->completion_mark = NULL;
	}
	
	g_free (devhelp->priv->word);
	devhelp->priv->word = NULL;
	
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

static gboolean
is_word_char (gunichar ch)
{
	return g_unichar_isprint (ch) && (g_unichar_isalnum (ch) || ch == '_' || ch == ':' || ch == '.');
}

static gchar *
get_word_at_iter (GscProviderDevhelp *devhelp,
                  GtkTextIter        *iter)
{
	GtkTextIter start = *iter;
	gint line = gtk_text_iter_get_line (iter);
	gboolean went_back = TRUE;
	
	if (!gtk_text_iter_backward_char (&start))
	{
		return NULL;
	}

	while (went_back &&
	       line == gtk_text_iter_get_line (&start) && 
	       is_word_char (gtk_text_iter_get_char (&start)))
	{
		went_back = gtk_text_iter_backward_char (&start);
	}
	
	if (went_back)
	{
		gtk_text_iter_forward_char (&start);
	}
	
	if (gtk_text_iter_equal (iter, &start))
	{
		return NULL;
	}
	
	devhelp->priv->completion_mark = gtk_text_buffer_create_mark (gtk_text_iter_get_buffer (iter),
	                                                              NULL,
	                                                              &start,
	                                                              TRUE);
	return gtk_text_iter_get_text (&start, iter);
}

static gint
compare_two_items (GscDevhelpItem *a,
                   GscDevhelpItem *b,
                   gpointer        data)
{
	return strcmp (a->word, b->word);
}

static gint
compare_items (GscDevhelpItem     *a,
               GscDevhelpItem     *b,
               GscProviderDevhelp *devhelp)
{
	gchar const *m1 = a == NULL ? b->word : a->word;
	
	return strcmp (m1, devhelp->priv->word);
}

static gboolean
iter_match_prefix (GscProviderDevhelp *devhelp,
                   GSequenceIter      *iter)
{
	GscDevhelpItem *item = (GscDevhelpItem *)g_sequence_get (iter);
	
	return strncmp (item->word, devhelp->priv->word, devhelp->priv->word_len) == 0;
}

static GSequenceIter *
find_first_proposal (GscProviderDevhelp *devhelp)
{
	GSequenceIter *iter;
	GSequenceIter *prev;

	iter = g_sequence_search (devhelp->priv->proposals,
	                          NULL,
	                          (GCompareDataFunc)compare_items,
	                          devhelp);

	if (iter == NULL)
	{
		return NULL;
	}
	
	/* Test if this position might be after the last match */
	if (!g_sequence_iter_is_begin (iter) && 
	    (g_sequence_iter_is_end (iter) || !iter_match_prefix (devhelp, iter)))
	{
		iter = g_sequence_iter_prev (iter);
	}
	
	/* Maybe there is actually nothing in the sequence */
	if (g_sequence_iter_is_end (iter))
	{
		return NULL;
	}
	
	/* Check if it still matches here */
	if (!iter_match_prefix (devhelp, iter))
	{
		return NULL;
	}
	
	/* Go back while it matches */
	while (iter &&
	       (prev = g_sequence_iter_prev (iter)) && 
	       iter_match_prefix (devhelp, prev))
	{
		iter = prev;
		
		if (g_sequence_iter_is_end (iter))
		{
			iter = NULL;
		}
	}
	
	return iter;
}

static GSequenceIter *
find_next_proposal (GscProviderDevhelp *devhelp,
                    GSequenceIter      *iter)
{
	GscDevhelpItem *item;
	iter = g_sequence_iter_next (iter);

	if (!iter || g_sequence_iter_is_end (iter))
	{
		return NULL;
	}
	
	item = (GscDevhelpItem *)g_sequence_get (iter);
	
	return iter_match_prefix (devhelp, iter) ? iter : NULL;
}

static gboolean
add_in_idle (GscProviderDevhelp *devhelp)
{
	guint idx = 0;
	GList *ret = NULL;
	gboolean finished;
	
	/* Don't complete empty string (when word == NULL) */
	if (devhelp->priv->word == NULL)
	{
		gtk_source_completion_context_add_proposals (devhelp->priv->context,
	                                                 GTK_SOURCE_COMPLETION_PROVIDER (devhelp),
	                                                 NULL,
	                                                 TRUE);
		population_finished (devhelp);
		return FALSE;
	}
	
	if (devhelp->priv->populate_iter == NULL)
	{
		devhelp->priv->populate_iter = find_first_proposal (devhelp);
	}

	while (idx < PROCESS_BATCH && devhelp->priv->populate_iter)
	{
		GtkSourceCompletionProposal *proposal;
		
		proposal = GTK_SOURCE_COMPLETION_PROPOSAL (g_sequence_get (devhelp->priv->populate_iter));
		ret = g_list_prepend (ret, proposal);
		
		devhelp->priv->populate_iter = find_next_proposal (devhelp,
		                                                   devhelp->priv->populate_iter);
		++idx;
	}
	
	ret = g_list_reverse (ret);
	finished = devhelp->priv->populate_iter == NULL;
	
	gtk_source_completion_context_add_proposals (devhelp->priv->context,
	                                             GTK_SOURCE_COMPLETION_PROVIDER (devhelp),
	                                             ret,
	                                             finished);
	
	if (finished)
	{
		population_finished (devhelp);
	}

	return !finished;
}

static gboolean
valid_link_type (DhLinkType type)
{
	switch (type)
	{
		case DH_LINK_TYPE_BOOK:
		case DH_LINK_TYPE_PAGE:
			return FALSE;
		break;
		default:
			return TRUE;
		break;
	}
}

static gchar *
string_for_compare (gchar const *s)
{
	return g_utf8_normalize (s, -1, G_NORMALIZE_ALL);
}

static gboolean
idle_populate_proposals (GscProviderDevhelp *devhelp)
{
	guint idx = 0;

	if (devhelp->priv->dhbase == NULL)
	{
		devhelp->priv->proposals = g_sequence_new ((GDestroyNotify)g_object_unref);
		devhelp->priv->dhbase = dh_base_new ();
		devhelp->priv->view = dh_assistant_view_new ();

		dh_assistant_view_set_base (DH_ASSISTANT_VIEW (devhelp->priv->view), 
		                            devhelp->priv->dhbase);
		                            
		gtk_widget_set_size_request (devhelp->priv->view, 400, 300);
		
		devhelp->priv->populateptr = dh_base_get_keywords (devhelp->priv->dhbase);
	}
	
	while (idx < POPULATE_BATCH && devhelp->priv->populateptr)
	{
		DhLink *link = (DhLink *)devhelp->priv->populateptr->data;
		gchar const *name = dh_link_get_name (link);
		
		if (valid_link_type (dh_link_get_link_type (link)) &&
		    name != NULL && *name != '\0')
		{
			GscDevhelpItem *proposal = g_object_new (gsc_devhelp_item_get_type (), 
				                                     NULL);

			proposal->link = link;
			proposal->word = string_for_compare (dh_link_get_name (proposal->link));

			g_sequence_insert_sorted (devhelp->priv->proposals,
				                      proposal,
				                      (GCompareDataFunc)compare_two_items,
				                      NULL);
		}

		++idx;
		devhelp->priv->populateptr = g_list_next (devhelp->priv->populateptr);
	}
	
	return devhelp->priv->populateptr != NULL;
}

static void
gsc_provider_devhelp_populate (GtkSourceCompletionProvider *provider,
                               GtkSourceCompletionContext  *context)
{
	GscProviderDevhelp *devhelp = GSC_PROVIDER_DEVHELP (provider);
	GtkTextIter iter;
	gchar *word;

	devhelp->priv->cancel_id = 
		g_signal_connect_swapped (context, 
			                      "cancelled", 
			                      G_CALLBACK (population_finished), 
			                      provider);
	
	devhelp->priv->counter = 0;
	devhelp->priv->populate_iter = NULL;
	devhelp->priv->context = g_object_ref (context);

	gtk_source_completion_context_get_iter (context, &iter);
	
	g_free (devhelp->priv->word);

	word = get_word_at_iter (devhelp, &iter);
	
	if (word != NULL)
	{
		devhelp->priv->word = string_for_compare (word);
		devhelp->priv->word_len = strlen (devhelp->priv->word);
	}
	else
	{
		devhelp->priv->word = NULL;
		devhelp->priv->word_len = 0;
	}
	
	g_free (word);
	
	/* Make sure we are finished populating the proposals from devhelp */
	if (devhelp->priv->idle_populate_id != 0)
	{
		g_source_remove (devhelp->priv->idle_populate_id);
		devhelp->priv->idle_populate_id = 0;
		
		while (idle_populate_proposals (devhelp))
		;
	}
	
	/* Do first right now */
	if (add_in_idle (devhelp))
	{
		devhelp->priv->idle_id = g_idle_add ((GSourceFunc)add_in_idle,
		                                     devhelp);
	}
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

static gboolean
gsc_provider_devhelp_get_start_iter (GtkSourceCompletionProvider *provider,
                                     GtkSourceCompletionProposal *proposal,
                                     GtkTextIter                 *iter)
{
	GscProviderDevhelp *devhelp = GSC_PROVIDER_DEVHELP (provider);
	
	if (devhelp->priv->completion_mark == NULL ||
	    gtk_text_mark_get_deleted (devhelp->priv->completion_mark))
	{
		return FALSE;
	}
	
	gtk_text_buffer_get_iter_at_mark (gtk_text_mark_get_buffer (devhelp->priv->completion_mark),
	                                  iter,
	                                  devhelp->priv->completion_mark);
	return TRUE;
}

static void
gsc_provider_devhelp_iface_init (GtkSourceCompletionProviderIface *iface)
{
	iface->get_name = gsc_provider_devhelp_get_name;
	iface->populate = gsc_provider_devhelp_populate;
	
	iface->get_info_widget = gsc_provider_devhelp_get_info_widget;
	iface->update_info = gsc_provider_devhelp_update_info;
	
	iface->get_icon = gsc_provider_devhelp_get_icon;
	
	iface->get_start_iter = gsc_provider_devhelp_get_start_iter;
}

static void
gsc_provider_devhelp_finalize (GObject *object)
{
	GscProviderDevhelp *provider = GSC_PROVIDER_DEVHELP (object);
	
	g_object_unref (provider->priv->dhbase);
	g_sequence_free (provider->priv->proposals);
	
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
	
	self->priv->idle_populate_id = g_idle_add ((GSourceFunc)idle_populate_proposals,
	                                           self);
}

GscProviderDevhelp *
gsc_provider_devhelp_new ()
{
	GscProviderDevhelp *ret = g_object_new (GSC_TYPE_PROVIDER_DEVHELP, NULL);
	
	return ret;
}
