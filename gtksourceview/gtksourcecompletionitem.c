#include <gtksourceview/gtksourcecompletionitem.h>

#include "gtksourcecompletionutils.h"
#include "gtksourceview-i18n.h"

#define GTK_SOURCE_COMPLETION_ITEM_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GTK_TYPE_SOURCE_COMPLETION_ITEM, GtkSourceCompletionItemPrivate))

struct _GtkSourceCompletionItemPrivate
{
	gchar *label;
	gchar *info;
	GdkPixbuf *icon;	
};

/* Properties */
enum
{
	PROP_0,
	PROP_LABEL,
	PROP_ICON,
	PROP_INFO
};

static void gtk_source_completion_proposal_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionItem, 
			 gtk_source_completion_item, 
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SOURCE_COMPLETION_PROPOSAL,
			 			gtk_source_completion_proposal_iface_init))

static gboolean
gtk_source_completion_proposal_activate_impl (GtkSourceCompletionProposal *self,
					      GtkSourceBuffer		  *buffer)
{
	GtkSourceCompletionItem *item = GTK_SOURCE_COMPLETION_ITEM (self);

	gtk_source_completion_utils_replace_current_word (buffer,
							  item->priv->label,
							  -1);
	return FALSE;
}

static const gchar *
gtk_source_completion_proposal_get_label_impl (GtkSourceCompletionProposal *self)
{
	return GTK_SOURCE_COMPLETION_ITEM (self)->priv->label;
}

static const GdkPixbuf *
gtk_source_completion_proposal_get_icon_impl (GtkSourceCompletionProposal *self)
{
	return GTK_SOURCE_COMPLETION_ITEM (self)->priv->icon;
}

static const gchar *
gtk_source_completion_proposal_get_info_impl (GtkSourceCompletionProposal *self)
{
	return GTK_SOURCE_COMPLETION_ITEM (self)->priv->info;
}

static void
gtk_source_completion_proposal_iface_init (gpointer g_iface, 
					   gpointer iface_data)
{
	GtkSourceCompletionProposalIface *iface = (GtkSourceCompletionProposalIface *)g_iface;
	
	/* Default activate handler */
	iface->activate = gtk_source_completion_proposal_activate_impl;
	
	/* Interface data getter implementations */
	iface->get_label = gtk_source_completion_proposal_get_label_impl;
	iface->get_icon = gtk_source_completion_proposal_get_icon_impl;
	iface->get_info = gtk_source_completion_proposal_get_info_impl;
}

static void
gtk_source_completion_item_finalize (GObject *object)
{
	GtkSourceCompletionItem *self = GTK_SOURCE_COMPLETION_ITEM(object);
	
	g_free (self->priv->label);
	g_free (self->priv->info);
	
	if (self->priv->icon != NULL)
	{
		g_object_unref (self->priv->icon);
	}

	G_OBJECT_CLASS (gtk_source_completion_item_parent_class)->finalize (object);
}

