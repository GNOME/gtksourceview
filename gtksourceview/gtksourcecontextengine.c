/* -*- mode: c; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcecontextengine.c
 *
 *  Copyright (C) 2003 - Gustavo Giráldez <gustavo.giraldez@gmx.net>
 *  Copyright (C) 2005, 2006 - Marco Barisione, Emanuele Aina
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * This is a general introduction to the contexts based engine, before
 * reading this you should read the documentation of the .lang format.
 *
 * ENGINE INITIALIZATION
 * The engine is created using gtk_source_context_engine_new(), the
 * definitions of the contexts are added using:
 * - gtk_source_context_engine_define_context() -- for container
 *   contexts, these are represented by ContextDefinition structures
 *   stored in the hash table priv->definitions. If the parent_id
 *   argument is not NULL a reference to the newly created
 *   definition is added to the children list of the parent definition.
 * - gtk_source_context_engine_add_sub_pattern() -- for sub pattern
 *   contexts, represented by SubPatternDefinition and stored
 *   in the list sub_patterns of the containing ContextDefinition.
 * - gtk_source_context_engine_add_ref() -- for reference to contexts,
 *   the context has already been created, so a reference to the
 *   existing context is added to the GSList children of the parent
 *   ContextDefinition.
 *
 *
 * SYNTAX ANALYSIS
 * The analysis process starts creating a new Context referring to the
 * main ContextDefinition of the language, that is the context whose id
 * is the same id of the language (for instance "c:c" for C) that is
 * stored in priv->root_context.
 *
 * The text is analyzed in the idle loop or when explicitly required
 * (see update_highlight_cb()), during the analysis process the engine
 * looks for transitions to other contexts at each character, i.e. when
 * the text matches one of the following:
 * - The StartEnd.start regex of a child container context definition.
 * - The StartEnd.end regex of the definition of the current
 *   context.
 * - The match regex of a child simple context definition.
 * - The StartEnd.end regex of a parent definition that can terminate
 *   the current context (see the description of the extend-parent
 *   attribute in .lang files.)
 * This way Contexts are stored in a tree whose root is stored in
 * priv->root_context, for instance for the following code:
 *
 *   // This is a comment.
 *   int a = 42;
 *   printf ("%d\n", a);
 *
 * We would obtain:
 *
 *                    c
 *                    |
 *        -------------------------
 *        |           |           |
 *     comment     keyword      string
 *                                |
 *                              escape
 *
 * Where each context is marked by a start and an end offset, if we have
 * not yet found the end of a context it is set to END_NOT_YET_FOUND.
 *
 * Note that looking for a transition at each character means doing an
 * anchored search (see EGG_REGEX_ANCHORED) at each character for each
 * possible transition, this is extremely slow, so this process is
 * accelerated with these steps:
 * 1 - Each ContextDefinition contains a reg_all regex that contains
 *     the union (with the regular expression "|" operator) of the
 *     possible transitions to other definitions. This will be NULL
 *     if there are regular expressions that depend on the matched
 *     text because they contain a "\{...@start}".
 * 2 - When a Context is created then Context.reg_all is set to the
 *     value of ContextDefinition.reg_all. If this is NULL the value is
 *     calculated as we now know the text matched. Note that this means
 *     the using a lot of "\{...@start}" can slow down the engine.
 * 3 - The position of the transition is found using a not anchored
 *     search using Context.reg_all
 * 4 - The exact transition is found using the norml method.
 *
 * The main functions involved in this process are: update_syntax(),
 * analyze_line(), next_context(), context_starts_here(),
 * simple_context_starts_here(), container_context_starts_here(),
 * apply_match() and ancestor_ends_here().
 *
 * The text to analyze is split in batches, the batches are analyzed
 * line by line and read through line_reader_*() functions.
 *
 *
 * MODIFICATIONS
 * When the buffer is modified the engine tries to minimize the work to
 * update the tree, to do so it splits the tree in two different trees,
 * the first contains contexts that are surely valid because they are
 * before the modification offset, the second contain the other contexts.
 * The analysis process continues as usual but after each step the engine
 * verifies if the current state is equal to the state at the same
 * position before the modification using the function states_are_equal().
 * If the function returns TRUE then the two trees are joined by
 * join_contexts_tree() and the analysis stops.
 *
 * Modifications can be synchronous or asynchronous, synchronous ones
 * are used if the text inserted is only one character (i.e. normal
 * input) and there are no others pending modifications, else they are
 * inserted in the queue priv->modifications. Asynchronous modifications
 * are analyzed in the idle loop.
 * This tecnique improves the perceived responsiveness after a
 * modification.
 *
 *
 * HIGHLIGHTING
 * The highlighting process is separated from the analysis, when a
 * region of the buffer is visualized then the tags of the visible part
 * of the tree are applied. See functions highlight_region(),
 * ensure_highlighted() and update_highlight_cb().
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "gtksourceview-i18n.h"
#include "gtksourcetag.h"
#include "gtksourcebuffer.h"
#include "gtksourcecontextengine.h"
#include "gtktextregion.h"

#undef ENABLE_DEBUG
#undef ENABLE_PROFILE
#undef ENABLE_PRINT_TREE
/* If this is defined then the engine verifies the tree coherency. */
#undef ENABLE_VERIFY_TREE

#ifdef ENABLE_DEBUG
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

#ifdef ENABLE_PROFILE
#define PROFILE(x) (x)
#else
#define PROFILE(x)
#endif

/* In milliseconds. */
#define WORKER_TIME_SLICE	30
/* The batch size is modified at runtime. */
#define INITIAL_WORKER_BATCH	8192
#define MINIMUM_WORKER_BATCH	1024

/* Regex used to match "\%{...@start}". */
#define START_REF_REGEX "(?<!\\\\)(\\\\\\\\)*\\\\%\\{(.*?)@start\\}"

#define SIGN(n) ((n) >= 0 ? 1 : -1)

struct _RegexInfo
{
	gchar			*pattern;
	EggRegexCompileFlags	 flags;
};
typedef struct _RegexInfo RegexInfo;

/* We do not use directly EggRegex to allow the use of "\%{...@start}". */
struct _Regex
{
	union
	{
		EggRegex	*regex;
		RegexInfo	 info;
	};
	gboolean		 resolved;
	gint			 ref_count;
};
typedef struct _Regex Regex;

enum _SubPatternWhere
{
	SUB_PATTERN_WHERE_DEFAULT = 0,
	SUB_PATTERN_WHERE_START,
	SUB_PATTERN_WHERE_END
};
typedef enum _SubPatternWhere SubPatternWhere;

struct _SubPatternDefinition
{
#if defined (ENABLE_DEBUG) || \
    defined (ENABLE_PRINT_TREE) || \
    defined (ENABLE_VERIFY_TREE)
	/* We need the id only for debugging. */
	gchar			*id;
#endif
	SubPatternWhere		 where;
	gchar			*style;
	GtkSourceTag		*tag;
	gboolean 		 is_named;
	union
	{
		gint	 	 num;
		gchar		*name;
	};
};
typedef struct _SubPatternDefinition SubPatternDefinition;

struct _SubPattern
{
	SubPatternDefinition	*definition;
	gint			 start_at;
	gint			 end_at;
};
typedef struct _SubPattern SubPattern;

struct _StartEnd
{
	Regex	*start;
	Regex	*end;
};
typedef struct _StartEnd StartEnd;

typedef struct _Context Context;

struct _ContextDefinition
{
	gchar			*id;

	ContextType		 type;
	union
	{
		Regex		*match;
		StartEnd	 start_end;
	};

	/* Does this context can extend its parent? */
	gboolean		 extend_parent;

	/* Name of the style used for contexts of this type. */
	gchar			*style;
	/* Tag used for contexts of this type. */
	GtkSourceTag		*tag;

	/* Does this context should end before the end of the line? */
	gboolean		 end_at_line_end;

	/* This is a list of DefinitionChild pointers. */
	GSList			*children;

	/* Sub patterns (list of SubPatternDefinition pointers.) */
	GSList			*sub_patterns;

	/* Union of every regular expression we can find from this
	 * context. */
	Regex			*reg_all;
};
typedef struct _ContextDefinition ContextDefinition;

/* Returns the definition corrsponding to the specified id. */
#define LOOKUP_DEFINITION(ce, id) \
	(g_hash_table_lookup ((ce)->priv->definitions, (id)))

struct _DefinitionChild
{
	gboolean		 is_ref_all;
	ContextDefinition	*definition;
};
typedef struct _DefinitionChild DefinitionChild;

struct _Context
{
	/* Definition for the context. */
	ContextDefinition	*definition;
	/* Parent context, i.e. the context containing this context.
	 * This can be NULL only if the context is the root. */
	Context			*parent;
	/* List of children. */
	Context			*children;
	/* The last child, used to optimize context_last(), if this is
	 * NULL then the last child needs to be calculated. */
	Context			*last_child;
	/* List of sub-patterns (each element is a SubPattern.) */
	GSList			*sub_patterns;
	/* Previous and next contexts in the list of children in
	 * parent->children. */
	Context			*prev;
	Context			*next;
	/* Do all the ancestors extend their parent? */
	gboolean		 all_ancestors_extend;
	/* The context in used in the interval [start_at; end_at).
	 * end_at is END_NOT_YET_FOUND if we have not yet found the end
	 * of the context. */
	gint			 start_at;
	gint			 end_at;
	/* This is the regex returned by regex_resolve() called on
	 * definition->start_end.end. */ 
	Regex			*end;
	/* The regular expression containing every regular expression that
	 * could be matched in this context. */
	Regex			*reg_all;
	/* Priorities are assigned to tags in an increasing order, so, if
	 * there is a reference to another definition, the inner context
	 * style is overridden by the outer style:
	 *   <context id="A" style="STYLE1">   <= priority 0
	 *     ...
	 *   </context>
	 *   <context id="B" style="STYLE2">   <= priority 1
	 *     ...
	 *     <include>
	 *       <context ref="A"/>            <= priority 0, overridden
	 *     </include>
	 *   </context>
	 * This cannot be avoided changing the way priorities are assigned
	 * as there can be circular references (i.e. A contains B and B
	 * contains A); so the tag of an outer context is cleared if
	 * needed. */
	GtkSourceTag		*clear_tag;
};

/* Asynchronous insertions or deletions. */
struct _Modify
{
	gint offset;
	gint delta;
};
typedef struct _Modify Modify;

#define CONTEXT_ENGINE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
				      GTK_TYPE_SOURCE_CONTEXT_ENGINE, \
				      GtkSourceContextEnginePrivate))

/* Context.end_at is END_NOT_YET_FOUND until the closing regex is matched. */
#define END_NOT_YET_FOUND G_MAXINT

/* Is context the root context? */
#define CONTEXT_IS_ROOT(context) ((context)->parent == NULL)

/* Does an ancestor can end the context? */
#define ANCESTOR_CAN_END_CONTEXT(context) \
	(!(context)->definition->extend_parent || \
	 !(context)->all_ancestors_extend)

struct _GtkSourceContextEnginePrivate
{
	/* Name of the language file. */
	gchar			*id;

	GtkSourceBuffer		*buffer;

	/* Contains every ContextDefinition indexed by its id. */
	GHashTable		*definitions;

	/* Whether or not to actually highlight the buffer. */
	gboolean		 highlight;

	/* Region covering the unhighlighted text. */
	GtkTextRegion		*refresh_region;

	/* Tree of contexts. */
	Context			*root_context;

	/* Modifications that need to be done asynchronously. The
	 * elements in the queue are pointers to Modify. */
	GQueue			*modifications;

	gint			 worker_last_offset;
	gint			 worker_batch_size;
	guint			 worker_handler;

	/* Views highlight requests. */
	GtkTextRegion		*highlight_requests;
};

struct _DefinitionsIter
{
	GSList				*children_stack;
};
typedef struct _DefinitionsIter DefinitionsIter;

static GtkSourceEngineClass *parent_class = NULL;

static void	 gtk_source_context_engine_class_init
						(GtkSourceContextEngineClass	*klass);
static void	 gtk_source_context_engine_init	(GtkSourceContextEngine		*engine);
static void	 gtk_source_context_engine_finalize
						(GObject			*object);

static void	 gtk_source_context_engine_attach_buffer
						(GtkSourceEngine		*engine,
						 GtkSourceBuffer		*buffer);

static void	 definition_free		(ContextDefinition		*definition);

static Context	*context_new			(ContextDefinition		*definition,
						 Context			*parent,
						 gint				 start_at,
						 const gchar			*end_text);
static void	 context_destroy		(Context			*context);

static void	 install_idle_worker		(GtkSourceContextEngine		*ce);

static void	 text_modified			(GtkSourceContextEngine		*ce,
						 gint				 offset,
						 gint				 delta);
static gboolean	 async_modify			(GtkSourceContextEngine *ce);
static void	 update_syntax			(GtkSourceContextEngine		*ce,
						 const GtkTextIter		*needed_end,
						 gint				 modification_offset,
						 gint				 delta);

static void	 highlight_region		(GtkSourceContextEngine		*ce,
						 GtkTextIter			*start,
						 GtkTextIter			*end);
static void	 refresh_range			(GtkSourceContextEngine		*ce,
						 GtkTextIter			*start,
						 GtkTextIter			*end);
static void	 enable_highlight		(GtkSourceContextEngine		*ce,
						 gboolean			 enable);
static void	 ensure_highlighted		(GtkSourceContextEngine		*ce,
						 const GtkTextIter		*start,
						 const GtkTextIter		*end);
static void	 highlight_queue		(GtkSourceContextEngine		*ce,
						 const GtkTextIter		*start,
						 const GtkTextIter		*end);

#ifdef ENABLE_PRINT_TREE
static void	 print_tree			(const gchar			*label,
						 Context			*tree);
#endif

#ifdef ENABLE_VERIFY_TREE
static void	verify_tree			(GtkSourceContextEngine		*ce);
#endif


GType
gtk_source_context_engine_get_type (void)
{
	static GType our_type = 0;

	if (!our_type) {
		static const GTypeInfo our_info = {
			sizeof (GtkSourceContextEngineClass),
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) gtk_source_context_engine_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (GtkSourceContextEngine),
			0,	/* n_preallocs */
			(GInstanceInitFunc) gtk_source_context_engine_init
		};

		our_type = g_type_register_static (GTK_TYPE_SOURCE_ENGINE,
			"GtkSourceContextEngine", &our_info, 0);
	}

	return our_type;
}


/* CLASS AND OBJECT LIFECYLCE --------------------------------------------- */

static void
gtk_source_context_engine_class_init (GtkSourceContextEngineClass *klass)
{
	GObjectClass *object_class;
	GtkSourceEngineClass *engine_class;

	object_class 	= G_OBJECT_CLASS (klass);
	parent_class 	= g_type_class_peek_parent (klass);
	engine_class	= GTK_SOURCE_ENGINE_CLASS (klass);

	object_class->finalize	       = gtk_source_context_engine_finalize;
	engine_class->attach_buffer    = gtk_source_context_engine_attach_buffer;

	g_type_class_add_private (object_class, sizeof (GtkSourceContextEnginePrivate));
}

static void
gtk_source_context_engine_init (GtkSourceContextEngine *ce)
{
	ce->priv = CONTEXT_ENGINE_GET_PRIVATE (ce);

	ce->priv->definitions = g_hash_table_new_full (g_str_hash, g_str_equal,
		g_free, (GDestroyNotify)definition_free);
	ce->priv->root_context = NULL;
	ce->priv->buffer = NULL;
	ce->priv->modifications = g_queue_new ();
}

