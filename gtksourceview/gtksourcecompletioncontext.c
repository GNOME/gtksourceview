/*
 * gtksourcecompletioncontext.c
 * This file is part of gtksourceview
 *
 * Copyright (C) 2009 - Jesse van den Kieboom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#include "gtksourcecompletioncontext.h"
#include "gtksourceview-typebuiltins.h"
#include "gtksourcecompletionprovider.h"
#include "gtksourceview-i18n.h"
#include "gtksourcecompletion.h"

#define GTK_SOURCE_COMPLETION_CONTEXT_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GTK_TYPE_SOURCE_COMPLETION_CONTEXT, GtkSourceCompletionContextPrivate))

struct _GtkSourceCompletionContextPrivate
{
	GtkSourceCompletion *completion;

	GtkTextIter iter;
	gboolean interactive_mode;
	gboolean default_mode;
};

/* Properties */
enum
{
	PROP_0,
	
	PROP_COMPLETION,
	PROP_VIEW,
	PROP_ITER,
	PROP_INTERACTIVE,
	PROP_DEFAULT
};

/* Signals */
enum
{
	CANCELLED,
	NUM_SIGNALS
};

guint context_signals[NUM_SIGNALS] = {0,};

G_DEFINE_TYPE (GtkSourceCompletionContext, gtk_source_completion_context, G_TYPE_INITIALLY_UNOWNED)

static void
gtk_source_completion_context_dispose (GObject *object)
{
	GtkSourceCompletionContext *context = GTK_SOURCE_COMPLETION_CONTEXT (object);
	
	if (context->priv->completion)
	{
		g_object_unref (context->priv->completion);
		context->priv->completion = NULL;
	}

	G_OBJECT_CLASS (gtk_source_completion_context_parent_class)->dispose (object);
}

static void
gtk_source_completion_context_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
	GtkSourceCompletionContext *self = GTK_SOURCE_COMPLETION_CONTEXT (object);
	
	switch (prop_id)
	{
		case PROP_COMPLETION:
			self->priv->completion = g_value_dup_object (value);
		break;
		case PROP_ITER:
			self->priv->iter = *((GtkTextIter *)g_value_get_pointer (value));
		break;
		case PROP_INTERACTIVE:
			self->priv->interactive_mode = g_value_get_boolean (value);
		break;
		case PROP_DEFAULT:
			self->priv->default_mode = g_value_get_boolean (value);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_completion_context_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
	GtkSourceCompletionContext *self = GTK_SOURCE_COMPLETION_CONTEXT (object);
	
	switch (prop_id)
	{
		case PROP_COMPLETION:
			g_value_set_object (value, self->priv->completion);
		break;
		case PROP_VIEW:
			g_value_set_object (value, gtk_source_completion_get_view (self->priv->completion));
		break;
		case PROP_ITER:
			g_value_set_pointer (value, &(self->priv->iter));
		break;
		case PROP_INTERACTIVE:
			g_value_set_boolean (value, self->priv->interactive_mode);
		break;
		case PROP_DEFAULT:
			g_value_set_boolean (value, self->priv->default_mode);
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_completion_context_class_init (GtkSourceCompletionContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->set_property = gtk_source_completion_context_set_property;
	object_class->get_property = gtk_source_completion_context_get_property;
	object_class->dispose = gtk_source_completion_context_dispose;

	/**
	 * GtkSourceCompletionContext::cancelled:
	 *
	 * Emitted when the current population of proposals has been cancelled.
	 * Providers adding proposals asynchronously should connect to this signal
	 * to know when to cancel running proposal queries.
	 **/
	context_signals[CANCELLED] =
		g_signal_new ("cancelled",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (GtkSourceCompletionContextClass, cancelled),
		              NULL, 
		              NULL,
		              g_cclosure_marshal_VOID__VOID, 
		              G_TYPE_NONE,
		              0);
	
	/**
	 * GtkSourceCompletionContext:completion:
	 *
	 * The #GtkSourceCompletion associated with the context.
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_COMPLETION,
	                                 g_param_spec_object ("completion",
	                                                      _("Completion"),
	                                                      _("The completion object to which the context belongs"),
	                                                      GTK_TYPE_SOURCE_COMPLETION,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GtkSourceCompletionContext:view:
	 *
	 * The #GtkSourceView associated with the context.
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_VIEW,
	                                 g_param_spec_object ("view",
	                                                      _("View"),
	                                                      _("The GtkSourceView"),
	                                                      GTK_TYPE_SOURCE_VIEW,
	                                                      G_PARAM_READABLE));

	/**
	 * GtkSourceCompletionContext:iter:
	 *
	 * The #GtkTextIter at which the completion is invoked.
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_ITER,
	                                 g_param_spec_pointer ("iter",
	                                                       _("Iter"),
	                                                       _("The GtkTextIter at which the completion was invoked"),
	                                                       G_PARAM_READWRITE));

	/**
	 * GtkSourceCompletionContext:interactive:
	 *
	 * Whether the completion is in 'interactive' mode.
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_INTERACTIVE,
	                                 g_param_spec_boolean ("interactive",
	                                                       _("Interactive"),
	                                                       _("Whether the completion was invoked in interactive mode"),
	                                                       FALSE,
	                                                       G_PARAM_READWRITE));
	
	/**
	 * GtkSourceCompletionContext:default:
	 *
	 * Whether the completion is in 'default' mode.
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_DEFAULT,
	                                 g_param_spec_boolean ("default",
	                                                       _("Default"),
	                                                       _("Whether completion was invoked in default mode"),
	                                                       FALSE,
	                                                       G_PARAM_READWRITE));
	
	g_type_class_add_private (object_class, sizeof(GtkSourceCompletionContextPrivate));
}

static void
gtk_source_completion_context_init (GtkSourceCompletionContext *self)
{
	self->priv = GTK_SOURCE_COMPLETION_CONTEXT_GET_PRIVATE (self);
}

/**
 * gtk_source_completion_context_add_proposals:
 * @context: A #GtkSourceCompletionContext
 * @provider: A #GtkSourceCompletionProvider
 * @proposals: The list of proposals to add
 * @finished: Whether the provider is finished adding proposals
 * 
 * Providers can use this function to add proposals to the completion. They
 * can do so asynchronously by means of the @finished argument. Providers must
 * ensure that they always call this function with @finished set to %TRUE
 * once each population (even if no proposals need to be added).
 *
 **/
void
gtk_source_completion_context_add_proposals (GtkSourceCompletionContext  *context,
                                             GtkSourceCompletionProvider *provider,
                                             GList                       *proposals,
                                             gboolean                     finished)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_CONTEXT (context));
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider));
	
	_gtk_source_completion_add_proposals (context->priv->completion,
	                                      context,
	                                      provider,
	                                      proposals,
	                                      finished);
}

