/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2007 - Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

#define TEST_TYPE_PROPOSAL (test_proposal_get_type())
G_DECLARE_FINAL_TYPE (TestProposal, test_proposal, TEST, PROPOSAL, GObject)

struct _TestProposal
{
	GObject parent_instance;
	char *label;
	char *text;
	char *markup;
	char *info;
	char *icon_name;
	GIcon *gicon;
	GtkIconPaintable *icon;
};

G_DEFINE_TYPE_WITH_CODE (TestProposal, test_proposal, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL, NULL))

static void
test_proposal_dispose (GObject *object)
{
	TestProposal *p = TEST_PROPOSAL (object);
	g_clear_pointer (&p->label, g_free);
	g_clear_pointer (&p->text, g_free);
	g_clear_pointer (&p->markup, g_free);
	g_clear_pointer (&p->info, g_free);
	g_clear_pointer (&p->icon_name, g_free);
	g_clear_object (&p->gicon);
	g_clear_object (&p->icon);
	G_OBJECT_CLASS (test_proposal_parent_class)->dispose (object);
}

static void
test_proposal_class_init (TestProposalClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = test_proposal_dispose;
}

static void
test_proposal_init (TestProposal *self)
{
}

static TestProposal *
test_proposal_new (void)
{
	return g_object_new (TEST_TYPE_PROPOSAL, NULL);
}

struct _TestProvider
{
	GObject parent;

	GList *proposals;
	gint priority;
	gchar *title;

	GtkIconPaintable *provider_icon;

	GtkIconPaintable *item_icon;
	GIcon *item_gicon;

	/* If it's a random provider, a subset of 'proposals' are chosen on
	 * each populate. Otherwise, all the proposals are shown. */
	guint is_random : 1;
};

struct _TestProviderClass
{
	GObjectClass parent_class;
};

static void test_provider_iface_init (GtkSourceCompletionProviderInterface *iface);

#define TEST_TYPE_PROVIDER (test_provider_get_type())
G_DECLARE_FINAL_TYPE (TestProvider, test_provider, TEST, PROVIDER, GObject)

static GtkSourceCompletionWords *word_provider;
static GtkSourceCompletionSnippets *snippet_provider;
static TestProvider *fixed_provider;
static TestProvider *random_provider;
static GMainLoop *main_loop;

G_DEFINE_TYPE_WITH_CODE (TestProvider,
			 test_provider,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
				 		test_provider_iface_init))

static gchar *
test_provider_get_title (GtkSourceCompletionProvider *provider)
{
	return g_strdup (((TestProvider *)provider)->title);
}

static gint
test_provider_get_priority (GtkSourceCompletionProvider *provider,
                            GtkSourceCompletionContext  *context)
{
	return ((TestProvider *)provider)->priority;
}

static GList *
select_random_proposals (GList *all_proposals)
{
	GList *selection = NULL;
	GList *prop;

	for (prop = all_proposals; prop != NULL; prop = g_list_next (prop))
	{
		if (g_random_boolean ())
		{
			selection = g_list_prepend (selection, prop->data);
		}
	}

	return selection;
}

static void
test_provider_populate_async (GtkSourceCompletionProvider *completion_provider,
                              GtkSourceCompletionContext  *context,
                              GCancellable                *cancellable,
                              GAsyncReadyCallback          callback,
                              gpointer                     user_data)
{
	TestProvider *provider = (TestProvider *)completion_provider;
	GListStore *results;
	GList *proposals;
	GTask *task;

	if (provider->is_random)
	{
		proposals = select_random_proposals (provider->proposals);
	}
	else
	{
		proposals = provider->proposals;
	}

	task = g_task_new (completion_provider, cancellable, callback, user_data);

	results = g_list_store_new (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL);

	for (const GList *iter = proposals; iter; iter = iter->next)
	{
		g_list_store_append (results, iter->data);
	}

	g_task_return_pointer (task, results, g_object_unref);

	if (provider->is_random)
	{
		g_list_free (proposals);
	}
}