static void
gtk_source_context_engine_finalize (GObject *object)
{
	GtkSourceContextEngine *ce;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_IS_SOURCE_CONTEXT_ENGINE (object));

	ce = GTK_SOURCE_CONTEXT_ENGINE (object);

	/* Disconnect the buffer (if there is one), which destroys almost
	 * eveything. */
	gtk_source_context_engine_attach_buffer (GTK_SOURCE_ENGINE (ce), NULL);

	/* If the engine has not been attached to a buffer root_context
	 * is NULL. */
	if (ce->priv->root_context != NULL)
		context_destroy (ce->priv->root_context);

	g_hash_table_destroy (ce->priv->definitions);
	g_queue_free (ce->priv->modifications);

	g_free (ce->priv->id);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GQuark
gtk_source_context_engine_error_quark (void)
{
	static GQuark err_q = 0;
	if (err_q == 0)
		err_q = g_quark_from_static_string (
			"gtk-source-context-engine-error-quark");

	return err_q;
}


/* BUFFER ATTACHMENT AND CHANGE TRACKING FUNCTIONS ------------------------ */

static void
text_inserted_cb (GtkSourceBuffer   *buffer,
		  const GtkTextIter *start,
		  const GtkTextIter *end,
		  gpointer           data)
{
	gint text_length;
	gint start_offset, end_offset;
	GtkSourceContextEngine *ce = data;

	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);
	g_return_if_fail (GTK_IS_SOURCE_CONTEXT_ENGINE (ce));

	g_return_if_fail (ce->priv->buffer == buffer);

	start_offset = gtk_text_iter_get_offset (start);
	end_offset = gtk_text_iter_get_offset (end);
	text_length = end_offset - start_offset;

	text_modified (ce, start_offset, text_length);
}

static void
text_deleted_cb (GtkSourceBuffer   *buffer,
		 const GtkTextIter *iter,
		 const gchar       *text,
		 gpointer           data)
{
	GtkSourceContextEngine *ce = data;

	g_return_if_fail (iter != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (iter) ==
		GTK_TEXT_BUFFER (buffer));
	g_return_if_fail (GTK_IS_SOURCE_CONTEXT_ENGINE (ce));

	g_return_if_fail (ce->priv->buffer == buffer);

	text_modified (ce, gtk_text_iter_get_offset (iter),
		-g_utf8_strlen (text, -1));
}

static void
update_highlight_cb (GtkSourceBuffer   *buffer,
		     const GtkTextIter *start,
		     const GtkTextIter *end,
		     gboolean           synchronous,
		     gpointer           data)
{
	GtkSourceContextEngine *ce = data;

	g_return_if_fail (GTK_IS_SOURCE_CONTEXT_ENGINE (ce));
	g_return_if_fail (start != NULL);
       	g_return_if_fail (end != NULL);

	if (!ce->priv->highlight)
		return;

	if (ce->priv->worker_last_offset < 0 ||
	    ce->priv->worker_last_offset >= gtk_text_iter_get_offset (end))
	{
		ensure_highlighted (ce, start, end);
	}
	else
	{
		if (synchronous)
		{
			/* Do pending asynchronous modifications. */
			while (async_modify (ce))
			{
			}
			update_syntax (ce, end, -1, 0);
			ensure_highlighted (ce, start, end);
		}
		else
		{
			highlight_queue (ce, start, end);
			install_idle_worker (ce);
		}
	}
}

static void
forget_tag (GtkSourceContextEngine  *ce,
	    GtkSourceTag	   **tag)
{
	GtkTextIter start, end;

	g_return_if_fail (ce->priv->buffer != NULL);

	if (*tag == NULL)
		return;

	/* Remove tag from buffer. */
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (ce->priv->buffer),
		&start, &end);
	gtk_text_buffer_remove_tag (GTK_TEXT_BUFFER (ce->priv->buffer),
		GTK_TEXT_TAG (*tag),
		&start, &end);

	g_object_unref (*tag);
	*tag = NULL;
}

static gboolean
update_tag (GtkSourceContextEngine  *ce,
	    GtkTextTagTable	    *table,
	    const gchar		    *style,
	    GtkSourceTag	   **tag)
{
	GtkTextTag *ttag;
	GtkSourceTag *stag;

	/* Lookup style */
	if (style != NULL)
		ttag = gtk_text_tag_table_lookup (table, style);
	else 
		ttag = NULL;
	ttag = GTK_IS_SOURCE_TAG (ttag) ? ttag : NULL;

	/* Cast the tag. */
	stag = ttag ? GTK_SOURCE_TAG (ttag) : NULL;

	/* Check for changes. */
	if (stag != *tag)
	{
		if (*tag != NULL)
			forget_tag (ce, tag);
		if (stag != NULL)
			g_object_ref (stag);
		*tag = stag;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static gboolean
retrieve_definition_tag (GtkSourceContextEngine *ce,
			 ContextDefinition      *definition)
{
	GtkTextTagTable *table;
	gboolean rval;
	GSList *sub_pattern_list;

	g_return_val_if_fail (ce->priv->buffer, FALSE);

	table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (ce->priv->buffer));
	g_return_val_if_fail (table, FALSE);

	rval = update_tag (ce, table, definition->style, &definition->tag);

	sub_pattern_list = definition->sub_patterns;
	while (sub_pattern_list != NULL)
	{
		SubPatternDefinition *sp_def = sub_pattern_list->data;
		rval = update_tag (ce, table, sp_def->style,
				&sp_def->tag) || rval;
		sub_pattern_list = g_slist_next (sub_pattern_list);
	}

	return rval;
}

struct _SyncWithTagTableData
{
	GtkSourceContextEngine *ce;
	gboolean invalidate;
};
typedef struct _SyncWithTagTableData SyncWithTagTableData;

static void
sync_with_tag_table_hash_cb (gpointer key,
			     gpointer value,
			     gpointer user_data)
{
	SyncWithTagTableData *data = user_data;
	ContextDefinition *definition = value;
	gboolean has_tag = retrieve_definition_tag (data->ce, definition);
	data->invalidate = data->invalidate || has_tag;
}

static void
sync_with_tag_table (GtkSourceContextEngine *ce)
{
	SyncWithTagTableData data;

	g_return_if_fail (ce->priv->buffer != NULL);

	/* Check for changes in used tags in the buffer's tag table. */
	data.ce = ce;
	data.invalidate = FALSE;
	g_hash_table_foreach (ce->priv->definitions,
		sync_with_tag_table_hash_cb, &data);
	if (data.invalidate)
		enable_highlight (ce, FALSE);
}

static void
tag_table_changed_cb (GtkSourceTagTable      *table,
		      GtkSourceContextEngine *ce)
{
	g_return_if_fail (GTK_IS_SOURCE_CONTEXT_ENGINE (ce));

	g_return_if_fail (GTK_TEXT_TAG_TABLE (table) ==
		gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (ce->priv->buffer)));

	sync_with_tag_table (ce);
}

static void
buffer_notify_cb (GObject    *object,
		  GParamSpec *pspec,
		  gpointer    user_data)
{
	GtkSourceBuffer *buffer;
	GtkSourceContextEngine *ce;
	gboolean highlight;

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (object));
	g_return_if_fail (GTK_IS_SOURCE_CONTEXT_ENGINE (user_data));

	ce = GTK_SOURCE_CONTEXT_ENGINE (user_data);
	buffer = GTK_SOURCE_BUFFER (object);

	highlight = gtk_source_buffer_get_highlight (buffer);
	if (highlight != ce->priv->highlight)
	{
		ce->priv->highlight = highlight;
		enable_highlight (ce, highlight);
	}
}

static void
forget_definition_tag_hash_cb (gpointer key,
			       gpointer value,
			       gpointer user_data)
{
	ContextDefinition *definition = value;
	GtkSourceContextEngine *ce = user_data;
	forget_tag (ce, &definition->tag);
}

static void
gtk_source_context_engine_attach_buffer (GtkSourceEngine *engine,
					 GtkSourceBuffer *buffer)
{
	GtkSourceContextEngine *ce;
	GtkTextTagTable *table;

	g_return_if_fail (GTK_IS_SOURCE_CONTEXT_ENGINE (engine));
	g_return_if_fail (buffer == NULL || GTK_IS_SOURCE_BUFFER (buffer));
	ce = GTK_SOURCE_CONTEXT_ENGINE (engine);

	/* Detach previous buffer if there is one. */
	if (ce->priv->buffer != NULL)
	{
		gpointer modify;

		enable_highlight (ce, FALSE);

		/* Disconnect signals. */
		table = gtk_text_buffer_get_tag_table (
			GTK_TEXT_BUFFER (ce->priv->buffer));
		g_signal_handlers_disconnect_matched (table,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, ce);
		g_signal_handlers_disconnect_matched (ce->priv->buffer,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, ce);

		if (ce->priv->worker_handler)
		{
			g_source_remove (ce->priv->worker_handler);
			ce->priv->worker_handler = 0;
		}

		g_hash_table_foreach (ce->priv->definitions,
			forget_definition_tag_hash_cb, ce);

		/* If not previously attached they are NULL */
		if (ce->priv->refresh_region != NULL)
			gtk_text_region_destroy (ce->priv->refresh_region, FALSE);
		if (ce->priv->highlight_requests != NULL)
			gtk_text_region_destroy (ce->priv->highlight_requests, FALSE);
		ce->priv->refresh_region = NULL;
		ce->priv->highlight_requests = NULL;

		while ((modify = g_queue_pop_head (ce->priv->modifications)))
			g_free (modify);
	}

	ce->priv->buffer = buffer;

	if (buffer != NULL)
	{
		ContextDefinition *main_definition;

		/* Retrieve references to all text tags. */
		sync_with_tag_table (ce);

		/* Create the root context. */
		gchar *root_id;
		root_id = g_strdup_printf ("%s:%s", ce->priv->id, ce->priv->id);
		main_definition = g_hash_table_lookup (ce->priv->definitions, 
				root_id);
		g_free (root_id);

		if (main_definition == NULL)
		{
			g_warning (_ ("Missing main language "
				      "definition (id = \"%s\".)"),
				ce->priv->id);
			return;
		}
		ce->priv->root_context = context_new (main_definition, NULL, 0, NULL);

		ce->priv->highlight = gtk_source_buffer_get_highlight (buffer);

		/* Highlight data. */
		ce->priv->refresh_region = gtk_text_region_new (
			GTK_TEXT_BUFFER (buffer));
		ce->priv->highlight_requests = gtk_text_region_new (
			GTK_TEXT_BUFFER (buffer));

		/* Initially the buffer is empty so it's entirely analyzed. */
		ce->priv->worker_last_offset = -1;
		ce->priv->worker_batch_size = INITIAL_WORKER_BATCH;

		g_signal_connect (buffer, "text_inserted",
			G_CALLBACK (text_inserted_cb), ce);
		g_signal_connect (buffer, "text_deleted",
			G_CALLBACK (text_deleted_cb), ce);
		g_signal_connect (buffer, "update_highlight",
			G_CALLBACK (update_highlight_cb), ce);
		g_signal_connect (buffer, "notify::highlight",
			G_CALLBACK (buffer_notify_cb), ce);

		table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer));
		if (GTK_IS_SOURCE_TAG_TABLE (table))
		{
			g_signal_connect (table, "changed",
				G_CALLBACK (tag_table_changed_cb), ce);
		}
		else
		{
			g_warning ("Please use GtkSourceTagTable with GtkSourceBuffer.");
		}
	}
}


/* REGEX HANDLING --------------------------------------------------------- */

/**
 * regex_ref:
 *
 * @regex: a #Regex.
 *
 * Increases the reference count of the regular expression.
 *
 * Return value: the regex.
 */
static Regex *
regex_ref (Regex *regex)
{
	if (regex == NULL)
		return NULL;
	regex->ref_count++;
	return regex;
}

/**
 * regex_unref:
 *
 * @regex: a #Regex.
 *
 * Decreases the reference count of the regular expression.
 */
static void
regex_unref (Regex *regex)
{
	if (regex == NULL)
		return;

	regex->ref_count--;

	if (regex->ref_count <= 0)
	{
		if (regex->resolved)
			egg_regex_free (regex->regex);
		else
			g_free (regex->info.pattern);
		g_free (regex);
	}
}

/**
 * regex_new:
 *
 * @pattern: the regular expression.
 * @flags: compile options for @pattern.
 * @error: location to store the error occuring, or NULL to ignore errors.
 *
 * Creates a new regex.
 *
 * Return value: a newly-allocated #Regex.
 */
static Regex *
regex_new (const gchar           *pattern,
	   EggRegexCompileFlags   flags,
	   GError               **error)
{
	EggRegex *start_ref = egg_regex_new (START_REF_REGEX, 0, 0, NULL);
	Regex *regex = g_new0 (Regex, 1);
	if (egg_regex_match (start_ref, pattern, 0))
	{
		regex->resolved = FALSE;
		regex->info.pattern = g_strdup (pattern);
		regex->info.flags = flags;
	}
	else
	{
		regex->resolved = TRUE;
		regex->regex = egg_regex_new (pattern, flags, 0, error);
		if (regex->regex == NULL)
		{
			g_free (regex);
			regex = NULL;
		}
		else
		{
			egg_regex_optimize (regex->regex, NULL);
		}
	}
	egg_regex_free (start_ref);
	return regex_ref (regex);
}

struct _ReplaceStartRegex
{
	Regex       *start_regex;
	const gchar *matched_text;
};
typedef struct _ReplaceStartRegex ReplaceStartRegex;

static gint
sub_pattern_to_int (gchar *name)
{
	gint number;
	gchar *end_name;

	number = g_ascii_strtoull (name, &end_name, 0);
	if (*end_name == '\0')
	{
		/* The name is entirely numerical, so we treat it
		 * as an integer. */
		return number;
	}

	return -1;
}

static gboolean
replace_start_regex (const EggRegex *regex,
		     const gchar    *matched_text,
		     GString        *expanded_regex,
		     gpointer        user_data)
{
	gchar *num_string, *subst, *subst_escaped, *escapes;
	gint num;
	ReplaceStartRegex *data = user_data;

	escapes = egg_regex_fetch (regex, matched_text, 1);
	num_string = egg_regex_fetch (regex, matched_text, 2);
	num = sub_pattern_to_int (num_string);
	if (num < 0)
		subst = egg_regex_fetch_named (data->start_regex->regex,
				data->matched_text, num_string);
	else
		subst = egg_regex_fetch (data->start_regex->regex,
				data->matched_text, num);

	if (subst != NULL)
	{
		subst_escaped = egg_regex_escape_string (subst, -1);
	}
	else
	{
		g_warning ("Invalid group: %s", num_string);
		subst_escaped = g_strdup ("");
	}

	g_string_append (expanded_regex, escapes);
	g_string_append (expanded_regex, subst_escaped);

	g_free (escapes);
	g_free (num_string);
	g_free (subst);
	g_free (subst_escaped);

	return FALSE;
}

/**
 * regex_resolve:
 *
 * @regex: a #Regex.
 * @start_regex: a #Regex.
 * @matched_text: the text matched against @start_regex.
 *
 * If the regular expression does not contain references to the start
 * regular expression, the functions increases the reference count
 * of @regex and returns it.
 *
 * If the regular expression contains references to the start regular
 * expression in the form "\%{start_sub_pattern@start}", it replaces
 * them (they are extracted from @start_regex and @matched_text) and
 * returns the new regular expression.
 *
 * Return value: a #Regex.
 */