static void
gtk_source_completion_item_get_property (GObject    *object,
					 guint       prop_id,
					 GValue     *value,
					 GParamSpec *pspec)
{
	GtkSourceCompletionItem *self;

	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_ITEM (object));

	self = GTK_SOURCE_COMPLETION_ITEM (object);

	switch (prop_id)
	{
		case PROP_LABEL:
			g_value_set_string (value, self->priv->label);
			break;
		case PROP_INFO:
			g_value_set_string (value, self->priv->info);
			break;
		case PROP_ICON:
			g_value_set_object (value, self->priv->icon);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_completion_item_set_property (GObject      *object,
					 guint         prop_id,
					 const GValue *value,
					 GParamSpec   *pspec)
{
	GtkSourceCompletionItem *self;

	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_ITEM (object));

	self = GTK_SOURCE_COMPLETION_ITEM (object);

	switch (prop_id)
	{
		case PROP_LABEL:
			g_free (self->priv->label);
			self->priv->label = g_value_dup_string (value);
			break;
		case PROP_INFO:
			g_free (self->priv->info);
			self->priv->info = g_value_dup_string (value);
			break;
		case PROP_ICON:
			if (self->priv->icon != NULL)
			{
				g_object_unref (self->priv->icon);
			}
			
			self->priv->icon = GDK_PIXBUF (g_value_dup_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_completion_item_class_init (GtkSourceCompletionItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_completion_item_finalize;
	object_class->get_property = gtk_source_completion_item_get_property;
	object_class->set_property = gtk_source_completion_item_set_property;

	/**
	 * GtkSourceCompletionItem:label:
	 *
	 * Label to be shown for this item
	 */
	g_object_class_install_property (object_class,
					 PROP_LABEL,
					 g_param_spec_string ("label",
							      _("Label"),
							      _("Label to be shown for this item"),
							      NULL,
							      G_PARAM_READWRITE));

	/**
	 * GtkSourceCompletionItem:icon:
	 *
	 * Icon to be shown for this item
	 */
	g_object_class_install_property (object_class,
					 PROP_ICON,
					 g_param_spec_object ("icon",
							      _("Icon"),
							      _("Icon to be shown for this item"),
							      GDK_TYPE_PIXBUF,
							      G_PARAM_READWRITE));

	/**
	 * GtkSourceCompletionItem:info:
	 *
	 * Info to be shown for this item
	 */
	g_object_class_install_property (object_class,
					 PROP_INFO,
					 g_param_spec_string ("info",
							      _("Info"),
							      _("Info to be shown for this item"),
							      NULL,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof(GtkSourceCompletionItemPrivate));
}

static void
gtk_source_completion_item_init (GtkSourceCompletionItem *self)
{
	self->priv = GTK_SOURCE_COMPLETION_ITEM_GET_PRIVATE (self);
}

/** 
 * gtk_source_completion_item_new:
 *
 * @label: the item label
 * @icon: the item icon
 * @info: the item extra information
 *
 * Create a new #GtkSourceCompletionItem with label @label, icon @icon and 
 * extra information @info
 *
 * Return value: the newly constructed #GtkSourceCompletionItem
 *
 */
GtkSourceCompletionItem *
gtk_source_completion_item_new (const gchar *label,
				GdkPixbuf   *icon,
				const gchar *info)
{
	return g_object_new (GTK_TYPE_SOURCE_COMPLETION_ITEM, 
			     "label", label,
			     "icon", icon,
			     "info", info,
			     NULL);
}

/** 
 * gtk_source_completion_item_new_from_stock:
 *
 * @label: the item label
 * @icon: the item icon
 * @info: the item extra information
 *
 * Create a new #GtkSourceCompletionItem from a stock item. If @label is NULL, 
 * the stock label will be used.
 *
 * Return value: the newly constructed #GtkSourceCompletionItem
 *
 */
GtkSourceCompletionItem *
gtk_source_completion_item_new_from_stock (const gchar *label,
					   const gchar *stock,
					   const gchar *info)
{
	GtkSourceCompletionItem *item;
	GdkPixbuf *icon;
	GtkIconTheme *theme;
	gint width;
	gint height;
	GtkStockItem stock_item;
	
	if (stock != NULL)
	{
		theme = gtk_icon_theme_get_default ();
	
		gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, &height);

		icon = gtk_icon_theme_load_icon (theme, 
						 stock, 
						 width, 
						 GTK_ICON_LOOKUP_USE_BUILTIN, 
						 NULL);

		if (label == NULL && gtk_stock_lookup (stock, &stock_item))
		{
			label = stock_item.label;
		}
	}
	else
	{
		icon = NULL;
	}
	
	item = gtk_source_completion_item_new (label, icon, info);
	
	if (icon != NULL)
	{
		g_object_unref (icon);
	}
	
	return item;	
}