static GListModel *
test_provider_populate_finish (GtkSourceCompletionProvider  *provider,
                               GAsyncResult                 *result,
                               GError                      **error)
{
	return g_task_propagate_pointer (G_TASK (result), error);
}

static void
test_provider_display (GtkSourceCompletionProvider *provider,
                       GtkSourceCompletionContext  *context,
                       GtkSourceCompletionProposal *proposal,
                       GtkSourceCompletionCell     *cell)
{
	TestProposal *p = (TestProposal *)proposal;
	GtkSourceCompletionColumn column;

	g_assert (TEST_IS_PROVIDER (provider));
	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
	g_assert (TEST_IS_PROPOSAL (p));
	g_assert (GTK_SOURCE_IS_COMPLETION_CELL (cell));

	column = gtk_source_completion_cell_get_column (cell);

	if (column == GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT)
	{
		g_assert (p->markup || p->text || p->label);

		if (p->markup)
			gtk_source_completion_cell_set_markup (cell, p->markup);
		else if (p->label)
			gtk_source_completion_cell_set_text (cell, p->label);
		else
			gtk_source_completion_cell_set_text (cell, p->text);
	}
	else if (column == GTK_SOURCE_COMPLETION_COLUMN_COMMENT ||
	         column == GTK_SOURCE_COMPLETION_COLUMN_DETAILS)
	{
		gchar *str = g_strdup (p->info);

		if (str != NULL)
		{
			g_strstrip (str);
		}

		if (str != NULL)
		{
			gtk_source_completion_cell_set_text (cell, str);
		}
		else
		{
			gtk_source_completion_cell_set_text (cell, NULL);
		}
	}
	else if (column == GTK_SOURCE_COMPLETION_COLUMN_ICON)
	{
		if (p->icon_name)
			gtk_source_completion_cell_set_icon_name (cell, p->icon_name);
		else if (p->gicon)
			gtk_source_completion_cell_set_gicon (cell, p->gicon);
		else
			gtk_source_completion_cell_set_icon_name (cell, NULL);
	}
}

static void
test_provider_activate (GtkSourceCompletionProvider *provider,
                        GtkSourceCompletionContext  *context,
                        GtkSourceCompletionProposal *proposal)
{
	GtkTextBuffer *buffer;
	GtkTextIter begin, end;

	gtk_source_completion_context_get_bounds (context, &begin, &end);
	buffer = gtk_text_iter_get_buffer (&begin);

	if (TEST_IS_PROPOSAL (proposal))
	{
		gtk_text_buffer_begin_user_action (buffer);
		gtk_text_buffer_delete (buffer, &begin, &end);
		gtk_text_buffer_insert (buffer, &begin, TEST_PROPOSAL (proposal)->text, -1);
		gtk_text_buffer_end_user_action (buffer);
	}
}

static void
test_provider_iface_init (GtkSourceCompletionProviderInterface *iface)
{
	iface->get_title = test_provider_get_title;
	iface->populate_async = test_provider_populate_async;
	iface->populate_finish = test_provider_populate_finish;
	iface->get_priority = test_provider_get_priority;
	iface->display = test_provider_display;
	iface->activate = test_provider_activate;
}

static void
test_provider_dispose (GObject *gobject)
{
	TestProvider *self = (TestProvider *)gobject;

	g_list_free_full (self->proposals, g_object_unref);
	self->proposals = NULL;

	g_clear_object (&self->provider_icon);
	g_clear_object (&self->item_icon);
	g_clear_object (&self->item_gicon);

	G_OBJECT_CLASS (test_provider_parent_class)->dispose (gobject);
}

static void
test_provider_finalize (GObject *gobject)
{
	TestProvider *self = (TestProvider *)gobject;

	g_free (self->title);
	self->title = NULL;

	G_OBJECT_CLASS (test_provider_parent_class)->finalize (gobject);
}