static Regex *
regex_resolve (Regex       *regex,
	       Regex       *start_regex,
	       const gchar *matched_text)
{
	if (regex == NULL)
		return NULL;

	if (regex->resolved)
	{
		return regex_ref (regex);
	}
	else
	{
		ReplaceStartRegex data;
		gchar *expanded_regex;
		Regex *new_regex;

		EggRegex *start_ref = egg_regex_new (START_REF_REGEX, 0,
			0, NULL);
		data.start_regex = start_regex;
		data.matched_text = matched_text;
		expanded_regex = egg_regex_replace_eval (start_ref,
			regex->info.pattern, -1, 0, replace_start_regex,
			&data, 0);
		new_regex = regex_new (expanded_regex, regex->info.flags, NULL);
		if (!new_regex || !new_regex->resolved)
		{
			regex_unref (new_regex);
			g_warning ("Regular expression %s cannot be expanded.",
				regex->info.pattern);
			/* Returns a regex that nevers matches. */
			new_regex = regex_new ("$never-match^", 0, NULL);
		}
		egg_regex_free (start_ref);
		return new_regex;
	}
}

static gboolean
regex_match (Regex       *regex,
	     const gchar *line,
	     gint         line_length,
	     gint         line_pos)
{
	return  egg_regex_match_extended (regex->regex, line,
		line_length, line_pos, 0, NULL);
}

static void
regex_fetch_pos (Regex       *regex,
		 const gchar *matched_text,
		 gint         num,
		 gint        *start_pos,
		 gint        *end_pos)
{
	if (!egg_regex_fetch_pos (regex->regex, matched_text, num,
		start_pos, end_pos))
	{
		if (start_pos != NULL)
			*start_pos = END_NOT_YET_FOUND;
		if (end_pos != NULL)
			*end_pos = END_NOT_YET_FOUND;
	}
}

static void
regex_fetch_named_pos (Regex       *regex,
		       const gchar *matched_text,
		       const gchar *name,
		       gint        *start_pos,
		       gint        *end_pos)
{
	if (!egg_regex_fetch_named_pos (regex->regex, matched_text, name,
		start_pos, end_pos))
	{
		if (start_pos != NULL)
			*start_pos = END_NOT_YET_FOUND;
		if (end_pos != NULL)
			*end_pos = END_NOT_YET_FOUND;
	}
}

static const gchar *
regex_get_pattern (Regex *regex)
{
	g_return_val_if_fail (regex->resolved, "");
	if (regex == NULL)
		return NULL;
	else
		return egg_regex_get_pattern (regex->regex);
}


/* DEFINITIONS MANAGEMENT ------------------------------------------------- */

static ContextDefinition *
definition_new (gchar              *id,
		ContextType         type,
		ContextDefinition  *parent,
		gchar              *match,
		gchar              *start,
		gchar              *end,
		gchar              *style,
		gboolean            extend_parent,
		gboolean            end_at_line_end,
		GError            **error)
{
	ContextDefinition *definition;
	gboolean regex_error = FALSE;
	gboolean unresolved_error = FALSE;

	g_return_val_if_fail (id != NULL, NULL);

	switch (type)
	{
		case CONTEXT_TYPE_SIMPLE:
			g_return_val_if_fail (match != NULL, NULL);
			break;
		case CONTEXT_TYPE_CONTAINER:
			if (end != NULL)
				g_return_val_if_fail (start != NULL, NULL);
			break;
	}

	definition = g_new0 (ContextDefinition, 1);

	if (match != NULL)
	{
		definition->match = regex_new (match,
			EGG_REGEX_ANCHORED, error);
		if (definition->match == NULL)
			regex_error = TRUE;
		else if (!definition->match->resolved)
		{
			unresolved_error = TRUE;
			regex_unref (definition->match);
			definition->match = NULL;
		}
	}

	if (start != NULL)
	{
		definition->start_end.start = regex_new (start,
			EGG_REGEX_ANCHORED, error);
		if (definition->start_end.start == NULL)
			regex_error = TRUE;
		else if (!definition->match->resolved)
		{
			unresolved_error = TRUE;
			regex_unref (definition->start_end.start);
			definition->start_end.start = NULL;
		}
	}

	if (end != NULL)
	{
		definition->start_end.end = regex_new (end,
			EGG_REGEX_ANCHORED, error);
		if (definition->start_end.end == NULL)
			regex_error = TRUE;
	}

	if (unresolved_error)
	{
		g_set_error (error,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR_INVALID_START_REF,
			"context '%s' cannot contain a \\%%{...@start} command",
			id);
		regex_error = TRUE;
	}

	if (regex_error)
	{
		g_free (definition);
		return NULL;
	}

	definition->id = g_strdup (id);
	definition->style = g_strdup (style);
	definition->type = type;
	definition->extend_parent = extend_parent;
	definition->end_at_line_end = end_at_line_end;
	definition->children = NULL;
	definition->tag = NULL;
	definition->sub_patterns = NULL;

	/* Main contexts (i.e. the contexts with id "language:language")
	 * should have extend-parent="true" and end-at-line-end="false". */
	if (parent == NULL)
	{
		EggRegex *r = egg_regex_new ("(.*):\\1", 0, 0, NULL);
		if (egg_regex_match (r, id, 0))
		{
			if (end_at_line_end)
			{
				g_warning ("end-at-line-end should be "
					"\"false\" for main contexts (id: %s)",
					id);
				definition->end_at_line_end = FALSE;
			}
			if (!extend_parent)
			{
				g_warning ("extend-parent should be "
					"\"true\" for main contexts (id: %s)",
					id);
				definition->extend_parent = TRUE;
			}
		}
		egg_regex_free (r);
	}

	return definition;
}

static void
definition_free (ContextDefinition *definition)
{
	GSList *sub_pattern_list;

	if (definition == NULL)
		return;

	switch (definition->type)
	{
		case CONTEXT_TYPE_SIMPLE:
			regex_unref (definition->match);
			break;
		case CONTEXT_TYPE_CONTAINER:
			regex_unref (definition->start_end.start);
			regex_unref (definition->start_end.end);
			break;
	}

	sub_pattern_list = definition->sub_patterns;
	while (sub_pattern_list != NULL)
	{
		SubPatternDefinition *sp_def = sub_pattern_list->data;
#if defined(ENABLE_DEBUG) || defined(ENABLE_PRINT_TREE)
		g_free (sp_def->id);
#endif
		g_free (sp_def->style);
		g_object_unref (sp_def->tag);
		if (sp_def->is_named)
			g_free (sp_def->name);
		g_free (sp_def);
		sub_pattern_list = g_slist_next (sub_pattern_list);
	}
	g_slist_free (definition->sub_patterns);

	g_free (definition->id);
	g_free (definition->style);
	regex_unref (definition->reg_all);

	g_slist_foreach (definition->children, (GFunc)g_free, NULL);
	g_slist_free (definition->children);
}


static void
definition_get_iter (ContextDefinition      *definition,
		     DefinitionsIter        *iter)
{
	iter->children_stack = g_slist_prepend (NULL, definition->children);
}

static ContextDefinition *
definition_iter_next (DefinitionsIter *iter)
{
	GSList *children_list;

	if (iter->children_stack == NULL)
		return NULL;

	children_list = iter->children_stack->data;
	if (children_list == NULL)
	{
		iter->children_stack = g_slist_delete_link (
			iter->children_stack, iter->children_stack);
		return definition_iter_next (iter);
	}
	else
	{
		DefinitionChild *curr_child = children_list->data;
		ContextDefinition *definition = curr_child->definition;
		children_list = g_slist_next (children_list);
		iter->children_stack->data = children_list;
		if (curr_child->is_ref_all)
		{
			iter->children_stack = g_slist_prepend (
				iter->children_stack, definition->children);
			return definition_iter_next (iter);
		}
		else
		{
			return definition;
		}
	}
}

/* CONTEXTS MANAGEMENT ---------------------------------------------------- */

/**
 * context_set_last_sibling:
 *
 * @context:
 * @last_sibling:
 *
 * Sets the cached value for the last sibling.
 */
static void
context_set_last_sibling (Context *context, Context *last_sibling)
{
	g_return_if_fail (context != NULL);
	g_return_if_fail (last_sibling == NULL || last_sibling->next == NULL);

	if (context->parent != NULL)
		context->parent->last_child = last_sibling;

#ifdef ENABLE_VERIFY_TREE
	g_assert (last_sibling == NULL || last_sibling->parent == context->parent);
#endif
}

/**
 * context_last:
 *
 * @context:
 *
 * Returns the last sibling of @context.
 *
 * Return value: the last sibling or %NULL if @context is %NULL.
 */
static Context *
context_last (Context *context)
{
	Context *last;

	if (context == NULL)
		return NULL;

#ifndef ENABLE_VERIFY_TREE
	/* Use the cached value if available. */
	if (context->parent != NULL && context->parent->last_child != NULL)
		return context->parent->last_child;
#endif

	last = context;
	while (last->next != NULL)
		last = last->next;

#ifdef ENABLE_VERIFY_TREE
	/* Verify that the cached value is correct. */
	if (context->parent != NULL && context->parent->last_child != NULL)
		g_assert (context->parent->last_child == last);
#endif

	context_set_last_sibling (context, last);
	return last;
}

/**
 * context_append_child:
 *
 * @context:
 * @child:
 *
 * Appends @child to the list of children of @context.
 */
static void
context_append_child (Context *context, Context *child)
{
	g_return_if_fail (context != NULL);
	g_return_if_fail (child != NULL);
	g_return_if_fail (child->next == NULL);

	if (context->children == NULL)
	{
		context->children = child;
	}
	else
	{
		Context *last_child = context_last (context->children);
		last_child->next = child;
		child->prev = last_child;
	}
	context->last_child = child;
}

static Regex *
create_reg_all (Context *context, ContextDefinition *definition)
{
	DefinitionsIter iter;
	ContextDefinition *child_def;
	GString *all;
	Regex *regex;

	g_return_val_if_fail ((context == NULL && definition != NULL) ||
		(context != NULL && definition == NULL), NULL);

	if (definition == NULL)
		definition = context->definition;

	all = g_string_new ("(");

	/* Closing regex. */
	if (definition->type == CONTEXT_TYPE_CONTAINER &&
	    definition->start_end.end != NULL)
	{
		Regex *end;
		if (definition->start_end.end->resolved)
		{
			end = definition->start_end.end;
		}
		else
		{
			g_return_val_if_fail (context != NULL, NULL);
			end = context->end;
			g_return_val_if_fail (end != NULL, NULL);
		}
		g_string_append (all, regex_get_pattern (end));
		g_string_append (all, "|");
	}

	/* Ancestors. */
	if (context)
	{
		Context *tmp_context = context;
		while (ANCESTOR_CAN_END_CONTEXT (tmp_context))
		{
			if (!tmp_context->definition->extend_parent)
			{
				g_string_append (all,
					regex_get_pattern (
						tmp_context->parent->end));
				g_string_append (all, "|");
			}
			tmp_context = tmp_context->parent;
		}
	}

	/* Children. */
	definition_get_iter (definition, &iter);
	while ((child_def = definition_iter_next (&iter)))
	{
		Regex *child_regex;
		switch (child_def->type)
		{
			case CONTEXT_TYPE_CONTAINER:
				child_regex = child_def->start_end.start;
				break;
			case CONTEXT_TYPE_SIMPLE:
				child_regex = child_def->match;
				break;
			default:
				g_string_free (all, TRUE);
				g_return_val_if_reached (NULL);
		}
		if (child_regex != NULL)
		{
			g_string_append (all,
				regex_get_pattern (child_regex));
			g_string_append (all, "|");
		}
	}

	if (all->len > 1)
		g_string_erase (all, all->len - 1, 1);
	g_string_append (all, ")");

	regex = regex_new (all->str, 0, NULL);
	if (regex == NULL)
	{
		/* regex_new could fail, for instance if there are different
		 * named sub-patterns with the same name. */
		g_warning ("Cannot create a regex for all the transitions, "
			"the syntax highlighting process will be slower "
			"than usual.");
	}

	g_string_free (all, TRUE);

	return regex;
}

/**
 * context_new:
 *
 * @definition: a #ContextDefinition.
 * @parent: a #Context containing the new context.
 * @start_at: beggining offset of the context.
 * @end_text: the text matched by @context->definition->start_end.start.
 *
 * Creates a new context.
 * @end_text is needed to resolve the end regex if this is a container
 * context. If this is not a container context @end_text should be
 * %NULL.
 *
 * Return value: a newly-allocated context.
 */
static Context *
context_new (ContextDefinition *definition,
	     Context           *parent,
	     gint               start_at,
	     const gchar       *end_text)
{
	Context *new_context = g_new0 (Context, 1);
	new_context->definition = definition;
	new_context->parent = parent;
	new_context->start_at = start_at;
	new_context->end_at = END_NOT_YET_FOUND;
	new_context->children = NULL;
	new_context->end = NULL;

	if (parent &&
	    (!parent->all_ancestors_extend ||
	     !parent->definition->extend_parent))
	{
		new_context->all_ancestors_extend = FALSE;
	}
	else
	{
		new_context->all_ancestors_extend = TRUE;
	}

	if (parent != NULL)
		context_append_child (parent, new_context);

	/* Do we need to clear the tag of an outer context?
	 * See the definition of Context for an explanation. */
	if (new_context->definition->tag != NULL)
	{
		gint priority = gtk_text_tag_get_priority (
			GTK_TEXT_TAG (new_context->definition->tag));
		GtkSourceTag *clear_tag = NULL;
		Context *ancestor = new_context->parent;
		while (ancestor != NULL)
		{
			if (ancestor->definition->tag != NULL)
			{
				gint ancestor_priority =
					gtk_text_tag_get_priority (
						GTK_TEXT_TAG (
						ancestor->definition->tag));
				if (ancestor_priority > priority)
				{
					clear_tag = ancestor->definition->tag;
					break;
				}
			}
			ancestor = ancestor->parent;
		}
		new_context->clear_tag = clear_tag;
	}

	if (end_text != NULL)
	{
		g_return_val_if_fail (definition->type == CONTEXT_TYPE_CONTAINER,
			NULL);
		new_context->end = regex_resolve (definition->start_end.end,
			definition->start_end.start, end_text);
	}

	/* Create reg_all. If it is possibile we share the same reg_all
	 * for more contexts storing it in the definition. */
	if (ANCESTOR_CAN_END_CONTEXT (new_context) ||
	    (definition->start_end.end != NULL &&
	     !definition->start_end.end->resolved))
	{
		new_context->reg_all = create_reg_all (new_context, NULL);
	}
	else
	{
		if (definition->reg_all == NULL)
			definition->reg_all = create_reg_all (NULL,
				definition);
		new_context->reg_all = regex_ref (definition->reg_all);
	}

	return new_context;
}

/**
 * context_destroy:
 *
 * @context: a #Context to delete.
 *
 * Deletes @context and its children. If @context is in a tree you need
 * to remove it using context_remove().
 */
static void
context_destroy (Context *context)
{
	Context *child;

	g_return_if_fail (context != NULL);

	child = context->children;
	while (child != NULL)
	{
		Context *next = child->next;
		context_destroy (child);
		child = next;
	}

	g_slist_foreach (context->sub_patterns, (GFunc)g_free, NULL);
	g_slist_free (context->sub_patterns);

	regex_unref (context->end);
	regex_unref (context->reg_all);

	g_free (context);
}

/**
 * context_remove:
 *
 * @context: a #Context.
 *
 * Removes @context from the contexts' tree containing it. After the tree
 * has been removed it can be deleted with context_destroy().
 */