/**
 * gtk_source_completion_context_get_view:
 * @context: A #GtkSourceCompletionContext
 * 
 * Get the #GtkSourceView to which the context applies
 *
 * Returns: A #GtkSourceView
 *
 **/
GtkSourceView *
gtk_source_completion_context_get_view (GtkSourceCompletionContext *context)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_CONTEXT (context), NULL);
	return gtk_source_completion_get_view (context->priv->completion);
}

/**
 * gtk_source_completion_context_get_iter:
 * @context: A #GtkSourceCompletionContext
 * @iter: A #GtkTextIter
 * 
 * Get the iter at which the completion was invoked. Providers can use this
 * to determine how and if to match proposals.
 *
 **/
void
gtk_source_completion_context_get_iter (GtkSourceCompletionContext *context,
                                        GtkTextIter                *iter)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_CONTEXT (context));
	*iter = context->priv->iter;
}

/**
 * gtk_source_completion_context_get_interactive:
 * @context: A #GtkSourceCompletionContext
 *
 * Get whether the context is targeting 'interactive' mode providers. The 
 * interactive mode is used by #GtkSourceCompletion to indicate a completion
 * that was started interactively (i.e. when typing).
 *
 * Returns: %TRUE if the context is in 'interactive' mode, %FALSE otherwise
 */
gboolean
gtk_source_completion_context_get_interactive (GtkSourceCompletionContext *context)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_CONTEXT (context), FALSE);
	return context->priv->interactive_mode;
	
}

/**
 * gtk_source_completion_context_get_default:
 * @context: A #GtkSourceCompletionContext
 *
 * Get whether the context is targeting 'default' mode providers. The default
 * mode is used by the #GtkSourceView completion binding to invoke the 
 * completion with providers that support default mode.
 *
 * Returns: %TRUE if the context is in 'default' mode, %FALSE otherwise
 */
gboolean
gtk_source_completion_context_get_default (GtkSourceCompletionContext *context)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_CONTEXT (context), FALSE);
	return context->priv->default_mode;
	
}

void
_gtk_source_completion_context_cancel (GtkSourceCompletionContext *context)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_CONTEXT (context));
	
	g_signal_emit (context, context_signals[CANCELLED], 0);
}