static void
test_provider_class_init (TestProviderClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = test_provider_dispose;
	gobject_class->finalize = test_provider_finalize;
}

static void
test_provider_init (TestProvider *self)
{
	GtkIconTheme *theme;
	GIcon *icon;
	GIcon *emblem_icon;
	GEmblem *emblem;

	theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());

	/* Just use some defaults for icons here. Normally we would create these with
	 * the widget to get proper direction, scale, etc.
	 */
	self->provider_icon = gtk_icon_theme_lookup_icon (theme, "dialog-information", NULL, 16, 1, GTK_TEXT_DIR_LTR, GTK_ICON_LOOKUP_PRELOAD);
	self->item_icon = gtk_icon_theme_lookup_icon (theme, "trophy-gold", NULL, 16, 1, GTK_TEXT_DIR_LTR, GTK_ICON_LOOKUP_PRELOAD);

	icon = g_themed_icon_new ("trophy-gold");
	emblem_icon = g_themed_icon_new ("emblem-urgent");
	emblem = g_emblem_new (emblem_icon);
	self->item_gicon = g_emblemed_icon_new (icon, emblem);
	g_object_unref (icon);
	g_object_unref (emblem_icon);
	g_object_unref (emblem);
}

static void
test_provider_set_fixed (TestProvider *provider,
			 gint          nb_proposals)
{
	TestProposal *item;
	GList *proposals = NULL;
	gint i;

	g_list_free_full (provider->proposals, g_object_unref);

	item = test_proposal_new ();
	item->markup = g_strdup ("A very <b>long</b> proposal. I <i>repeat</i>, a very long proposal!");
	item->text = g_strdup ("A very long proposal. I repeat, a very long proposal!");
	item->info = g_strdup ("To test the horizontal scrollbar and the markup.");
	proposals = g_list_prepend (proposals, item);

	item = test_proposal_new ();
	item->markup = g_strdup ("A proposal with a <b>symbolic</b> icon");
	item->text = g_strdup ("Test setting the icon-name property");
	item->icon_name = g_strdup ("face-cool-symbolic");
	proposals = g_list_prepend (proposals, item);

	item = test_proposal_new ();
	item->markup = g_strdup ("A proposal with an emblem <b>GIcon</b>");
	item->text = g_strdup ("Test setting the GIcon property");
	item->gicon = g_object_ref (provider->item_gicon);
	proposals = g_list_prepend (proposals, item);

	for (i = nb_proposals - 1; i > 0; i--)
	{
		gchar *name = g_strdup_printf ("Proposal %d", i);

		item = test_proposal_new ();
		item->label = name;
		item->text = g_strdup (name);
		item->icon = g_object_ref (provider->item_icon);
		item->info = g_strdup ("The extra info of the proposal.\nA second line.");
		proposals = g_list_prepend (proposals, item);
	}

	provider->proposals = proposals;
	provider->is_random = 0;
}

static void
test_provider_set_random (TestProvider *provider,
			  gint          nb_proposals)
{
	GList *proposals = NULL;
	gint i;

	g_list_free_full (provider->proposals, g_object_unref);

	for (i = 0; i < nb_proposals; i++)
	{
		TestProposal *item;
		gchar *padding = g_strnfill ((i * 3) % 10, 'o');
		gchar *name = g_strdup_printf ("Propo%ssal %d", padding, i);

		item = test_proposal_new ();
		item->label = name;
		item->text = g_strdup (name);
		item->icon = g_object_ref (provider->item_icon);
		proposals = g_list_prepend (proposals, item);

		g_free (padding);
	}

	provider->proposals = proposals;
	provider->is_random = 1;
}