static void
context_remove (Context *context)
{
	if (context->parent == NULL)
		return;

	if (context->prev != NULL)
		context->prev->next = NULL;
	else
		context->parent->children = context->next;

	if (context->next != NULL)
		context->next->prev = context->prev;

	context_set_last_sibling (context, NULL);
	context->prev = context->next = NULL;
	context->parent = NULL;
}

/**
 * context_dup:
 *
 * @context: a #Context to duplicate.
 *
 * Copies @context, the copy does not have child contexts and
 * sub-patterns.
 *
 * Return value: a copy of @context.
 */
static Context *
context_dup (const Context *context)
{
	Context *ret = g_new (Context, 1);
	memcpy (ret, context, sizeof (Context));
	ret->children = NULL;
	ret->sub_patterns = NULL;
	regex_ref (context->end);
	regex_ref (ret->reg_all);
	return ret;
}


/* IDLE WORKER CODE ------------------------------------------------------- */

static gboolean
idle_worker (GtkSourceContextEngine *ce)
{
	GtkTextRegionIterator reg_iter;
	GtkTextIter start_iter, end_iter, last_end_iter;

	g_return_val_if_fail (ce->priv->buffer != NULL, FALSE);

	if (!g_queue_is_empty (ce->priv->modifications))
		/* Do asynchronous modifications. */
		async_modify (ce);
	else if (ce->priv->worker_last_offset >= 0)
		/* The contexts' tree is incomplete. */
		update_syntax (ce, NULL, -1, 0);

	if (ce->priv->highlight)
	{
		/* Highlight subregions requested by the views. */
		gtk_text_buffer_get_iter_at_offset (
			GTK_TEXT_BUFFER (ce->priv->buffer), &last_end_iter, 0);

		gtk_text_region_get_iterator (ce->priv->highlight_requests,
					      &reg_iter, 0);

		while (!gtk_text_region_iterator_is_end (&reg_iter))
		{
			gtk_text_region_iterator_get_subregion (&reg_iter,
								&start_iter,
								&end_iter);

			if (ce->priv->worker_last_offset < 0 ||
			    ce->priv->worker_last_offset >= gtk_text_iter_get_offset (&end_iter))
			{
				ensure_highlighted (ce, &start_iter, &end_iter);
				last_end_iter = end_iter;
			}
			else
			{
				/* Since the subregions are ordered, we are
				 * guaranteed here that all subsequent
				 * subregions will be beyond the already
				 * analyzed text. */
				break;
			}

			gtk_text_region_iterator_next (&reg_iter);
		}

		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (ce->priv->buffer),
			&start_iter, 0);

		if (!gtk_text_iter_equal (&start_iter, &last_end_iter))
		{
			/* Remove already highlighted subregions from requests. */
			gtk_text_region_subtract (ce->priv->highlight_requests,
				&start_iter, &last_end_iter);
		}
	}

	if (ce->priv->worker_last_offset < 0 &&
	    g_queue_is_empty (ce->priv->modifications))
	{
		/* Idle handler will be removed. */
		ce->priv->worker_handler = 0;
		return FALSE;
	}

	return TRUE;
}

static void
install_idle_worker (GtkSourceContextEngine *ce)
{
	if (ce->priv->worker_handler == 0) {
		/* Use the text view validation priority to get
		 * highlighted text even before complete validation of
		 * the buffer. */
		ce->priv->worker_handler =
			g_idle_add_full (GTK_TEXT_VIEW_PRIORITY_VALIDATE,
				(GSourceFunc) idle_worker, ce, NULL);
	}
}


/* READING A LINE FROM THE BUFFER ----------------------------------------- */

/* If we need to analyze an entire batch, reading it in a single step is
 * faster than reading it line by line.
 * However if the text has been modified, usually we need to analyze only
 * some lines.
 *
 * So, if we surely need to analyze all the batch or if a long text is
 * inserted, we read the batch.
 * If a short text has been added/removed we read the buffer line by line;
 * but if, after some lines, the syntax trees cannot be joined we read all
 * the remaining text.
 *
 * This structure is not used directly but through line_reader_*()
 * functions. */
struct _LineReader
{
	GtkTextIter	 start;
	GtkTextIter	 end;

	gchar		*line;
	gint		 line_length;
	gint		 line_byte_length;
	gint		 line_starts_at;

	gchar		*text;

	gboolean	 has_more_lines;
	gint		 read_lines;
};
typedef struct _LineReader LineReader;

/* How many lines can be read before reading all the remaining text
 * in a single step. */
#define MAX_LINES 4

static LineReader *
line_reader_new (const GtkTextIter *start,
		 const GtkTextIter *end,
		 gboolean           read_all)
{
	LineReader *reader;

	reader = g_new0 (LineReader, 1);

	reader->start = *start;
	reader->end = *end;
	reader->has_more_lines = TRUE;
	reader->read_lines = read_all ? MAX_LINES : 0;

	return reader;
}

static void
line_reader_free (LineReader *reader)
{
	if (reader == NULL)
		return;

	if (reader->text != NULL)
		g_free (reader->text);
	else
		g_free (reader->line);

	g_free (reader);
}

static gchar *
line_reader_get_line (LineReader *reader,
		      gint       *line_length,
		      gint       *line_starts_at)
{
	if (!reader->has_more_lines)
		return NULL;

	if (reader->read_lines >= MAX_LINES)
	{
		/* Read the text from reader->start to reader->end. */
		reader->text = gtk_text_iter_get_slice (&reader->start,
			&reader->end);
		reader->line = reader->text;
		reader->line_length = 0;
		reader->line_byte_length = 0;
		reader->line_starts_at = gtk_text_iter_get_offset (
			&reader->start);
		reader->read_lines = -1;
	}

	if (reader->read_lines == -1)
	{
		gchar *prev_line;
		gchar *eol_ptr;
		gboolean eol_found;

		/* Move to the new line using the previous line length. */
		prev_line = reader->line;
		reader->line = reader->line + reader->line_byte_length;

		if (reader->line [0] == '\0')
			/* We have reached the end of the batch. */
			return NULL;

		/* Search the end of the line, */
		eol_found = FALSE;
		eol_ptr = reader->line;
		while (*eol_ptr != '\0' && !eol_found)
		{
			gunichar unichar = g_utf8_get_char (eol_ptr);
			eol_ptr = g_utf8_next_char (eol_ptr);
			switch (g_unichar_break_type (unichar))
			{
				case G_UNICODE_BREAK_MANDATORY:
				case G_UNICODE_BREAK_LINE_FEED:
					eol_found = TRUE;
					break;
				default:
					break;
			}
		}
		/* Length of the line including the '\n' if present. */
		reader->line_byte_length = eol_ptr - reader->line;
		reader->line_length = g_utf8_pointer_to_offset (
			reader->line, reader->line + reader->line_byte_length);

		reader->line_starts_at += g_utf8_pointer_to_offset (
			prev_line, reader->line);
	}
	else
	{
		GtkTextIter line_end = reader->start;
		/* Move line_end to the beggining of the next line, so we
		 * include the '\n'. */
		if (!gtk_text_iter_forward_line (&line_end))
		{
			/* There are no more lines after the current one,
			 * so just move the iterator to the end of the
			 * line. */
			line_end = reader->start;
			gtk_text_iter_forward_to_line_end (&line_end);
		}

		if (reader->line != NULL)
			g_free (reader->line);

		reader->line = gtk_text_iter_get_slice (&reader->start, &line_end);
		reader->line_starts_at = gtk_text_iter_get_offset (&reader->start);
		reader->line_length = g_utf8_strlen (reader->line, -1);

		reader->read_lines++;

		/* Do we have other lines to analyze after the current one? */
		if (!gtk_text_iter_forward_line (&reader->start) ||
		    gtk_text_iter_compare (&reader->start, &reader->end) >= 0)
		{
			reader->has_more_lines = FALSE;
		}
	}

	*line_starts_at = reader->line_starts_at;
	*line_length = reader->line_length;

	return reader->line;
}


/* SYNTAX ANALYSIS CODE --------------------------------------------------- */

/**
 * text_modified:
 *
 * @ce: a GtkSourceContextEngine.
 * @offset: where the modification was done.
 * @delta: how many characters were added or removed.
 *
 * Updates the syntax if @delta characters where added or removed at @offset.
 */
static void
text_modified (GtkSourceContextEngine *ce,
	       gint                    offset,
	       gint                    delta)
{
	if (ABS (delta) == 1 && g_queue_is_empty (ce->priv->modifications))
	{
		update_syntax (ce, NULL, offset, delta);
	}
	else
	{
		Modify *modify = g_queue_peek_tail (ce->priv->modifications);
		if (modify != NULL &&
		    modify->offset + modify->delta == offset &&
		    SIGN (modify->delta) == SIGN (delta))
		{
			/* The two modifications can be joined in a
			 * single one. */
			modify->offset = MIN (modify->offset, offset);
			modify->delta += delta;
		}
		else
		{
			modify = g_new0 (Modify, 1);
			modify->delta = delta;
			modify->offset = offset;
			g_queue_push_tail (ce->priv->modifications, modify);
			install_idle_worker (ce);
		}
	}
}

/**
 * async_modify:
 *
 * @ce: a #GtkSourceContextEngine.
 *
 * Does a single asynchronous modification.
 *
 * Return value: %TRUE is the modification has been done, %FALSE if the
 * queue is empty.
 */
static gboolean
async_modify (GtkSourceContextEngine *ce)
{
	Modify *modify;
	gboolean clean_updates;

	modify = g_queue_pop_head (ce->priv->modifications);
	if (modify == NULL)
		return FALSE;

	update_syntax (ce, NULL, modify->offset, modify->delta);

	/* If we have not updated all the tree in a single step, the
	 * modifications after ce->priv->worker_last_offset are no more
	 * needed. */
	clean_updates = ce->priv->worker_last_offset != -1 &&
		ce->priv->worker_last_offset < modify->offset +
		MAX (modify->delta, 0);
	g_free (modify);
	if (clean_updates)
	{
		gint n_del = 0;
		GList *curr = ce->priv->modifications->head;
		while (curr != NULL)
		{
			modify = curr->data;
			if (modify->offset >= ce->priv->worker_last_offset)
			{
				/* Remove modify from the list. */
				GList *tmp;
				n_del++;
				if (curr->prev != NULL)
					curr->prev->next = curr->next;
				else
					ce->priv->modifications->head = curr->next;
				if (curr->next != NULL)
					curr->next->prev = curr->prev;
				tmp = curr;
				curr = g_list_next (curr);
				g_free (modify);
				g_free (tmp);
			}
			else
			{
				curr = g_list_next (curr);
			}
		}
		ce->priv->modifications->length -= n_del;
		ce->priv->modifications->tail = g_list_last (ce->priv->modifications->head);
	}

	return TRUE;
}

/**
 * get_next_context:
 *
 * @current_context: a @Context.
 * @offset: the current position.
 *
 * Returns the next context beginning after @offset if we are currently
 * in @current_context.
 *
 * Return value: the next context or %NULL if @current_context is the
 * last context.
 */
static Context *
get_next_context (Context *current_context,
		  gint     offset)
{
	/* FIRST STEP. Search it among the children. */
	Context *next_context = current_context->children;
	while (next_context != NULL)
	{
		if (next_context->start_at >= offset)
			break;
		next_context = next_context->next;
	}

	if (next_context == NULL)
	{
		/* SECOND STEP. Search the context on the same level of
		 * current_context. The root context does not have contexts
		 * on the same level. */
		next_context = current_context->next;
	}

	if (next_context == NULL)
	{
		/* THIRD STEP. Search the context after the parent. We exclude
		 * the root context (it does not have a parent)  and the direct
		 * children of the rooot context (their parent is the root
		 * context, so it does not have contexts after it.) */
		Context *parent = current_context->parent;
		while (parent != NULL && parent->parent != NULL)
		{
			next_context = parent->next;
			if (next_context != NULL)
				/* We have found the context we were looking for,
				 * so we stop the research. */
				parent = NULL;
			else
				/* We have not found the context, so we go up
				 * a level in the tree. */
				parent = parent->parent;
		}
	}

	return next_context;
}

/**
 * ancestor_ends_here:
 *
 * @ce: a #GtkSourceContextEngine.
 * @state: the current state of the parser.
 * @line_starts_at: beginning offset of the line.
 * @line: the line to analyze.
 * @line_pos: the position inside @line.
 * @line_length: the length of @line.
 * @new_state: where to store the new state.
 *
 * Verifies if an ancestor context ends at the current position, if the
 * state changed the new state is stored in @new_state (if it is not
 * %NULL.)
 *
 * Return value: %TRUE if an ancestor ends at the given position.
 */
static gboolean
ancestor_ends_here (GtkSourceContextEngine   *ce,
		    Context                  *state,
		    gint                      line_starts_at,
		    const gchar              *line,
		    gint                      line_pos,
		    gint                      line_length,
		    Context                **new_state)
{
	Context *current_context;
	GSList *current_context_list;
	GSList *check_ancestors;
	Context *terminating_context;

	/* A context can be terminated by the parent if extend_parent is
	 * FALSE, so we need to verify the end of all the parents of
	 * not-extending contexts. The list is ordered by ascending
	 * depth. */
	check_ancestors = NULL;
	current_context = state;
	while (ANCESTOR_CAN_END_CONTEXT (current_context))
	{
		if (!current_context->definition->extend_parent)
			check_ancestors = g_slist_prepend (check_ancestors,
				current_context->parent);
		current_context = current_context->parent;
	}

	/* The first context that ends here terminates its descendants. */
	terminating_context = NULL;
	current_context_list = check_ancestors;
	while (current_context_list != NULL)
	{
		current_context = current_context_list->data;
		if (current_context->end != NULL &&
		    current_context->end->regex != NULL &&
		    regex_match (current_context->end, line,
			line_length, line_pos))
		{
			terminating_context = current_context;
			break;
		}

		current_context_list = g_slist_next (current_context_list);
	}
	g_slist_free (check_ancestors);

	if (new_state != NULL && terminating_context != NULL)
	{
		/* We have found a context that ends here, so we close
		 * all the descendants. terminating_context will be
		 * closed by next_context(). */
		gint end_offset = line_starts_at + line_pos;
		current_context = state;
		while (current_context != terminating_context)
		{
			current_context->end_at = end_offset;
			current_context = current_context->parent;
		}
		*new_state = terminating_context;
	}

	return terminating_context != NULL;
}

/**
 * apply_sub_patterns:
 *
 * @ce: a #GtkSourceContextEngine.
 * @contextstate: a #Context.
 * @line_starts_at: beginning offset of the line.
 * @line: the line to analyze.
 * @line_pos: the position inside @line.
 * @line_length: the length of @line.
 * @regex: regex that matched.
 * @where: kind of sub patterns to apply.
 *
 * Applies sub patterns of kind @where to the matched text.
 */
