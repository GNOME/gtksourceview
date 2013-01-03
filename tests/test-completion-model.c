/*
 * test-completion-model.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2013 - Sébastien Wilmet <swilmet@gnome.org>
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include "gtksourceview/gtksourcecompletionmodel.h"

/* Basic provider.
 * The populate function is not implemented. Proposals are created
 * independendly (it is more convenient).
 */

typedef struct _TestProvider      TestProvider;
typedef struct _TestProviderClass TestProviderClass;

struct _TestProvider
{
	GObject parent_instance;
	gint priority;
};

struct _TestProviderClass
{
	GObjectClass parent_class;
};

GType test_provider_get_type (void);

static void test_provider_iface_init (GtkSourceCompletionProviderIface *iface);

G_DEFINE_TYPE_WITH_CODE (TestProvider,
			 test_provider,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
						test_provider_iface_init));

static gchar *
test_provider_get_name (GtkSourceCompletionProvider *provider)
{
	return g_strdup ("Hobbits");
}

static gint
test_provider_get_priority (GtkSourceCompletionProvider *provider)
{
	return ((TestProvider *)provider)->priority;
}

static void
test_provider_iface_init (GtkSourceCompletionProviderIface *iface)
{
	iface->get_name = test_provider_get_name;
	iface->get_priority = test_provider_get_priority;
}

static void
test_provider_class_init (TestProviderClass *klass)
{
}

static void
test_provider_init (TestProvider *self)
{
	self->priority = 0;
}


static TestProvider *
test_provider_new (void)
{
	return g_object_new (test_provider_get_type (), NULL);
}

/* Utilities functions */

static GList *
create_proposals (void)
{
	GList *list = NULL;

	list = g_list_append (list, gtk_source_completion_item_new ("Frodo", "Frodo", NULL, NULL));
	list = g_list_append (list, gtk_source_completion_item_new ("Bilbo", "Bilbo", NULL, NULL));

	return list;
}

/* Tests */

static void
test_is_empty (void)
{
	GtkSourceCompletionModel *model;
	TestProvider *provider;
	GList *list_providers = NULL;
	GList *list_proposals = NULL;
#if 0
	GList *visible_providers = NULL;
#endif

	/* Completely empty */
	model = gtk_source_completion_model_new ();

	g_assert (gtk_source_completion_model_is_empty (model, FALSE));
	g_assert (gtk_source_completion_model_is_empty (model, TRUE));

	/* One visible provider */
	provider = test_provider_new ();
	list_providers = g_list_append (list_providers, provider);
	list_proposals = create_proposals ();

	gtk_source_completion_model_begin_populate (model, list_providers);

	gtk_source_completion_model_add_proposals (model,
						   GTK_SOURCE_COMPLETION_PROVIDER (provider),
						   list_proposals);

	gtk_source_completion_model_end_populate (model,
						  GTK_SOURCE_COMPLETION_PROVIDER (provider));

	g_assert (!gtk_source_completion_model_is_empty (model, FALSE));
	g_assert (!gtk_source_completion_model_is_empty (model, TRUE));

	/* One invisible provider */

	/* FIXME The two following tests are broken with the current implementation, it
	 * will be fixed with the new implementation.
	 * However it seems that this situation never happen, because if there is only one
	 * invisible provider, the completion window is hidden.
	 */
#if 0
	visible_providers = g_list_append (visible_providers, test_provider_new ());
	gtk_source_completion_model_set_visible_providers (model, visible_providers);

	g_assert (!gtk_source_completion_model_is_empty (model, FALSE));
	g_assert (gtk_source_completion_model_is_empty (model, TRUE));
#endif

	g_object_unref (model);
	g_list_free_full (list_providers, g_object_unref);
	g_list_free_full (list_proposals, g_object_unref);
#if 0
	g_list_free_full (visible_providers, g_object_unref);
#endif
}

int
main (int argc, char **argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/CompletionModel/is-empty", test_is_empty);

	return g_test_run ();
}