static void
add_remove_provider (GtkCheckButton              *button,
		     GtkSourceCompletion         *completion,
		     GtkSourceCompletionProvider *provider)
{
	g_return_if_fail (provider != NULL);

	if (gtk_check_button_get_active (button))
	{
		gtk_source_completion_add_provider (completion, provider);
	}
	else
	{
		gtk_source_completion_remove_provider (completion, provider);
	}
}

static void
enable_word_provider_toggled_cb (GtkCheckButton      *button,
				 GtkSourceCompletion *completion)
{
	add_remove_provider (button,
			     completion,
			     GTK_SOURCE_COMPLETION_PROVIDER (word_provider));
}

static void
enable_fixed_provider_toggled_cb (GtkCheckButton      *button,
				  GtkSourceCompletion *completion)
{
	add_remove_provider (button,
			     completion,
			     GTK_SOURCE_COMPLETION_PROVIDER (fixed_provider));
}

static void
enable_random_provider_toggled_cb (GtkCheckButton      *button,
				   GtkSourceCompletion *completion)
{
	add_remove_provider (button,
			     completion,
			     GTK_SOURCE_COMPLETION_PROVIDER (random_provider));
}

static void
nb_proposals_changed_cb (GtkSpinButton *spin_button,
			 TestProvider  *provider)
{
	gint nb_proposals = gtk_spin_button_get_value_as_int (spin_button);

	if (provider->is_random)
	{
		test_provider_set_random (provider, nb_proposals);
	}
	else
	{
		test_provider_set_fixed (provider, nb_proposals);
	}
}

static void
enable_snippet_provider_toggled_cb (GtkCheckButton      *button,
                                    GtkSourceCompletion *completion)
{
	add_remove_provider (button,
	                     completion,
	                     GTK_SOURCE_COMPLETION_PROVIDER (snippet_provider));
}

static void
create_completion (GtkSourceView       *source_view,
		   GtkSourceCompletion *completion)
{
	/* Snippets completion provider */
	snippet_provider = gtk_source_completion_snippets_new ();
	g_object_set (snippet_provider, "priority", 20, NULL);
	gtk_source_completion_add_provider (completion,
	                                    GTK_SOURCE_COMPLETION_PROVIDER (snippet_provider));

	/* Words completion provider */
	word_provider = gtk_source_completion_words_new (NULL);

	gtk_source_completion_words_register (word_provider,
	                                      gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view)));

	gtk_source_completion_add_provider (completion,
	                                    GTK_SOURCE_COMPLETION_PROVIDER (word_provider));

	g_object_set (word_provider, "priority", 10, NULL);

	/* Fixed provider: the proposals don't change */
	fixed_provider = g_object_new (test_provider_get_type (), NULL);
	test_provider_set_fixed (fixed_provider, 3);
	fixed_provider->priority = 5;
	fixed_provider->title = g_strdup ("Fixed Provider");

	gtk_source_completion_add_provider (completion,
	                                    GTK_SOURCE_COMPLETION_PROVIDER (fixed_provider));

	/* Random provider: the proposals vary on each populate */
	random_provider = g_object_new (test_provider_get_type (), NULL);
	test_provider_set_random (random_provider, 10);
	random_provider->priority = 1;
	random_provider->title = g_strdup ("Random Provider");

	gtk_source_completion_add_provider (completion,
	                                    GTK_SOURCE_COMPLETION_PROVIDER (random_provider));
}