static void
apply_sub_patterns (GtkSourceContextEngine  *ce,
		    Context                 *context,
		    gint                     line_starts_at,
		    const gchar             *line,
		    gint                     line_pos,
		    Regex                   *regex,
		    SubPatternWhere          where)
{
	GSList *sub_pattern_list = context->definition->sub_patterns;
	while (sub_pattern_list != NULL)
	{
		SubPatternDefinition *sp_def = sub_pattern_list->data;
		gint start_pos;
		gint end_pos;
		gint start, end;

		if (sp_def->where == where)
		{
			if (sp_def->is_named)
				regex_fetch_named_pos (regex, line,
					sp_def->name,
					&start_pos, &end_pos);
			else
				regex_fetch_pos (regex, line,
					sp_def->num,
					&start_pos, &end_pos);
			if (start_pos != END_NOT_YET_FOUND &&
			    start_pos != end_pos)
			{
				SubPattern *sp;
				start = line_starts_at + start_pos;
				end = line_starts_at + end_pos;
				sp = g_new0 (SubPattern, 1);
				sp->start_at = start;
				sp->end_at = end;
				sp->definition = sp_def;
				context->sub_patterns = g_slist_prepend (
					context->sub_patterns, sp);
			}
		}

		sub_pattern_list = g_slist_next (sub_pattern_list);
	}
}

/**
 * apply_match:
 *
 * @ce: a #GtkSourceContextEngine.
 * @state: the current state of the parser.
 * @line_starts_at: beginning offset of the line.
 * @line: the line to analyze.
 * @line_pos: the position inside @line.
 * @line_length: the length of @line.
 * @regex: regex that matched.
 * @where: kind of sub patterns to apply.
 *
 * Moves @line_pos after the matched text. @line_pos is not
 * updated and the function returns %FALSE if the match cannot be
 * applied beacuse an ancestor ends in the middle of the matched
 * text.
 *
 * If the match can be applied the function applies the appropriate
 * sub patterns.
 *
 * Return value: %TRUE if the match can be applied.
 */
static gboolean
apply_match (GtkSourceContextEngine  *ce,
	     Context                 *state,
	     gint                     line_starts_at,
	     const gchar             *line,
	     gint                    *line_pos,
	     gint                     line_length,
	     Regex                   *regex,
	     SubPatternWhere          where)
{
	gint end_match_pos;
	gboolean ancestor_ends;
	gint original_line_pos = *line_pos;

	ancestor_ends = FALSE;
	/* end_match_pos is the position of the end of the matched regex. */
	regex_fetch_pos (regex, line, 0, NULL, &end_match_pos);
	/* Verify if an ancestor ends in the matched text. */
	if (ANCESTOR_CAN_END_CONTEXT (state))
	{
		while (TRUE)
		{
			(*line_pos)++;
			if (*line_pos >= end_match_pos)
				break;

			if (ancestor_ends_here (ce, state, line_starts_at,
				line, *line_pos, line_length, NULL))
			{
				ancestor_ends = TRUE;
				break;
			}
		}
	}
	else
	{
		*line_pos = end_match_pos;
	}

	if (ancestor_ends)
	{
		/* An ancestor ends in the middle of the match, we verify
		 * if the regex matches against the available string before
		 * the end of the ancestor.
		 * For instance in C a net-address context matches even if
		 * it contains the end of a multi-line comment. */
		if (!regex_match (regex, line, *line_pos, original_line_pos))
		{
			/* This match is not valid, so we can try to match
			 * the next definition, so the position should not
			 * change. */
			*line_pos = original_line_pos;
			return FALSE;
		}
	}

	apply_sub_patterns (ce, state, line_starts_at, line,
		original_line_pos, regex, where);

	return TRUE;
}

static gboolean
container_context_starts_here (GtkSourceContextEngine  *ce,
			       Context                 *state,
			       ContextDefinition       *curr_definition,
			       gint                     line_starts_at,
			       const gchar             *line,
			       gint                    *line_pos,
			       gint                     line_length,
			       Context                **new_state)
{
	/* We can have a container context definition (i.e. the main
	 * language definition) without start_end.start. */
	if (curr_definition->start_end.start == NULL)
		return FALSE;

	if (regex_match (curr_definition->start_end.start, line,
		line_length, *line_pos))
	{
		gint offset = line_starts_at + *line_pos;
		Context *new_context = context_new (curr_definition,
			state, offset, line);
		if (apply_match (ce, new_context, line_starts_at, line,
			line_pos, line_length,
			curr_definition->start_end.start,
			SUB_PATTERN_WHERE_START))
		{
			*new_state = new_context;
			return TRUE;
		}
		else
		{
			context_remove (new_context);
			context_destroy (new_context);
			return FALSE;
		}
	}
	else
	{
		return FALSE;
	}
}

static gboolean
simple_context_starts_here (GtkSourceContextEngine  *ce,
			    Context                 *state,
			    ContextDefinition       *curr_definition,
			    gint                     line_starts_at,
			    const gchar             *line,
			    gint                    *line_pos,
			    gint                     line_length,
			    Context                **new_state)
{
	g_return_val_if_fail (curr_definition->match != NULL, FALSE);
	if (regex_match (curr_definition->match, line,
		line_length, *line_pos))
	{
		gint offset = line_starts_at + *line_pos;
		Context *new_context = context_new (curr_definition,
			state, offset, NULL);
		if (apply_match (ce, new_context, line_starts_at, line,
			line_pos, line_length,
			curr_definition->match,
			SUB_PATTERN_WHERE_DEFAULT))
		{
			new_context->end_at = line_starts_at + *line_pos;
			return TRUE;
		}
		else
		{
			context_remove (new_context);
			context_destroy (new_context);
			return FALSE;
		}
	}
	else
	{
		return FALSE;
	}
}

/**
 * context_starts_here:
 *
 * @ce: a #GtkSourceContextEngine.
 * @state: the current state of the parser.
 * @curr_definition: a #ContextDefinition.
 * @line_starts_at: beginning offset of the line.
 * @line: the line to analyze.
 * @line_pos: the position inside @line.
 * @line_length: the length of @line.
 * @new_state: where to store the new state.
 *
 * Verifies if a context of the type in @curr_definition starts at
 * @line_pos in @line. If the contexts start here @new_state and
 * @line_pos are updated.
 *
 * Return value: %TRUE if the context starts here.
 */
static gboolean
context_starts_here (GtkSourceContextEngine  *ce,
		     Context                 *state,
		     ContextDefinition       *curr_definition,
		     gint                     line_starts_at,
		     const gchar             *line,
		     gint                    *line_pos,
		     gint                     line_length,
		     Context                **new_state)
{
	switch (curr_definition->type)
	{
		case CONTEXT_TYPE_SIMPLE:
			return simple_context_starts_here (ce, state,
				curr_definition, line_starts_at, line,
				line_pos, line_length, new_state);
		case CONTEXT_TYPE_CONTAINER:
			return container_context_starts_here (ce, state,
				curr_definition, line_starts_at, line,
				line_pos, line_length, new_state);
		default:
			g_return_val_if_reached (FALSE);
	}
}

/**
 * next_context:
 *
 * @ce: a #GtkSourceContextEngine.
 * @state: the current state of the parser.
 * @line_starts_at: beginning offset of the line.
 * @line: the line to analyze.
 * @line_pos: the position inside @line.
 * @line_length: the length of @line.
 * @new_state: where to store the new state.
 *
 * Verifies if a context starts or ends in @line at @line_pos of after it.
 * If the contexts starts or ends here @new_state and @line_pos are updated.
 *
 * Return value: %FALSE is there are no more contexts in @line.
 */
static gboolean
next_context (GtkSourceContextEngine  *ce,
	      Context                 *state,
	      gint                     line_starts_at,
	      const                    gchar *line,
	      gint                    *line_pos,
	      gint                     line_length,
	      Context                **new_state)
{
	*new_state = NULL;

	while (*line_pos < line_length)
	{
		DefinitionsIter def_iter;
		gboolean context_end_found;
		ContextDefinition *child_def;

		if (state->reg_all != NULL)
		{
			if (!regex_match (state->reg_all, line, line_length,
				*line_pos))
			{
				return FALSE;
			}

			regex_fetch_pos (state->reg_all, line, 0,
				line_pos, NULL);
		}

		/* Does an ancestor end here? */
		if (ANCESTOR_CAN_END_CONTEXT (state) &&
		    ancestor_ends_here (ce, state, line_starts_at, line,
			*line_pos, line_length, new_state))
		{
			return TRUE;
		}

		/* Does the current context end here? */
		if (state->definition->start_end.end != NULL &&
		    regex_match (state->end, line, line_length,
			*line_pos))
		{
			context_end_found = TRUE;
		}
		else
		{
			context_end_found = FALSE;
		}

		/* Iter over the definitions we can find in the current
		 * context. */
		definition_get_iter (state->definition, &def_iter);
		while ((child_def = definition_iter_next (&def_iter)))
		{
			/* If the child definition does not extend the parent
			 * and the current context could end here we do not
			 * need to examine this child. */
			if (child_def->extend_parent || !context_end_found)
			{
				/* Does this child definition start a new
				 * context at the current position? */
				if (context_starts_here (ce, state, child_def,
					line_starts_at, line, line_pos,
					line_length, new_state))
				{
					return TRUE;
				}
			}

			/* This child definition does not start here, so we
			 * analyze another definition. */
		}
		if (context_end_found)
		{
			/* We have found that the current context could end
			 * here and that it cannot be extended by a child. */
			apply_match (ce, state,	line_starts_at, line,
				line_pos, line_length, state->end,
				SUB_PATTERN_WHERE_END);
			state->end_at = line_starts_at + *line_pos;
			*new_state = state->parent;
			return TRUE;
		}
		/* At this position there is nothing new, so we examine the
		 * following char. */
		(*line_pos)++;
	}
	return FALSE;
}

/**
 * get_context_at:
 *
 * @ce: a #GtkSourceContextEngine.
 * @offset: an offset in the buffer.
 *
 * Returns the current context at the position contained in @offset.
 *
 * Return value: a #Context.
 */
static Context *
get_context_at (GtkSourceContextEngine *ce,
		gint                    offset)
{
	Context *curr_context;
	Context *ret = ce->priv->root_context;
	gboolean maybe_child;

	do
	{
		maybe_child = FALSE;
		curr_context = ret->children;
		while (curr_context != NULL)
		{
			if (curr_context->start_at <= offset &&
			    curr_context->end_at > offset)
			{
				/* Ok, we are in curr_context, but maybe we
				 * are in a sub context of it. */
				ret = curr_context;
				maybe_child = TRUE;
				break;
			}
			curr_context = curr_context->next;
		}
	} while (maybe_child);

	return ret;
}

/**
 * move_offset:
 * @offset: the offset to move.
 * @modification_offset: where the modification was done.
 * @delta: how many characters were added or removed.
 *
 * Moves an offset to its new position after @delta characters were added
 * (delta > 0) or removed (@delta < 0) at the offset @modification_offset.
 *
 * Return value: the new offset.
 */
static gint
move_offset (gint offset,
	     gint modification_offset,
	     gint delta)
{
	if (offset == END_NOT_YET_FOUND ||
	    offset < modification_offset)
	{
		return offset;
	}
	else
	{
		return offset + delta;
	}
}

/**
 * split_contexts_tree:
 *
 * @ce: a #GtkSourceContextEngine.
 * @start: where to split the contexts' tree.
 *
 * Splits the contexts tree, eliminating the contexts that start after
 * @start.
 *
 * Return value: the removed contexts' tree.
 */
static Context *
split_contexts_tree (GtkSourceContextEngine *ce,
		     GtkTextIter             start)
{
	GSList *common_context_list;
	GSList *common_context_element;
	Context *moved_context;
	Context *common_context;
	Context *common_context_copy = NULL;
	Context *removed_tree;
	Context *last_copied_context;
	Context *current_context;
	gint start_offset;

	/* This function splits the contexts' tree; in ce->priv->root_context
	 * we leave the tree of the contexts that are surely valid (they
	 * starts before the start of the line where the modification
	 * occurred.) The return value of the function is the tree of
	 * contexts removed, join_contexts_tree() will join the two trees
	 * if states_are_equal() detects that a part of the removed tree
	 * is still valid.
	 *
	 *
	 * FIRST CASE: insertion at the beginning of the buffer
	 *
	 * Suppose we have this C code (where /+ means the beginning of a
	 * multi-line comment and +/ is its end) to analyze:
	 *
	 *     /+ This is a comment at the beggining of the buffer
	 *      + And this is an address: http://www.gnome.org/ +/
	 *     void foo ();
	 *     [ more code ]
	 *
	 * The contexts' tree will be:
	 *
	 *                    c
	 *                    |
	 *        -------------------------
	 *        |           |           |
	 *     comment     keyword       ...
	 *        |
	 *     address
	 *
	 * If we insert some text in the first line we need to keep only
	 * "c" in ce->priv->root_context, create a copy of "c" (called "c'")
	 * in the removed tree and move the sub-contexts in it:
	 *
	 *     c                      c'
	 *                            |
	 *                -------------------------
	 *                |           |           |
	 *             comment     keyword       ...
	 *                |
	 *             address
	 *
	 *
	 * SECOND CASE: no child contexts after the modification offset
	 *
	 * This is the example code:
	 *     [ some code ]
	 *     /+ This is a comment
	 *      + This is the second line of the comment. +/
	 *     void foo ();
	 *     [ more code ]
	 *
	 * The tree will be:
	 *
	 *                          c
	 *                          |
	 *        ---------------------------------------
	 *        |           |            |            |
	 *       ...       comment      keyword        ...
	 *
	 * If we insert some text in the second line of the comment we
	 * need to keep the current context ("comment") and the contexts
	 * before it.
	 * The current context is copied in the removed tree.
	 * The contexts after the current context are moved after the copy.
	 * These operations are repeated for the ancestors.
	 * If we have sub-contexts in the current context before the
	 * modification offset, they are left in the original tree.
	 * We obtain:
	 *
	 *            c                               c'
	 *            |                               |
	 *      -------------             -------------------------
	 *      |           |             |           |           |
	 *     ...       comment       comment'    keyword       ...
	 *
	 *
	 * THIRD CASE: child contexts after the modification offset
	 *
	 * This is like the second case but with a child context after
	 * the modification offset (the address in the example):
	 *     [ some code ]
	 *     /+ This is a comment at the beggining of the buffer
	 *      + And this is an address: http://www.gnome.org/ +/
	 *     void foo ();
	 *     [ more code ]
	 *
	 * The tree is:
	 *
	 *                          c
	 *                          |
	 *        ---------------------------------------
	 *        |           |            |            |
	 *       ...       comment      keyword        ...
	 *                    |
	 *                 address
	 *
	 * If we insert some text in the second line of the comment we
	 * should do the operations for the second case, then we move
	 * the children after the modification offset in the removed
	 * tree:
	 *
	 *            c                               c'
	 *            |                               |
	 *      -------------             -------------------------
	 *      |           |             |           |           |
	 *     ...       comment       comment'    keyword       ...
	 *                                |
	 *                             address
	 *
	 */
	if (gtk_text_iter_is_start (&start))
	{
		/* First case. */
		if (!ce->priv->root_context->children)
			return NULL;
		common_context = ce->priv->root_context;
		moved_context = ce->priv->root_context->children;
		start_offset = gtk_text_iter_get_offset (&start);
	}
	else
	{
		Context *next_context;
		/* We move backward so we are in the last valid context. */
		gtk_text_iter_backward_char (&start);
		start_offset = gtk_text_iter_get_offset (&start);
		common_context = get_context_at (ce, start_offset);
		/* Maybe common_context is a context that matched the end
		 * of the line (for instance "c:comment-continue"), so we
		 * need to go up to the first not closed context. */
		while (common_context->end_at == start_offset + 1)
			common_context = common_context->parent;
		next_context = get_next_context (common_context, start_offset);
		if (next_context == NULL || next_context->parent != common_context)
			/* Second case. */
			moved_context = NULL;
		else
			/* Third case. */
			moved_context = next_context;
	}

	/* These are the common contexts that need to be copied in the
	 * removed tree, the first context in the list is the root context. */
	common_context_list = NULL;
	while (common_context != NULL)
	{
		common_context_list = g_slist_prepend (common_context_list,
			common_context);
		common_context = common_context->parent;
	}

	/* Copy the common contexts. */
	removed_tree = NULL;
	last_copied_context = NULL;
	common_context_element = common_context_list;
	while (common_context_element)
	{
		GSList *tmp_list;
		GSList *sub_pattern_list;

		common_context = common_context_element->data;
		common_context_copy = context_dup (common_context);
		/* We are in common_context, so we have not found its end. */
		common_context->end_at = END_NOT_YET_FOUND;

		/* Splits the sub-patterns list between common_context and
		 * common_context_copy. */
		tmp_list = common_context->sub_patterns;
		sub_pattern_list = tmp_list;
		common_context->sub_patterns = NULL;
		while (sub_pattern_list != NULL)
		{
			Context *sp_dest_context;
			SubPattern *sp = sub_pattern_list->data;
			if (sp->end_at <= start_offset)
				sp_dest_context = common_context;
			else
				sp_dest_context = common_context_copy;
			sp_dest_context->sub_patterns = g_slist_append (
				sp_dest_context->sub_patterns, sp);
			sub_pattern_list = g_slist_next (sub_pattern_list);
		}
		g_slist_free (tmp_list);

		/* The root of the removed tree is the copy of the root of
		 * ce->priv->root_context. */
		if (removed_tree == NULL)
			removed_tree = common_context_copy;

		/* Split the list of contexts. */
		common_context->next = NULL;
		/* We are breaking the list of contexts after common_context,
		 * so the last context will be common_context. */
		context_set_last_sibling (common_context, common_context);
		common_context_copy->prev = NULL;

		/* last_copied_context is the parent of the copied context. */
		if (last_copied_context)
			last_copied_context->children = common_context_copy;

		/* Set the parent of the copied contexts and of the contexts
		 * after it. */
		current_context = common_context_copy;
		while (current_context != NULL)
		{
			current_context->parent = last_copied_context;
			current_context = current_context->next;
		}

		/* If common_context_copy is not the last context then
		 * context->parent->last_child is still valid else we
		 * need to update it. */
		if (common_context_copy->next == NULL)
			context_set_last_sibling (common_context_copy,
				common_context_copy);

		/* The parent of the next common context is the current
		 * context. */
		last_copied_context = common_context_copy;

		/* Next context to analyze. */
		common_context_element = g_slist_next (common_context_element);
	}
	g_slist_free (common_context_list);

	/* Move the child contexts after the modification offset. */
	if (moved_context)
	{
		Context *old_parent = moved_context->parent;
		Context *new_parent = last_copied_context;
		g_assert (new_parent != NULL);

		new_parent->children = moved_context;
		new_parent->last_child = NULL;
		old_parent->last_child = NULL;
		/* Split the list of contexts, updating old_parent->children
		 * if it is needed. */
		if (moved_context->prev == NULL)
		{
			old_parent->children = NULL;
		}
		else
		{
			moved_context->prev->next = NULL;
			moved_context->prev = NULL;
		}

		/* Set the new parent for moved_context and for the contexts
		 * after it. */
		current_context = moved_context;
		while (current_context != NULL)
		{
			current_context->parent = new_parent;
			current_context = current_context->next;
		}
	}

	return removed_tree;
}

/**
 * move_tree_offsets:
 *
 * @context: a #Context.
 * @modification_offset: where the modification was done.
 * @delta: how many characters were added or removed.
 *
 * Moves the offsets in @context and in its children.
 */
static void
move_tree_offsets (Context	*context,
		   gint		 modification_offset,
		   gint		 delta)
{
	GSList *sub_pattern_list;
	Context *child;

	context->start_at = move_offset (context->start_at,
		modification_offset, delta);
	context->end_at = move_offset (context->end_at,
		modification_offset, delta);

	sub_pattern_list = context->sub_patterns;
	while (sub_pattern_list != NULL)
	{
		SubPattern *sp = sub_pattern_list->data;
		sp->start_at = move_offset (sp->start_at,
			modification_offset, delta);
		sp->end_at = move_offset (sp->end_at,
			modification_offset, delta);
		sub_pattern_list = g_slist_next (sub_pattern_list);
	}

	child = context->children;
	while (child)
	{
		move_tree_offsets (child, modification_offset, delta);
		child = child->next;
	}
}

/**
 * states_are_equal:
 *
 * @current_state: the current state at @offset.
 * @removed_tree: the old tree.
 * @current_offset: the current offset.
 * @modification_offset: where the modification was done.
 * @delta: how many characters were added or removed.
 *
 * Returns %TRUE if the two trees can be joined. @removed_tree can be
 * modified if some sub-contexts have been surely deleted (i.e. we have
 * not found them even if @current_offset is bigger the their end offset.)
 *
 * Return value: %TRUE if the trees can be joined.
 */
static gboolean
states_are_equal (Context	*current_state,
		  Context	*removed_tree,
		  gint		 current_offset,
		  gint		 modification_offset,
		  gint		 delta)
{
	Context *curr_context;
	gboolean stop = FALSE;
	gboolean states_equal;
	Context *new_state;
	Context *old_state;

	/* Delete contexts that are no more needed. At the end of this loop
	 * curr_context will contain the current position in the old
	 * tree. */
	curr_context = removed_tree;
	while (!stop)
	{
		Context *child = curr_context->children;
		while (child != NULL)
		{
			gint moved_start, moved_end;
			moved_start = move_offset (child->start_at,
				modification_offset, delta);
			moved_end = move_offset (child->end_at,
				modification_offset, delta);
			if (moved_start > current_offset)
				/* This child context begins after the
				 * current position. */
				stop = TRUE;
			if (moved_end <= current_offset)
			{
				/* This child context ends before the
				 * current position, so we delete it and
				 * analyze the following context. */
				Context *tmp = child;
				child = child->next;
				if (child != NULL)
					child->prev = NULL;
				curr_context->children = child;
				context_destroy (tmp);
			}
			else
			{
				/* We have found the context we were looking
				 * for at this level in the tree, now we have
				 * to analyze its children. */
				child = NULL;
			}
		}
		if (curr_context->children != NULL && !stop)
		{
			/* Analyze the children of the current context.
			 * Note that curr_context->children is the last
			 * analyzed child. */
			curr_context = curr_context->children;
		}
		else
		{
			stop = TRUE;
		}
	}

	states_equal = TRUE;
	/* new_state and old_state are the current contexts in the
	 * their trees. */
	new_state = current_state;
	old_state = curr_context;
	while (states_equal && new_state != NULL && old_state != NULL)
	{
		/* Are the two current contexts equal? */
		gboolean contexts_equal = FALSE;
		if (new_state->parent != NULL && old_state->parent != NULL)
		{
			if (new_state->definition == old_state->definition &&
			    new_state->start_at == move_offset (
				old_state->start_at,
				modification_offset, delta) &&
			    egg_regex_equal (new_state->end->regex,
				old_state->end->regex))
			{
				contexts_equal = TRUE;
			}
			else
			{
				contexts_equal = FALSE;
			}
		}
		else
		{
			/* The root contexts are always equal. */
			contexts_equal = TRUE;
		}

		if (!contexts_equal)
		{
			/* The contexts are not equal, so the two states
			 * are different. */
			states_equal = FALSE;
		}
		else
		{
			/* The contexts are equal, so the two states may
			 * be equal. */
			new_state = new_state->parent;
			old_state = old_state->parent;
		}
	}

	/* If the two states have different length (i.e. one of them is NULL
	 * and the other is not NULL) they are different. */
	if (states_equal && new_state != old_state )
		states_equal = FALSE;

	return states_equal;
}

/**
 * join_contexts_tree:
 *
 * @ce: a #GtkSourceContextEngine.
 * @removed_tree: the old tree returned by @split_contexts_tree.
 * @current_offset: the current offset.
 * @modification_offset: where the modification was done.
 * @delta: how many characters were added or removed.
 *
 * Joins the main tree with the old tree (@removed_tree). This function
 * can be called only if states_are_equal() returned %TRUE.
 */
static void
join_contexts_tree (GtkSourceContextEngine *ce,
		    Context                *removed_tree,
		    gint                    current_offset,
		    gint                    modification_offset,
		    gint                    delta)
{
	Context *new_context, *old_context, *current_context, *stop_delete;
	gboolean stop;

	/* Update the offsets in the old tree. */
	move_tree_offsets (removed_tree, modification_offset, delta);

	/* Join the two trees, we need to delete the first node on each
	 * level of the old tree if it is equal to the last node of the new
	 * tree, for instance:
	 *
	 *              c                              c
	 *              |                              |
	 *        -------------            -------------------------
	 *        |           |            |           |           |
	 *     string     comment       comment     keyword       ...
	 *                                 |
	 *                              address
	 *
	 * becomes:
	 *
	 *                          c
	 *                          |
	 *        -------------------------------------
	 *        |           |           |           |
	 *     string      comment     keyword       ...
	 *                    |
	 *                 address
	 */
	stop = FALSE;
	/* We do not need to join the root context, so start from the
	 * second level. */
	new_context = context_last (ce->priv->root_context->children);
	old_context = removed_tree->children;
	while (old_context != NULL && new_context != NULL && !stop)
	{
		GSList *sub_pattern_list;
		Context *last;

		if (old_context->start_at != new_context->start_at)
		{
			/* This are two different contexts, we need
			 * to concatenate them. */
			new_context = new_context->parent;
			break;
		}

		new_context->end_at = old_context->end_at;

		/* Concatenate the two lists deleting old_context. */
		new_context->next = old_context->next;
		if (old_context->next != NULL)
			old_context->next->prev = new_context;

		/* Insert in new_context->sub_patterns the sub_patterns
		 * after the current position. */
		sub_pattern_list = old_context->sub_patterns;
		while (sub_pattern_list != NULL)
		{
			SubPattern *sp = sub_pattern_list->data;
			if (sp->start_at >= current_offset)
			{
				new_context->sub_patterns = g_slist_append (
					new_context->sub_patterns, sp);
				sub_pattern_list->data = NULL;
			}
			sub_pattern_list = g_slist_next (sub_pattern_list);
		}

		/* Update the parent of the contexts in the old tree. */
		current_context = old_context->next;
		while (current_context != NULL)
		{
			current_context->parent = new_context->parent;
			current_context = current_context->next;
		}

		if (new_context->next == NULL)
			/* This is the last sibling. */
			context_set_last_sibling (new_context, new_context);
		else
			/* Use the cached value in the old tree. */
			context_set_last_sibling (new_context,
				old_context->parent->last_child);

		last = context_last (new_context->children);
		if (last != NULL && last->end_at != END_NOT_YET_FOUND)
		{
			old_context = old_context->children;
			stop = TRUE;
		}
		else
		{
			old_context = old_context->children;
			if (new_context->children != NULL)
				/* Continue with the next level. */
				new_context = context_last (new_context->children);
			else
				stop = TRUE;
		}
	}

	/* old_context is the first not joined context, so we do not delete
	 * it and its children. */
	stop_delete = old_context;

	/* Move the contexts from the removed tree to the new tree.
	 * old_context is the first element of the list the we have to copy.
	 * new_context is the last joined context, i.e. it is the new parent
	 * of old_context. */
	if (old_context != NULL)
	{
		Context *last;

		if (new_context == NULL)
			/* If the main tree contains only the root context,
			 * then new_context is initialized to NULL (as we
			 * do not need to join the root context). */
			new_context = ce->priv->root_context;
		if (new_context->children != NULL)
		{
			last = context_last (new_context->children);
			last->next = old_context;
			old_context->prev = last;
		}
		else
		{
			new_context->children = old_context;
		}
		last = old_context->parent->last_child;
		while (old_context != NULL)
		{
			old_context->parent = new_context;
			old_context = old_context->next;
		}
		context_set_last_sibling (new_context->children, last);
	}

	/* We delete the contexts removed from the list after concatenation. */
	old_context = removed_tree;
	while (old_context != NULL && old_context != stop_delete)
	{
		/* Only the first child needs to be deleted, the other
		 * children are now in the main tree. */
		Context *next = NULL;
		if (old_context->children != NULL)
			next = old_context->children;
		old_context->children = NULL;
		context_destroy (old_context);
		old_context = next;
	}
}

/**
 * end_at_line_end:
 *
 * @ce: a #GtkSourceContextEngine.
 * @state: the current state.
 * @end_offset: the offset of the end of the line.
 *
 * Closes the contexts that cannot contain end of lines if needed. 
 *
 * Return value: the new state.
 */
static Context *
end_at_line_end (GtkSourceContextEngine   *ce,
		 Context                  *state,
		 gint                      end_offset)
{
	Context *current_context;
	Context *terminating_context;

	/* A context can be terminated by the parent if extend_parent is
	 * FALSE, so we need to verify the end of all the parents of
	 * not-extending contexts. */
	terminating_context = NULL;
	current_context = state;
	do
	{
		if (current_context->definition->end_at_line_end)
			terminating_context = current_context;
		current_context = current_context->parent;
	} while (ANCESTOR_CAN_END_CONTEXT (current_context));

	if (terminating_context != NULL)
	{
		/* We have found a context that ends here, so we close
		 * it and its descendants. */
		current_context = state;
		do
		{
			current_context->end_at = end_offset;
			current_context = current_context->parent;
		} while (current_context != terminating_context->parent);
		return terminating_context->parent;
	}
	else
	{
		return state;
	}
}

/**
 * analyze_line:
 *
 * @ce: a #GtkSourceContextEngine.
 * @modification_offset: where the modification was done.
 * @delta: how many characters were added or removed.
 * @current_state: the current state.
 * @line: the line to analyze.
 * @line_length: the length of @line.
 * @line_starts_at: beginning offset of the line.
 * @removed_tree: the old tree.
 *
 * Analyzes a single line of text and returns the new state. If @removed_tree
 * is used %NULL is returned.
 *
 * Return value: the new state or %NULL if @removed_tree is used.
 */
static Context *
analyze_line (GtkSourceContextEngine *ce,
	      gint                    modification_offset,
	      gint                    delta,
	      Context                *current_state,
	      const gchar            *line,
	      gint                    line_length,
	      gint                    line_starts_at,
	      Context                *removed_tree)
{
	Context *new_state;
	gboolean has_more_contexts = TRUE;
	gint line_pos = 0;
	gboolean old_tree_used = FALSE;

	/* Find the contexts in the line. */
	while (has_more_contexts && !old_tree_used)
	{
		has_more_contexts = next_context (ce, current_state,
			line_starts_at, line, &line_pos,
			line_length, &new_state);

		if (new_state != NULL)
			current_state = new_state;

		if (removed_tree != NULL)
		{
			gint current_offset;
			gboolean offset_ok;

			/* If we do not have more contexts, move to the end
			 * of the line. */
			if (!has_more_contexts)
				line_pos = line_length - 1;

			/* If we have a removed tree we can try to join it
			 * with the tree in ce->priv->root_context, but only
			 * if we are after the modified text. */
			current_offset = line_starts_at + line_pos;
			offset_ok = (current_offset >
				modification_offset + MAX (delta, 0));
			if (offset_ok &&
			    states_are_equal (current_state, removed_tree,
				current_offset, modification_offset, delta))
			{
				/* We can use the old tree. */
				join_contexts_tree (ce, removed_tree,
					current_offset, modification_offset,
					delta);
				old_tree_used = TRUE;
			}
		}
	}

	/* Verify if we need to close the context because we are at
	 * the end of the line. */
	if (!old_tree_used &&
	    (ANCESTOR_CAN_END_CONTEXT (current_state) ||
	     current_state->definition->end_at_line_end))
	{
		current_state = end_at_line_end (ce, current_state,
			line_starts_at + line_length);
	}

	if (old_tree_used)
		return NULL;
	else
		return current_state;
}