static void
create_window (void)
{
	GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default ();
	GtkSourceLanguage *l = gtk_source_language_manager_get_language (lm, "c");
	GtkBuilder *builder;
	GError *error = NULL;
	GtkWindow *window;
	GtkTextBuffer *buffer;
	GtkSourceView *source_view;
	GtkSourceCompletion *completion;
	GtkCheckButton *remember_info_visibility;
	GtkCheckButton *select_on_show;
	GtkCheckButton *show_icons;
	GtkCheckButton *enable_word_provider;
	GtkCheckButton *enable_fixed_provider;
	GtkCheckButton *enable_random_provider;
	GtkCheckButton *enable_snippet_provider;
	GtkSpinButton *nb_fixed_proposals;
	GtkSpinButton *nb_random_proposals;

	builder = gtk_builder_new ();

	gtk_builder_add_from_resource (builder,
				       "/org/gnome/gtksourceview/tests/ui/test-completion.ui",
				       &error);

	if (error != NULL)
	{
		g_error ("Impossible to load test-completion.ui: %s", error->message);
	}

	window = GTK_WINDOW (gtk_builder_get_object (builder, "window"));
	source_view = GTK_SOURCE_VIEW (gtk_builder_get_object (builder, "source_view"));
	remember_info_visibility = GTK_CHECK_BUTTON (gtk_builder_get_object (builder, "checkbutton_remember_info_visibility"));
	select_on_show = GTK_CHECK_BUTTON (gtk_builder_get_object (builder, "checkbutton_select_on_show"));
	show_icons = GTK_CHECK_BUTTON (gtk_builder_get_object (builder, "checkbutton_show_icons"));
	enable_word_provider = GTK_CHECK_BUTTON (gtk_builder_get_object (builder, "checkbutton_word_provider"));
	enable_snippet_provider = GTK_CHECK_BUTTON (gtk_builder_get_object (builder, "checkbutton_snippet_provider"));
	enable_fixed_provider = GTK_CHECK_BUTTON (gtk_builder_get_object (builder, "checkbutton_fixed_provider"));
	enable_random_provider = GTK_CHECK_BUTTON (gtk_builder_get_object (builder, "checkbutton_random_provider"));
	nb_fixed_proposals = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "spinbutton_nb_fixed_proposals"));
	nb_random_proposals = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "spinbutton_nb_random_proposals"));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
	gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), l);
	gtk_source_view_set_enable_snippets (source_view, TRUE);

	completion = gtk_source_view_get_completion (source_view);

	g_signal_connect_swapped (window,
	                          "destroy",
	                          G_CALLBACK (g_main_loop_quit),
	                          main_loop);

	g_object_bind_property (completion, "select-on-show",
	                        select_on_show, "active",
	                        G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

	g_object_bind_property (completion, "remember-info-visibility",
	                        remember_info_visibility, "active",
	                        G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

	g_object_bind_property (completion, "show-icons",
	                        show_icons, "active",
	                        G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

	create_completion (source_view, completion);

	g_signal_connect (enable_snippet_provider,
	                  "toggled",
	                  G_CALLBACK (enable_snippet_provider_toggled_cb),
	                  completion);

	g_signal_connect (enable_word_provider,
			  "toggled",
			  G_CALLBACK (enable_word_provider_toggled_cb),
			  completion);

	g_signal_connect (enable_fixed_provider,
			  "toggled",
			  G_CALLBACK (enable_fixed_provider_toggled_cb),
			  completion);

	g_signal_connect (enable_random_provider,
			  "toggled",
			  G_CALLBACK (enable_random_provider_toggled_cb),
			  completion);

	g_signal_connect (nb_fixed_proposals,
			  "value-changed",
			  G_CALLBACK (nb_proposals_changed_cb),
			  fixed_provider);

	g_signal_connect (nb_random_proposals,
			  "value-changed",
			  G_CALLBACK (nb_proposals_changed_cb),
			  random_provider);

	g_object_unref (builder);

	gtk_window_present (window);
}

int
main (int   argc,
      char *argv[])
{
	main_loop = g_main_loop_new (NULL, FALSE);

	gtk_init ();
	gtk_source_init ();

	create_window ();

	g_main_loop_run (main_loop);

	/* Not really useful, except for debugging memory leaks. */
	g_object_unref (word_provider);
	g_object_unref (snippet_provider);
	g_object_unref (fixed_provider);
	g_object_unref (random_provider);

	return 0;
}