/**
 * update_syntax:
 *
 * @ce: a #GtkSourceContextEngine.
 * @needed_end: analyze at least to needed_end.
 * @modification_offset: where the modification was done.
 * @delta: how many characters were added or removed.
 *
 * Updates the syntax tree and syntax highlighting. If you need only
 * to analyze a batch of text and no insertions or deletions were
 * made, then modification_offset should be -1 and delta should be 0.
 */
static void
update_syntax (GtkSourceContextEngine *ce,
	       const GtkTextIter      *needed_end,
	       gint                    modification_offset,
	       gint                    delta)
{
	gint batch_size;
	GtkTextIter start, end;
	gint text_starts_at;
	Context *current_state;
	LineReader *reader;
	GTimer *timer;
	Context *removed_tree;
	gboolean old_tree_used = FALSE;

	g_return_if_fail (ce->priv->buffer != NULL);
	g_return_if_fail ((modification_offset == -1 && delta == 0) ||
			  (modification_offset != -1 && delta != 0));

	/* Check if we still have text to analyze. */
	if (delta == 0 && ce->priv->worker_last_offset < 0)
		return;

	/* If the modification is at an unanalyzed region do the update
	 * in the idle worker. */
	if (ce->priv->worker_last_offset != -1 &&
	    modification_offset > ce->priv->worker_last_offset)
	{
		/* We do not to call install_idle_worker() because, if
		 * worker_last_offset is not -1, it is surely installed. */
		return;
	}

	/* This function is often paused by the scheduler, so it prints
	 * wrong times. Using g_usleep the function is usually suspended
	 * before the timer is started. */
	PROFILE (g_usleep (1));

	timer = g_timer_new ();

	/* Compute starting iter of the batch. */
	if (delta == 0)
		text_starts_at = ce->priv->worker_last_offset;
	else
		text_starts_at = modification_offset;
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (ce->priv->buffer),
		&start, text_starts_at);

	/* Move to the beginning of the line. */
	if (!gtk_text_iter_starts_line (&start))
	{
		/* The analysis starts at the beginning of the line. */
		gtk_text_iter_set_line_offset (&start, 0);
		text_starts_at = gtk_text_iter_get_offset (&start);
	}

	/* Compute ending iter of the batch. If delta is not 0 we use a
	 * smaller batch, so we do not slow the UI while the user is
	 * typing. */
	if (delta == 0)
		batch_size = ce->priv->worker_batch_size;
	else
		batch_size = MAX (MINIMUM_WORKER_BATCH,
			ce->priv->worker_batch_size / 2);
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (ce->priv->buffer),
		&end, text_starts_at + batch_size);

	/* Extend the range to include needed_end if necessary. */
	if (needed_end && gtk_text_iter_compare (&end, needed_end) < 0)
		end = *needed_end;

	/* Always stop processing at end of lines: this minimizes the
	 * chance of not getting a context because it was split in
	 * between batches. */
	if (!gtk_text_iter_ends_line (&end))
		gtk_text_iter_forward_to_line_end (&end);

	/* We read all the batch if delta is zero or if the text
	 * iserted/deleted is long (so rarely the removed tree can
	 * be used.) */
	reader = line_reader_new (&start, &end,
		delta == 0 || delta > MAX_LINES * 60);

	if (delta != 0)
		removed_tree = split_contexts_tree (ce, start);
	else
		removed_tree = NULL;

	current_state = get_context_at (ce, text_starts_at);
	while (current_state->start_at == text_starts_at &&
	       current_state->parent != NULL)
	{
		current_state = current_state->parent;
	}

	/* We need to eliminate contexts already on this line, in fact we may
	 * have other contexts even if delta is 0, for instance suppose we
	 * have this code:
	 *   foo ();
	 *   /+ Multiline
	 *    + comment. +/
	 * and that the batch ends at the end of the second line.
	 * After the first step we have a "c:multiline-comment" from 8 to
	 * END_NOT_YET_FOUND.
	 * In the second step we analyze another batch, starting at position
	 * 8; the current context is "c:c", so we found a "c:multiline-comment"
	 * starting at position 8 that is a duplicate of the previous one. */
	while (TRUE)
	{
		Context *wrong_context = get_next_context (current_state,
			text_starts_at);
		if (wrong_context != NULL)
		{
			context_remove (wrong_context);
			context_destroy (wrong_context);
		}
		else
		{
			break;
		}
	}

	/* MAIN LOOP: build the tree. */
	while (!old_tree_used)
	{
		gint line_length;
		gint line_starts_at;
		gchar *line;

		line = line_reader_get_line (reader, &line_length,
			&line_starts_at);
		if (line == NULL)
			/* No more lines. */
			break;

		current_state = analyze_line (ce, modification_offset, delta,
			current_state, line, line_length, line_starts_at,
			removed_tree);
		if (current_state == NULL)
			old_tree_used = TRUE;
	}

	line_reader_free (reader);

	if (!old_tree_used && removed_tree != NULL)
		context_destroy (removed_tree);

	/* Update worker_last_offset. */
	if (gtk_text_iter_is_end (&end))
	{
		/* All the text has been analyzed. */
		ce->priv->worker_last_offset = -1;
	}
	else if (old_tree_used)
	{
		if (ce->priv->worker_last_offset != -1)
		{
			/* We have used the old tree, so we can use the
			 * old offset. */
			gint moved_offset = move_offset (
				ce->priv->worker_last_offset,
				modification_offset, delta);
			ce->priv->worker_last_offset = MAX (moved_offset,
				gtk_text_iter_get_offset (&end));
		}
	}
	else
	{
		ce->priv->worker_last_offset = gtk_text_iter_get_offset (&end);
		install_idle_worker (ce);
	}

	/* Update worker_batch_size. */
	if (delta == 0)
	{
		gint length;
		gdouble et;
		gint new_size;

		length = gtk_text_iter_get_offset (&end) -
			gtk_text_iter_get_offset (&start);
		/* elapsed time in milliseconds */
		et = 1000 * g_timer_elapsed (timer, NULL);

		/* make sure the elapsed time is never 0.
		 * This happens in particular with the GTimer
		 * implementation on win32 which is not accurate.
		 * 1 ms seems to work well enough as a fallback.
		 */
		et = et != 0 ? et : 1.0;
		new_size = MIN (length * WORKER_TIME_SLICE / et, G_MAXINT);

		ce->priv->worker_batch_size = MAX (MINIMUM_WORKER_BATCH, new_size);
		DEBUG (g_message ("new batch size: %d",
			ce->priv->worker_batch_size));
	}

	/* Make sure the analyzed region gets highlighted. */
	refresh_range (ce, &start, &end);

	PROFILE (g_message ("ended worker batch (from %d to %d), %g ms elapsed",
		gtk_text_iter_get_offset (&start),
		gtk_text_iter_get_offset (&end),
		g_timer_elapsed (timer, NULL) * 1000));

#ifdef ENABLE_VERIFY_TREE
	verify_tree (ce);
#endif

#ifdef ENABLE_PRINT_TREE
	print_tree ("tree", ce->priv->root_context);
#endif

	g_timer_destroy (timer);
}


/* HIGHLIGHTING CODE ------------------------------------------------------ */

struct _UnhighlightRegionData
{
	GtkSourceContextEngine *ce;
	const GtkTextIter *start, *end;
};
typedef struct _UnhighlightRegionData UnhighlightRegionData;

static void
unhighlight_region_cb (gpointer key,
		       gpointer value,
		       gpointer user_data)
{
	ContextDefinition *definition = value;
	UnhighlightRegionData *data = user_data;
	GSList *sub_pattern_list;

	if (definition->tag != NULL)
		gtk_text_buffer_remove_tag (
			GTK_TEXT_BUFFER (data->ce->priv->buffer),
			GTK_TEXT_TAG (definition->tag),
			data->start, data->end);

	sub_pattern_list = definition->sub_patterns;
	while (sub_pattern_list != NULL)
	{
		SubPatternDefinition *sp_def = sub_pattern_list->data;
		if (sp_def->tag != NULL)
			gtk_text_buffer_remove_tag (
				GTK_TEXT_BUFFER (data->ce->priv->buffer),
				GTK_TEXT_TAG (sp_def->tag),
				data->start, data->end);
		sub_pattern_list = g_slist_next (sub_pattern_list);
	}
}

static void
unhighlight_region (GtkSourceContextEngine *ce,
		    const GtkTextIter      *start,
		    const GtkTextIter      *end)
{
	/* FIXME Find a better way to do this as more definitions
	 * could refer to the same tag. */
	UnhighlightRegionData data = {ce, start, end};
	g_hash_table_foreach (ce->priv->definitions, unhighlight_region_cb,
		&data);
}

/**
 * apply_tag:
 *
 * @buffer: a #GtkSourceBuffer.
 * @context: the context to highlight.
 * @start_region_offset: the beggining of the region to highlight.
 * @end_region_offset: the end of the region to highlight.
 *
 * Highlights the part of @context contained in the interval from
 * @start_region_offset to @end_region_offset.
 */
static void
apply_tag (GtkSourceBuffer *buffer,
	   Context         *context,
	   gint             start_region_offset,
	   gint             end_region_offset)
{
	GtkTextIter start_iter, end_iter;
	GSList *sub_pattern_list;
	gint tag_priority = -1;
	GtkSourceTag *clear_tag = NULL;
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (buffer);

	if (context->definition->tag != NULL)
	{
		gtk_text_buffer_get_iter_at_offset (text_buffer, &start_iter,
			MAX (context->start_at, start_region_offset));
		gtk_text_buffer_get_iter_at_offset (text_buffer, &end_iter,
			MIN (context->end_at, end_region_offset));

		if (context->clear_tag != NULL)
			gtk_text_buffer_remove_tag (GTK_TEXT_BUFFER (buffer),
				GTK_TEXT_TAG (context->clear_tag),
				&start_iter, &end_iter);

		gtk_text_buffer_apply_tag (text_buffer,
			GTK_TEXT_TAG (context->definition->tag),
			&start_iter, &end_iter);

		clear_tag = context->definition->tag;
	}
	else
	{
		/* If the parent context has not a tag we search for a tag
		 * to clear in the ancestors. */
		Context *ancestor = context->parent;
		while (ancestor != NULL && clear_tag == NULL)
		{
			if (ancestor->definition->tag != NULL)
				clear_tag = ancestor->definition->tag;
			ancestor = ancestor->parent;
		}
	}

	if (clear_tag != NULL && context->definition->tag != NULL)
		tag_priority = gtk_text_tag_get_priority (
			GTK_TEXT_TAG (context->definition->tag));

	/* Apply the tags for the sub-patterns. */
	sub_pattern_list = context->sub_patterns;
	while (sub_pattern_list != NULL)
	{
		SubPattern *sp = sub_pattern_list->data;

		if (sp->definition->tag != NULL &&
		    sp->end_at > start_region_offset &&
		    sp->start_at < end_region_offset)
		{
			gtk_text_buffer_get_iter_at_offset (text_buffer,
				&start_iter,
				MAX (sp->start_at, start_region_offset));
			gtk_text_buffer_get_iter_at_offset (text_buffer,
				&end_iter,
				MIN (sp->end_at, end_region_offset));

			if (clear_tag != NULL)
			{
				/* Remove the parent (or ancestor) tag if needed. */
				gint sp_tag_priority = gtk_text_tag_get_priority (
					GTK_TEXT_TAG (sp->definition->tag));
				if (tag_priority > sp_tag_priority)
					gtk_text_buffer_remove_tag (
						GTK_TEXT_BUFFER (buffer),
						GTK_TEXT_TAG (clear_tag),
						&start_iter, &end_iter);
			}

			gtk_text_buffer_apply_tag (text_buffer,
				GTK_TEXT_TAG (sp->definition->tag),
				&start_iter, &end_iter);
		}

		sub_pattern_list = g_slist_next (sub_pattern_list);
	}
}

/**
 * highlight_region:
 *
 * @ce: a #GtkSourceContextEngine.
 * @start: the beginning of the region to highlight.
 * @end: the end of the region to highlight.
 *
 * Highlights the specified region.
 */
static void
highlight_region (GtkSourceContextEngine *ce,
		  GtkTextIter            *start,
		  GtkTextIter            *end)
{
	Context *current_context, *current_parent;
	gint start_region_offset, end_region_offset;
#ifdef ENABLE_PROFILE
	GTimer *timer = g_timer_new ();
#endif

	/* First we need to delete tags in the regions. */
	unhighlight_region (ce, start, end);

	start_region_offset = gtk_text_iter_get_offset (start);
	end_region_offset = gtk_text_iter_get_offset (end);

	/* The contexts to highlight are:
	 *  - the current context;
	 *  - the following contexts returned by get_next_context;
	 *  - the ancestors of the current context.
	 *
	 * Note that apply_tag applies only the part of the tag
	 * in the interval from start_region_offset to
	 * end_region_offset. */
	current_context = get_context_at (ce, start_region_offset);

	/* Ancestors. */
	current_parent = current_context->parent;
	while (current_parent != NULL)
	{
		apply_tag (ce->priv->buffer, current_parent,
			start_region_offset, end_region_offset);
		current_parent = current_parent->parent;
	}

	/* Contexts after the current context. */
	while (current_context != NULL &&
	       current_context->start_at < end_region_offset)
	{
		apply_tag (ce->priv->buffer, current_context,
			start_region_offset, end_region_offset);
		current_context = get_next_context (current_context,
			MAX (current_context->start_at, start_region_offset));
	}

	PROFILE (g_message ("highlight (from %d to %d), %g ms elapsed",
		gtk_text_iter_get_offset (start),
		gtk_text_iter_get_offset (end),
		g_timer_elapsed (timer, NULL) * 1000));
}

static void
refresh_range (GtkSourceContextEngine *ce,
	       GtkTextIter            *start,
	       GtkTextIter            *end)
{
	/* Add this region to the refresh region. */
	gtk_text_region_add (ce->priv->refresh_region, start, end);

	/* Notify views of the updated highlight region */
	g_signal_emit_by_name (ce->priv->buffer, "highlight_updated", start, end);
}

static void
enable_highlight (GtkSourceContextEngine *ce,
		  gboolean                enable)
{
	GtkTextIter start, end;

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (ce->priv->buffer),
			&start, &end);
	if (enable)
		refresh_range (ce, &start, &end);
	else
		unhighlight_region (ce, &start, &end);
}

static void
ensure_highlighted (GtkSourceContextEngine *ce,
		    const GtkTextIter      *start,
		    const GtkTextIter      *end)
{
	GtkTextRegion *region;

	/* Assumes the entire region to highlight has already been analyzed. */

	/* Get the subregions not yet highlighted. */
	region = gtk_text_region_intersect (ce->priv->refresh_region, start, end);
	if (region)
	{
		GtkTextRegionIterator reg_iter;

		gtk_text_region_get_iterator (region, &reg_iter, 0);

		/* Highlight all subregions from the intersection.
                   hopefully this will only be one subregion. */
		while (!gtk_text_region_iterator_is_end (&reg_iter))
		{
			GtkTextIter s, e;

			gtk_text_region_iterator_get_subregion (&reg_iter,
								&s, &e);
			highlight_region (ce, &s, &e);

			gtk_text_region_iterator_next (&reg_iter);
		}

		gtk_text_region_destroy (region, TRUE);

		/* Remove the just highlighted region. */
		gtk_text_region_subtract (ce->priv->refresh_region, start, end);
	}
}

static void
highlight_queue (GtkSourceContextEngine *ce,
		 const GtkTextIter      *start,
		 const GtkTextIter      *end)
{
	gtk_text_region_add (ce->priv->highlight_requests, start, end);

	DEBUG (g_message ("queueing highlight [%d, %d]",
		gtk_text_iter_get_offset (start),
		gtk_text_iter_get_offset (end)));
}


/* ENGINE INITIALIZATION -------------------------------------------------- */

/**
 * gtk_source_context_engine_new:
 * @id: the id of the language.
 *
 * Return value: a new context based highlighting engine
 */
GtkSourceEngine *
gtk_source_context_engine_new (const gchar *id)
{
	GtkSourceEngine *engine;
	GtkSourceContextEngine *ce;

	engine = GTK_SOURCE_ENGINE (g_object_new (
			GTK_TYPE_SOURCE_CONTEXT_ENGINE,
			NULL));
	ce = GTK_SOURCE_CONTEXT_ENGINE (engine);
	ce->priv->id = g_strdup (id);

	return engine;
}

gboolean
gtk_source_context_engine_define_context (GtkSourceContextEngine  *ce,
					  gchar                   *id,
					  gchar                   *parent_id,
					  gchar                   *match_regex,
					  gchar                   *start_regex,
					  gchar                   *end_regex,
					  gchar                   *style,
					  gboolean                 extend_parent,
					  gboolean                 end_at_line_end,
					  GError                 **error)
{
	ContextDefinition *definition, *parent = NULL;
	ContextType type;

	gboolean wrong_args = FALSE;

	DefinitionChild *self;

	g_return_val_if_fail (ce != NULL, FALSE);
	g_return_val_if_fail (id != NULL, FALSE);

	/* If the id is already present in the hastable it is a duplicate,
	 * so we report the error (probably there is a duplicate id in the
	 * XML lang file) */
	if (LOOKUP_DEFINITION (ce, id) != NULL)
	{
		g_set_error (error,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR_DUPLICATED_ID,
			"duplicated context id '%s'", id);
		return FALSE;
	}
	
	if (match_regex != NULL)
		type = CONTEXT_TYPE_SIMPLE;
	else
		type = CONTEXT_TYPE_CONTAINER;

	/* Check if the arguments passed are exactly what we expect, no more, no less. */
	switch (type)
	{
		case CONTEXT_TYPE_SIMPLE:
			if (start_regex || end_regex)
				wrong_args = TRUE;
			break;
		case CONTEXT_TYPE_CONTAINER:
			if (match_regex)
				wrong_args = TRUE;
			break;
	}

	if (wrong_args)
	{
		g_set_error (error,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR_INVALID_ARGS,
			_ ("insufficient or redunduant arguments creating "
			   "the context '%s'"),
			id);
		return FALSE;
	}

	if (parent_id == NULL)
	{
		parent = NULL;
	}
	else
	{
		parent = LOOKUP_DEFINITION (ce, parent_id);
		g_return_val_if_fail (parent != NULL, FALSE);
	}

	definition = definition_new (id, type, parent, match_regex,
		start_regex, end_regex, style, extend_parent, end_at_line_end,
		error);
	if (definition == NULL)
		return FALSE;
	g_hash_table_insert (ce->priv->definitions, g_strdup (id), definition);

	if (parent != NULL)
	{
		self = g_new0 (DefinitionChild, 1);
		self->is_ref_all = FALSE;
		self->definition = definition;
		parent->children = g_slist_append (parent->children, self);
	}

	return TRUE;
}

gboolean
gtk_source_context_engine_add_sub_pattern (GtkSourceContextEngine  *ce,
					   gchar                   *id,
					   gchar                   *parent_id,
					   gchar                   *name,
					   gchar                   *where,
					   gchar                   *style,
					   GError                 **error)
{
	ContextDefinition *parent;
	SubPatternDefinition *sp_def;
	SubPatternWhere where_num;
	gint number;

	g_return_val_if_fail (ce != NULL, FALSE);
	g_return_val_if_fail (id != NULL, FALSE);
	g_return_val_if_fail (parent_id != NULL, FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_CONTEXT_ENGINE (ce), FALSE);

	/* If the id is already present in the hastable it is a duplicate,
	 * so we report the error (probably there is a duplicate id in the
	 * XML lang file) */
	if (LOOKUP_DEFINITION (ce, id) != NULL)
	{
		g_set_error (error,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR_DUPLICATED_ID,
			"duplicated context id '%s'", id);
		return FALSE;
	}

	parent = LOOKUP_DEFINITION (ce, parent_id);
	g_return_val_if_fail (parent != NULL, FALSE);

	if (where == NULL || where [0] == '\0' ||
	    strcmp (where, "default") == 0)
		where_num = SUB_PATTERN_WHERE_DEFAULT;
	else if (strcmp (where, "start") == 0)
		where_num = SUB_PATTERN_WHERE_START;
	else if (strcmp (where, "end") == 0)
		where_num = SUB_PATTERN_WHERE_END;
	else
		where_num = (SubPatternWhere)-1;
	if ((parent->type == CONTEXT_TYPE_SIMPLE &&
	     where_num != SUB_PATTERN_WHERE_DEFAULT) ||
	    (parent->type == CONTEXT_TYPE_CONTAINER &&
	     where_num == SUB_PATTERN_WHERE_DEFAULT))
	{
		where_num = (SubPatternWhere)-1;
	}
	if (where_num == -1)
	{
		g_set_error (error,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR_INVALID_WHERE,
			_ ("invalid location ('%s') for sub pattern '%s'"),
			where, id);
		return FALSE;
	}

	sp_def = g_new0 (SubPatternDefinition, 1);
#if defined(ENABLE_DEBUG) || defined(ENABLE_PRINT_TREE)
	sp_def->id = g_strdup (id);
#endif
	sp_def->style = g_strdup (style);
	sp_def->where = where_num;
	number = sub_pattern_to_int (name);
	if (number < 0)
	{
		sp_def->is_named = TRUE;
		sp_def->name = g_strdup (name);
	}
	else
	{
		sp_def->is_named = FALSE;
		sp_def->num = number;
	}
	parent->sub_patterns = g_slist_prepend (parent->sub_patterns, sp_def);

	return TRUE;
}

gboolean
gtk_source_context_engine_add_ref (GtkSourceContextEngine  *ce,
				   gchar                   *parent_id,
				   gchar                   *ref_id,
				   gboolean                 all,
				   GError                 **error)
{
	ContextDefinition *parent;
	ContextDefinition *ref;
	DefinitionChild *self;

	g_return_val_if_fail (ce != NULL, FALSE);
	g_return_val_if_fail (parent_id != NULL, FALSE);
	g_return_val_if_fail (ref_id != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_CONTEXT_ENGINE (ce), FALSE);

	/* If the id is already present in the hastable it is a duplicate,
	 * so we report the error (probably there is a duplicate id in the
	 * XML lang file). */
	ref = LOOKUP_DEFINITION (ce, ref_id);
	if (ref == NULL)
	{
		g_set_error (error,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR_INVALID_REF,
			_ ("invalid id '%s', the definition does not exist"),
			ref_id);
		return FALSE;
	}
	if (all && ref->type != CONTEXT_TYPE_CONTAINER)
	{
		g_set_error (error,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR_INVALID_REF,
			_ ("context '%s' is not a container context"),
			ref_id);
		return FALSE;
	}

	parent = LOOKUP_DEFINITION (ce, parent_id);
	g_return_val_if_fail (parent != NULL, FALSE);
	
	if (parent->type != CONTEXT_TYPE_CONTAINER)
	{
		g_set_error (error,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR,
			GTK_SOURCE_CONTEXT_ENGINE_ERROR_INVALID_PARENT,
			_ ("invalid parent type for the context '%s'"),
			ref_id);
		return FALSE;
	}

	self = g_new0 (DefinitionChild, 1);
	self->is_ref_all = all;
	self->definition = ref;
	parent->children = g_slist_append (parent->children, self);

	return TRUE;
}


/* DEBUGGING CODE --------------------------------------------------------- */

#if defined (ENABLE_PRINT_TREE) || defined (ENABLE_VERIFY_TREE)
#include <glib/gprintf.h>
#endif


#ifdef ENABLE_PRINT_TREE

static void
print_div (gint depth)
{
	gint i, j;
	g_printf ("  ");
	for (i = 0; i < depth; i++)
	{
		for (j = 0; j < 3; j++)
			g_printf (" ");
	}
}

static void
print_tree_helper (Context *tree, gint depth)
{
	GSList *sub_pattern_list;
	Context *child;

	print_div (depth);
	g_printf (" %s [%d; %d) at %p\n", tree->definition->id, tree->start_at,
		tree->end_at, tree);

	sub_pattern_list = tree->sub_patterns;
	while (sub_pattern_list != NULL)
	{
		SubPattern *sp = sub_pattern_list->data;
		print_div (depth + 1);
		g_printf (" (%s [%d; %d), style=%s at %p)\n",
			sp->definition->id, sp->start_at, sp->end_at,
			sp->definition->style, sp);
		sub_pattern_list = g_slist_next (sub_pattern_list);
	}

	child = tree->children;
	while (child)
	{
		print_tree_helper (child, depth + 1);
		child = child->next;
	}
}

static void
print_tree (const gchar *label, Context *tree)
{
	g_printf ("\nTREE: %s\n", label);
	print_tree_helper (tree, 0);
	g_printf ("\n");
}

#endif /* #ifdef ENABLE_PRINT_TREE */


#ifdef ENABLE_VERIFY_TREE

static void
verify_parent (GtkSourceContextEngine *ce,
	       Context		      *context)
{
	Context *child;

	if (context == NULL)
		return;

	if (context->parent == NULL)
	{
		/* This should be the root context. */
		if (context != ce->priv->root_context)
			g_printf ("Wrong NULL parent for %s [%d; %d) at %p\n\n",
				context->definition->id, context->start_at,
				context->end_at, context);
	}
	else if (ce->priv->root_context == context)
	{
		/* This is the root context but the parent is not NULL. */
		g_printf ("Root context should not have a parent: "
			"%s [%d; %d) at %p\n\n",
			context->definition->id, context->start_at,
			context->end_at, context);
	}

	child = context->children;
	while (child != NULL)
	{
		if (child->parent != context)
			g_printf ("Wrong parent for %s [%d; %d) at %p\n\n",
				child->definition->id, child->start_at,
				child->end_at, child);

		verify_parent (ce, child);

		child = child->next;
	}
}

static void
verify_sequence (GtkSourceContextEngine *ce,
		 Context		*context)
{
	Context *child, *prev_child;

	if (context == NULL)
		return;

	if (context->parent == NULL)
	{
		/* This is the root context. */
		if (context->prev != NULL)
			g_printf ("Root context should not have a previous context: "
				"%s [%d; %d) at %p\n\n",
				context->definition->id, context->start_at,
				context->end_at, context);
		if (context->next != NULL)
			g_printf ("Root context should not have a next context: "
				"%s [%d; %d) at %p\n\n",
				context->definition->id, context->start_at,
				context->end_at, context);
	}

	child = context->children;
	prev_child = NULL;
	while (child != NULL)
	{
		if (child->prev != prev_child)
			g_printf ("Wrong previous pointer for %s [%d; %d) at %p\n\n",
				child->definition->id, child->start_at,
				child->end_at, child);
		else if (prev_child != NULL && prev_child->next != child)
			g_printf ("Wrong next pointer for %s [%d; %d) at %p\n\n",
				prev_child->definition->id, prev_child->start_at,
				prev_child->end_at, prev_child);

		verify_sequence (ce, child);

		prev_child = child;
		child = child->next;
	}

	if (context->children != NULL)
	{
		const gchar *err_msg = NULL;
		Context *expected = NULL;
		Context *obtained = NULL;

		g_assert (prev_child);

		Context *cached_last = context->last_child;
		/* context_last() forces context->last_child to be not NULL. */
		Context *last = context_last (context->children);
		if (cached_last != NULL && cached_last != last)
		{
			err_msg = "Wrong cached value for the last sibling of";
			expected = last;
			obtained = cached_last;
		}

		if (context->last_child != last)
		{
			err_msg = "Wrong cached value (after context_last) for "
				"the last sibling of";
			expected = last;
			obtained = context->last_child;
		}

		if (last != prev_child)
		{
			err_msg = "Wrong last sibling of";
			expected = prev_child;
			obtained = last;
		}

		if (err_msg != NULL)
		{
			g_assert (expected != NULL);
			g_assert (obtained != NULL);

			g_printf ("%s %s [%d; %d) at %p\n"
				"Expected %s [%d; %d) at %p\n"
				"Obtained %s [%d; %d) at %p\n\n",
				err_msg,
				context->children->definition->id,
				context->children->start_at,
				context->children->end_at,
				context->children,
				expected->definition->id,
				expected->start_at,
				expected->end_at,
				expected,
				obtained->definition->id,
				obtained->start_at,
				obtained->end_at,
				obtained);
		}
	}
}

static void
verify_positions (GtkSourceContextEngine *ce,
		  Context		 *context)
{
	Context *child;

	if (context == NULL)
		return;

	if (context->parent == NULL)
	{
		/* Root context */
		if (context->start_at != 0)
			g_printf ("Wrong start position for root context "
				"%s (%d instead of %d) at %p\n\n",
				context->definition->id, context->start_at,
				0, context);
		if (context->end_at != END_NOT_YET_FOUND)
			g_printf ("Wrong end position for root context "
				"%s (%d instead of %d) at %p\n\n",
				context->definition->id, context->start_at,
				END_NOT_YET_FOUND, context);
	}

	if (context->start_at >= context->end_at)
		g_printf ("Wrong position for context "
			"%s [%d; %d) at %p\n\n",
			context->definition->id, context->start_at,
			context->end_at, context);

	child = context->children;
	while (child != NULL)
	{
		if (child->start_at < context->start_at)
			g_printf ("Wrong start position for "
				"%s [%d; %d) at %p\n"
				"The parent is %s [%d; %d) at %p\n\n",
				child->definition->id,
				child->start_at,
				child->end_at,
				child,
				context->definition->id,
				context->start_at,
				context->end_at,
				context);

		if (child->end_at > context->end_at)
			g_printf ("Wrong end position for "
				"%s [%d; %d) at %p\n"
				"The parent is %s [%d; %d) at %p\n\n",
				child->definition->id,
				child->start_at,
				child->end_at,
				child,
				context->definition->id,
				context->start_at,
				context->end_at,
				context);

		if (child->next != NULL && child->end_at > child->next->start_at)
			g_printf ("Wrong sequence position for "
				"%s [%d; %d) at %p and "
				"%s [%d; %d) at %p\n\n",
				child->definition->id,
				child->start_at,
				child->end_at,
				child,
				child->next->definition->id,
				child->next->start_at,
				child->next->end_at,
				child->next);

		verify_positions (ce, child);

		child = child->next;
	}
}

static void
verify_tree (GtkSourceContextEngine *ce)
{
	g_return_if_fail (ce != NULL);

	verify_parent (ce, ce->priv->root_context);
	verify_sequence (ce, ce->priv->root_context);
	verify_positions (ce, ce->priv->root_context);
}

#endif /* #ifdef ENABLE_VERIFY_TREE */

