/*
 * This file is part of GtkSourceView
 *
 * Copyright 2001 Mikael Hermansson <tyan@linux.se> and
 *                Chris Phelps <chicane@reninet.com>
 * Copyright 2002 Jeroen Zwartepoorte
 * Copyright 2003 Gustavo Giráldez and Paolo Maggi
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

#include "config.h"

#include "gtksourceview-private.h"

#include <string.h>
#include <fribidi.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <pango/pango-tabs.h>
#include <glib/gi18n-lib.h>

#include "gtksourcebuffer.h"
#include "gtksourcebuffer-private.h"
#include "gtksourcebufferinternal-private.h"
#include "gtksourcecompletion.h"
#include "gtksourcegutter.h"
#include "gtksourcegutter-private.h"
#include "gtksourcegutterrendererlines-private.h"
#include "gtksourcegutterrenderermarks-private.h"
#include "gtksourcehover-private.h"
#include "gtksourceindenter-private.h"
#include "gtksource-enumtypes.h"
#include "gtksourcemark.h"
#include "gtksourcemarkattributes.h"
#include "gtksource-marshal.h"
#include "gtksourcestylescheme-private.h"
#include "gtksourcecompletion-private.h"
#include "gtksourcecompletionprovider.h"
#include "gtksourceiter-private.h"
#include "gtksourcesearchcontext-private.h"
#include "gtksourcespacedrawer.h"
#include "gtksourcespacedrawer-private.h"
#include "gtksourceannotations.h"
#include "gtksourceannotations-private.h"
#include "gtksourcesnippet.h"
#include "gtksourcesnippetcontext.h"
#include "gtksourcetrace.h"
#include "gtksourceutils-private.h"

/**
 * GtkSourceView:
 *
 * Subclass of [class@Gtk.TextView].
 *
 * `GtkSourceView` is the main class of the GtkSourceView library.
 * Use a [class@Buffer] to display text with a `GtkSourceView`.
 *
 * This class provides:
 *
 *  - Show the line numbers;
 *  - Show a right margin;
 *  - Highlight the current line;
 *  - Indentation settings;
 *  - Configuration for the Home and End keyboard keys;
 *  - Configure and show line marks;
 *  - And a few other things.
 *
 * An easy way to test all these features is to use the test-widget mini-program
 * provided in the GtkSourceView repository, in the tests/ directory.
 *
 * # GtkSourceView as GtkBuildable
 *
 * The GtkSourceView implementation of the [iface@Gtk.Buildable] interface exposes the
 * [property@View:completion] object with the internal-child "completion".
 *
 * An example of a UI definition fragment with GtkSourceView:
 * ```xml
 * <object class="GtkSourceView" id="source_view">
 *   <property name="tab-width">4</property>
 *   <property name="auto-indent">True</property>
 *   <child internal-child="completion">
 *     <object class="GtkSourceCompletion">
 *       <property name="select-on-show">False</property>
 *     </object>
 *   </child>
 * </object>
 * ```
 *
 * # Changing the Font
 *
 * Gtk CSS provides the best way to change the font for a `GtkSourceView` in a
 * manner that allows for components like [class@Map] to scale the desired
 * font.
 *
 * ```c
 * GtkCssProvider *provider = gtk_css_provider_new ();
 * gtk_css_provider_load_from_string (provider,
 *                                   "textview { font-family: Monospace; font-size: 8pt; }");
 * gtk_style_context_add_provider (gtk_widget_get_style_context (view),
 *                                 GTK_STYLE_PROVIDER (provider),
 *                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
 * g_object_unref (provider);
 * ```
 * ```python
 * provider = Gtk.CssProvider()
 * provider.load_from_string("textview { font-family: Monospace; font-size: 8pt; }")
 * style_context = view.get_style_context()
 * style_context.add_provider(provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
 * ```
 *
 * If you need to adjust the font or size of font within a portion of the
 * document only, you should use a [class@Gtk.TextTag] with the [property@Gtk.TextTag:family] or
 * [property@Gtk.TextTag:scale] set so that the font size may be scaled relative to
 * the default font set in CSS.
 */

#define GUTTER_PIXMAP                   16
#define DEFAULT_TAB_WIDTH               8
#define MAX_TAB_WIDTH                   32
#define MAX_INDENT_WIDTH                32

#define DEFAULT_RIGHT_MARGIN_POSITION   80
#define MAX_RIGHT_MARGIN_POSITION       1000

#define RIGHT_MARGIN_LINE_ALPHA         40
#define RIGHT_MARGIN_OVERLAY_ALPHA      15

enum
{
	CHANGE_CASE,
	CHANGE_NUMBER,
	JOIN_LINES,
	LINE_MARK_ACTIVATED,
	MOVE_LINES,
	MOVE_TO_MATCHING_BRACKET,
	MOVE_WORDS,
 	PUSH_SNIPPET,
	SHOW_COMPLETION,
	SMART_HOME_END,
	N_SIGNALS
};

enum
{
	PROP_0,
	PROP_AUTO_INDENT,
	PROP_BACKGROUND_PATTERN,
	PROP_COMPLETION,
	PROP_ENABLE_SNIPPETS,
	PROP_HIGHLIGHT_CURRENT_LINE,
	PROP_INDENT_ON_TAB,
	PROP_INDENT_WIDTH,
	PROP_INDENTER,
	PROP_INSERT_SPACES,
	PROP_RIGHT_MARGIN_POSITION,
	PROP_SHOW_LINE_MARKS,
	PROP_SHOW_LINE_NUMBERS,
	PROP_SHOW_RIGHT_MARGIN,
	PROP_SMART_BACKSPACE,
	PROP_SMART_HOME_END,
	PROP_SPACE_DRAWER,
	PROP_ANNOTATIONS,
	PROP_TAB_WIDTH,
	N_PROPS
};

static GParamSpec *properties[N_PROPS];

typedef struct
{
	GtkSourceStyleScheme *style_scheme;

	GtkSourceSpaceDrawer *space_drawer;

	GtkSourceAnnotations *annotations;

	GHashTable *mark_categories;

	GtkSourceBuffer *source_buffer;

	GtkSourceGutter *left_gutter;
	GtkSourceGutter *right_gutter;

	GtkSourceGutterRenderer *line_renderer;
	GtkSourceGutterRenderer *marks_renderer;

	GdkRGBA background_pattern_color;
	GdkRGBA current_line_background_color;
	GdkRGBA current_line_number_color;
	GdkRGBA current_line_number_background_color;
	GdkRGBA right_margin_line_color;
	GdkRGBA right_margin_overlay_color;

	GtkSourceCompletion *completion;
	GtkSourceHover *hover;
	GtkSourceIndenter *indenter;

	char im_commit_text[32];
	guint im_commit_len;

	guint right_margin_pos;
	gint cached_right_margin_pos;
	guint tab_width;
	gint indent_width;
	GtkSourceSmartHomeEndType smart_home_end;
	GtkSourceBackgroundPatternType background_pattern;

	GtkSourceViewAssistants assistants;

	GtkSourceViewSnippets snippets;

	guint background_pattern_color_set : 1;
	guint current_line_background_color_set : 1;
	guint current_line_number_bold : 1;
	guint current_line_number_color_set : 1;
	guint current_line_number_background_color_set : 1;
	guint right_margin_line_color_set : 1;
	guint right_margin_overlay_color_set : 1;
	guint tabs_set : 1;
	guint show_line_numbers : 1;
	guint show_line_marks : 1;
	guint auto_indent : 1;
	guint insert_spaces : 1;
	guint highlight_current_line : 1;
	guint indent_on_tab : 1;
	guint show_right_margin  : 1;
	guint smart_backspace : 1;
	guint enable_snippets : 1;

	GtkAdjustment *vadj;
	GtkAdjustment *hadj;
} GtkSourceViewPrivate;

typedef struct
{
	GtkSourceMarkAttributes *attributes;
	gint priority;
} MarkCategory;

static guint signals[N_SIGNALS];

static void gtk_source_view_buildable_interface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceView, gtk_source_view, GTK_TYPE_TEXT_VIEW,
                         G_ADD_PRIVATE (GtkSourceView)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_source_view_buildable_interface_init))

static void           gtk_source_view_dispose              (GObject                 *object);
static void           gtk_source_view_finalize             (GObject                 *object);
static void           gtk_source_view_show_completion_real (GtkSourceView           *view);
static GtkTextBuffer *gtk_source_view_create_buffer        (GtkTextView             *view);
static void           remove_source_buffer                 (GtkSourceView           *view);
static void           set_source_buffer                    (GtkSourceView           *view,
                                                            GtkTextBuffer           *buffer);
static void           gtk_source_view_move_cursor          (GtkTextView             *text_view,
                                                            GtkMovementStep          step,
                                                            gint                     count,
                                                            gboolean                 extend_selection);
static void           gtk_source_view_delete_from_cursor   (GtkTextView             *text_view,
                                                            GtkDeleteType            type,
                                                            gint                     count);
static gboolean       gtk_source_view_extend_selection     (GtkTextView             *text_view,
                                                            GtkTextExtendSelection   granularity,
                                                            const GtkTextIter       *location,
                                                            GtkTextIter             *start,
                                                            GtkTextIter             *end);
static void           gtk_source_view_get_lines            (GtkTextView             *text_view,
                                                            gint                     first_y,
                                                            gint                     last_y,
                                                            GArray                  *buffer_coords,
                                                            GArray                  *line_heights,
                                                            GArray                  *numbers,
                                                            gint                    *countp);
static void           gtk_source_view_move_lines           (GtkSourceView           *view,
                                                            gboolean                 down);
static void           gtk_source_view_move_words           (GtkSourceView           *view,
                                                            gint                     step);
static gboolean       gtk_source_view_key_pressed          (GtkSourceView           *view,
                                                            guint                    key,
                                                            guint                    keycode,
                                                            guint                    state,
                                                            GtkEventControllerKey   *controller);
static void 	      gtk_source_view_adjustment_changed   (GtkSourceView *view,
                                                            GtkAdjustment *adjustment);
static void           gtk_source_view_adj_property_changed (GObject    *object,
                                                            GParamSpec *pspec,
                                                            gpointer    user_data);
static void           gtk_source_view_clicked              (GtkSourceView           *view);
static gboolean       gtk_source_view_key_released         (GtkSourceView           *view,
                                                            guint                    key,
                                                            guint                    keycode,
                                                            guint                    state,
                                                            GtkEventControllerKey   *controller);
static gint           calculate_real_tab_width             (GtkSourceView           *view,
                                                            guint                    tab_size,
                                                            gchar                    c);
static void           gtk_source_view_set_property         (GObject                 *object,
                                                            guint                    prop_id,
                                                            const GValue            *value,
                                                            GParamSpec              *pspec);
static void           gtk_source_view_get_property         (GObject                 *object,
                                                            guint                    prop_id,
                                                            GValue                  *value,
                                                            GParamSpec              *pspec);
static void           gtk_source_view_css_changed          (GtkWidget               *widget,
                                                            GtkCssStyleChange       *change);
static void           gtk_source_view_update_style_scheme  (GtkSourceView           *view);
static MarkCategory  *mark_category_new                    (GtkSourceMarkAttributes *attributes,
                                                            gint                     priority);
static void           mark_category_free                   (MarkCategory            *category);
static void           gtk_source_view_snapshot_layer       (GtkTextView             *text_view,
                                                            GtkTextViewLayer         layer,
                                                            GtkSnapshot             *snapshot);
static void           gtk_source_view_snapshot             (GtkWidget               *widget,
                                                            GtkSnapshot             *snapshot);
static void           gtk_source_view_queue_draw           (GtkSourceView           *view);
static gboolean       gtk_source_view_rgba_drop            (GtkDropTarget           *dest,
                                                            const GValue            *value,
                                                            int                      x,
                                                            int                      y,
                                                            GtkSourceView           *view);
static void           gtk_source_view_populate_extra_menu  (GtkSourceView           *view);
static void           gtk_source_view_real_push_snippet    (GtkSourceView           *view,
                                                            GtkSourceSnippet        *snippet,
                                                            GtkTextIter             *location);
static void           gtk_source_view_ensure_redrawn_rect_is_highlighted
                                                           (GtkSourceView           *view,
                                                            GdkRectangle            *clip);

static GtkSourceCompletion *
get_completion (GtkSourceView *self)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (self);

	g_assert (GTK_SOURCE_IS_VIEW (self));

	if (priv->completion == NULL)
	{
		priv->completion = _gtk_source_completion_new (self);
	}

	return priv->completion;
}

static void
gtk_source_view_constructed (GObject *object)
{
	GtkSourceView *view = GTK_SOURCE_VIEW (object);
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	_gtk_source_view_snippets_init (&priv->snippets, view);

	set_source_buffer (view, gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	G_OBJECT_CLASS (gtk_source_view_parent_class)->constructed (object);

	g_signal_connect (view, "notify::vadjustment",
		     	  G_CALLBACK (gtk_source_view_adj_property_changed),
		          NULL);
	g_signal_connect (view, "notify::hadjustment",
		          G_CALLBACK (gtk_source_view_adj_property_changed),
		          NULL);
}

static void
gtk_source_view_focus_changed (GtkSourceView           *view,
                               GtkEventControllerFocus *focus)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_assert (GTK_SOURCE_IS_VIEW (view));
	g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (focus));

	if (priv->left_gutter)
	{
		gtk_widget_queue_draw (GTK_WIDGET (priv->left_gutter));
	}

	if (priv->right_gutter)
	{
		gtk_widget_queue_draw (GTK_WIDGET (priv->right_gutter));
	}
}

static void
gtk_source_view_move_to_matching_bracket (GtkSourceView *view,
                                          gboolean       extend_selection)
{
	GtkTextView *text_view = GTK_TEXT_VIEW (view);
	GtkTextBuffer *buffer;
	GtkTextMark *insert_mark;
	GtkTextIter insert;
	GtkTextIter bracket_match;
	GtkSourceBracketMatchType result;

	buffer = gtk_text_view_get_buffer (text_view);
	insert_mark = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, &insert, insert_mark);

	result = _gtk_source_buffer_find_bracket_match (GTK_SOURCE_BUFFER (buffer),
							&insert,
							NULL,
							&bracket_match);

	if (result == GTK_SOURCE_BRACKET_MATCH_FOUND)
	{
		if (extend_selection)
		{
			gtk_text_buffer_move_mark (buffer, insert_mark, &bracket_match);
		}
		else
		{
			gtk_text_buffer_place_cursor (buffer, &bracket_match);
		}

		gtk_text_view_scroll_mark_onscreen (text_view, insert_mark);
	}
}

static void
gtk_source_view_change_number (GtkSourceView *view,
                               gint           count)
{
	GtkTextView *text_view = GTK_TEXT_VIEW (view);
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *str;

	buffer = gtk_text_view_get_buffer (text_view);
	if (!GTK_SOURCE_IS_BUFFER (buffer))
	{
		return;
	}

	if (!gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
	{
		if (!gtk_text_iter_starts_word (&start))
		{
			GtkTextIter prev;

			gtk_text_iter_backward_word_start (&start);

			/* Include the negative sign if there is one.
			 * https://gitlab.gnome.org/GNOME/gtksourceview/-/issues/117
			 */
			prev = start;
			if (gtk_text_iter_backward_char (&prev) && gtk_text_iter_get_char (&prev) == '-')
			{
				start = prev;
			}
		}

		if (!gtk_text_iter_ends_word (&end))
		{
			gtk_text_iter_forward_word_end (&end);
		}
	}

	str = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	if (str != NULL && *str != '\0')
	{
		gchar *p;
		gint64 n;
		glong len;

		len = gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&start);
		g_assert (len > 0);

		n = g_ascii_strtoll (str, &p, 10);

		/* do the action only if strtoll succeeds (p != str) and
		 * the whole string is the number, e.g. not 123abc
		 */
		if ((p - str) == len)
		{
			gchar *newstr;

			newstr = g_strdup_printf ("%"G_GINT64_FORMAT, (n + count));

			gtk_text_buffer_begin_user_action (buffer);
			gtk_text_buffer_delete (buffer, &start, &end);
			gtk_text_buffer_insert (buffer, &start, newstr, -1);
			gtk_text_buffer_end_user_action (buffer);

			g_free (newstr);
		}

		g_free (str);
	}
}

static void
gtk_source_view_change_case (GtkSourceView           *view,
                             GtkSourceChangeCaseType  case_type)
{
	GtkSourceBuffer *buffer;
	GtkTextIter start, end;

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	gtk_text_view_reset_im_context (GTK_TEXT_VIEW (view));

	if (!gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &start, &end))
	{
		/* if no selection, change the current char */
		gtk_text_iter_forward_char (&end);
	}

	gtk_source_buffer_change_case (buffer, case_type, &start, &end);
}

static void
gtk_source_view_activate_change_case (GtkWidget   *widget,
                                      const gchar *action_name,
                                      GVariant    *parameter)
{
	GEnumClass *klass;
	GEnumValue *value;
	const gchar *nick;

	nick = g_variant_get_string (parameter, NULL);
	klass = g_type_class_ref (GTK_SOURCE_TYPE_CHANGE_CASE_TYPE);
	value = g_enum_get_value_by_nick (klass, nick);

	if (value != NULL)
	{
		gtk_source_view_change_case (GTK_SOURCE_VIEW (widget), value->value);
	}

	g_type_class_unref (klass);
}

static void
gtk_source_view_join_lines (GtkSourceView *view)
{
	GtkSourceBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	gtk_text_view_reset_im_context (GTK_TEXT_VIEW (view));

	gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);

	gtk_source_buffer_join_lines (buffer, &start, &end);
}

static void
gtk_source_view_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
	GtkSourceView *view = GTK_SOURCE_VIEW (widget);
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GdkRectangle visible_rect;

	GTK_WIDGET_CLASS (gtk_source_view_parent_class)->size_allocate (widget, width, height, baseline);

	_gtk_source_view_assistants_size_allocate (&priv->assistants, width, height, baseline);

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (widget), &visible_rect);
	gtk_source_view_ensure_redrawn_rect_is_highlighted (GTK_SOURCE_VIEW (widget), &visible_rect);
}

static void
gtk_source_view_unmap (GtkWidget *widget)
{
	GtkSourceView *view = GTK_SOURCE_VIEW (widget);
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	GTK_WIDGET_CLASS (gtk_source_view_parent_class)->unmap (widget);

	_gtk_source_view_assistants_hide_all (&priv->assistants);
}

static void
gtk_source_view_class_init (GtkSourceViewClass *klass)
{
	GObjectClass *object_class;
	GtkTextViewClass *textview_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	textview_class = GTK_TEXT_VIEW_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructed = gtk_source_view_constructed;
	object_class->dispose = gtk_source_view_dispose;
	object_class->finalize = gtk_source_view_finalize;
	object_class->get_property = gtk_source_view_get_property;
	object_class->set_property = gtk_source_view_set_property;

	widget_class->snapshot = gtk_source_view_snapshot;
	widget_class->css_changed = gtk_source_view_css_changed;
	widget_class->size_allocate = gtk_source_view_size_allocate;
	widget_class->unmap = gtk_source_view_unmap;

	textview_class->move_cursor = gtk_source_view_move_cursor;
	textview_class->delete_from_cursor = gtk_source_view_delete_from_cursor;
	textview_class->extend_selection = gtk_source_view_extend_selection;
	textview_class->create_buffer = gtk_source_view_create_buffer;
	textview_class->snapshot_layer = gtk_source_view_snapshot_layer;

	klass->show_completion = gtk_source_view_show_completion_real;
	klass->move_lines = gtk_source_view_move_lines;
	klass->move_words = gtk_source_view_move_words;
	klass->push_snippet = gtk_source_view_real_push_snippet;

	/**
	 * GtkSourceView:completion:
	 *
	 * The completion object associated with the view
	 */
	properties [PROP_COMPLETION] =
		g_param_spec_object ("completion",
		                     "Completion",
		                     "The completion object associated with the view",
		                     GTK_SOURCE_TYPE_COMPLETION,
		                     (G_PARAM_READABLE |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceView:enable-snippets:
	 *
	 * The property denotes if snippets should be
	 * expanded when the user presses Tab after having typed a word
	 * matching the snippets found in [class@SnippetManager].
	 *
	 * The user may tab through focus-positions of the snippet if any
	 * are available by pressing Tab repeatedly until the desired focus
	 * position is selected.
	 */
	properties [PROP_ENABLE_SNIPPETS] =
		g_param_spec_boolean ("enable-snippets",
		                      "Enable Snippets",
		                      "Whether to enable snippet expansion",
		                      FALSE,
		                      (G_PARAM_READWRITE |
		                       G_PARAM_EXPLICIT_NOTIFY |
		                       G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceView:show-line-numbers:
	 *
	 * Whether to display line numbers
	 */
	properties [PROP_SHOW_LINE_NUMBERS] =
		g_param_spec_boolean ("show-line-numbers",
		                      "Show Line Numbers",
		                      "Whether to display line numbers",
		                      FALSE,
		                      (G_PARAM_READWRITE |
				       G_PARAM_EXPLICIT_NOTIFY |
		                       G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceView:show-line-marks:
	 *
	 * Whether to display line mark pixbufs
	 */
	properties [PROP_SHOW_LINE_MARKS] =
		g_param_spec_boolean ("show-line-marks",
		                      "Show Line Marks",
		                      "Whether to display line mark pixbufs",
		                      FALSE,
		                      (G_PARAM_READWRITE |
		                       G_PARAM_EXPLICIT_NOTIFY |
		                       G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceView:tab-width:
	 *
	 * Width of a tab character expressed in number of spaces.
	 */
	properties [PROP_TAB_WIDTH] =
		g_param_spec_uint ("tab-width",
		                   "Tab Width",
		                   "Width of a tab character expressed in spaces",
		                   1,
		                   MAX_TAB_WIDTH,
		                   DEFAULT_TAB_WIDTH,
		                   (G_PARAM_READWRITE |
		                    G_PARAM_EXPLICIT_NOTIFY |
		                    G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceView:indenter:
	 *
	 * The property is a [iface@Indenter] to use to indent
	 * as the user types into the [class@View].
	 */
	properties [PROP_INDENTER] =
		g_param_spec_object ("indenter",
		                     "Indenter",
		                     "A indenter to use to indent typed text",
		                     GTK_SOURCE_TYPE_INDENTER,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceView:indent-width:
	 *
	 * Width of an indentation step expressed in number of spaces.
	 */
	properties [PROP_INDENT_WIDTH] =
		g_param_spec_int ("indent-width",
		                  "Indent Width",
		                  "Number of spaces to use for each step of indent",
		                  -1,
		                  MAX_INDENT_WIDTH,
		                  -1,
		                  (G_PARAM_READWRITE |
		                   G_PARAM_EXPLICIT_NOTIFY |
		                   G_PARAM_STATIC_STRINGS));

	properties [PROP_AUTO_INDENT] =
		g_param_spec_boolean ("auto-indent",
		                      "Auto Indentation",
		                      "Whether to enable auto indentation",
		                      FALSE,
		                      (G_PARAM_READWRITE |
		                       G_PARAM_EXPLICIT_NOTIFY |
		                       G_PARAM_STATIC_STRINGS));

	properties [PROP_INSERT_SPACES] =
		g_param_spec_boolean ("insert-spaces-instead-of-tabs",
		                      "Insert Spaces Instead of Tabs",
		                      "Whether to insert spaces instead of tabs",
		                      FALSE,
		                      (G_PARAM_READWRITE |
		                       G_PARAM_EXPLICIT_NOTIFY |
		                       G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceView:show-right-margin:
	 *
	 * Whether to display the right margin.
	 */
	properties [PROP_SHOW_RIGHT_MARGIN] =
		g_param_spec_boolean ("show-right-margin",
		                      "Show Right Margin",
		                      "Whether to display the right margin",
		                      FALSE,
		                      (G_PARAM_READWRITE |
		                       G_PARAM_EXPLICIT_NOTIFY |
		                       G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceView:right-margin-position:
	 *
	 * Position of the right margin.
	 */
	properties [PROP_RIGHT_MARGIN_POSITION] =
		g_param_spec_uint ("right-margin-position",
		                   "Right Margin Position",
		                   "Position of the right margin",
		                   1,
		                   MAX_RIGHT_MARGIN_POSITION,
		                   DEFAULT_RIGHT_MARGIN_POSITION,
		                   (G_PARAM_READWRITE |
		                    G_PARAM_EXPLICIT_NOTIFY |
		                    G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceView:smart-home-end:
	 *
	 * Set the behavior of the HOME and END keys.
	 */
	properties [PROP_SMART_HOME_END] =
		g_param_spec_enum ("smart-home-end",
		                   "Smart Home/End",
		                   "HOME and END keys move to first/last "
		                   "non whitespace characters on line before going "
		                   "to the start/end of the line",
		                   GTK_SOURCE_TYPE_SMART_HOME_END_TYPE,
		                   GTK_SOURCE_SMART_HOME_END_DISABLED,
		                   (G_PARAM_READWRITE |
		                    G_PARAM_EXPLICIT_NOTIFY |
		                    G_PARAM_STATIC_STRINGS));

	properties [PROP_HIGHLIGHT_CURRENT_LINE] =
		g_param_spec_boolean ("highlight-current-line",
		                      "Highlight current line",
		                      "Whether to highlight the current line",
		                      FALSE,
		                      (G_PARAM_READWRITE |
		                       G_PARAM_EXPLICIT_NOTIFY |
		                       G_PARAM_STATIC_STRINGS));

	properties [PROP_INDENT_ON_TAB] =
		g_param_spec_boolean ("indent-on-tab",
		                      "Indent on tab",
		                      "Whether to indent the selected text when the tab key is pressed",
		                      TRUE,
		                      (G_PARAM_READWRITE |
		                       G_PARAM_EXPLICIT_NOTIFY |
		                       G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceView:background-pattern:
	 *
	 * Draw a specific background pattern on the view.
	 */
	properties [PROP_BACKGROUND_PATTERN] =
		g_param_spec_enum ("background-pattern",
		                   "Background pattern",
		                   "Draw a specific background pattern on the view",
		                   GTK_SOURCE_TYPE_BACKGROUND_PATTERN_TYPE,
		                   GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE,
		                   (G_PARAM_READWRITE |
		                    G_PARAM_EXPLICIT_NOTIFY |
		                    G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceView:smart-backspace:
	 *
	 * Whether smart Backspace should be used.
	 */
	properties [PROP_SMART_BACKSPACE] =
		g_param_spec_boolean ("smart-backspace",
		                      "Smart Backspace",
		                      "",
		                      FALSE,
		                      (G_PARAM_READWRITE |
		                       G_PARAM_EXPLICIT_NOTIFY |
		                       G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceView:space-drawer:
	 *
	 * The [class@SpaceDrawer] object associated with the view.
	 */
	properties [PROP_SPACE_DRAWER] =
		g_param_spec_object ("space-drawer",
		                     "Space Drawer",
		                     "",
		                     GTK_SOURCE_TYPE_SPACE_DRAWER,
		                     (G_PARAM_READABLE |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceView:annotations:
	 *
	 * The [class@Annotations] object associated with the view.
	 *
	 * Since: 5.18
	 */
	properties [PROP_ANNOTATIONS] =
		g_param_spec_object ("annotations", NULL, NULL,
		                     GTK_SOURCE_TYPE_ANNOTATIONS,
		                     (G_PARAM_READABLE |
		                      G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);

	/**
	 * GtkSourceView::show-completion:
	 * @view: The #GtkSourceView who emits the signal
	 *
	 * The signal is a key binding signal which gets
	 * emitted when the user requests a completion, by pressing
	 * <keycombo><keycap>Control</keycap><keycap>space</keycap></keycombo>.
	 *
	 * This will create a [class@CompletionContext] with the activation
	 * type as %GTK_SOURCE_COMPLETION_ACTIVATION_USER_REQUESTED.
	 *
	 * Applications should not connect to it, but may emit it with
	 * [func@GObject.signal_emit_by_name] if they need to activate the completion by
	 * another means, for example with another key binding or a menu entry.
	 */
	signals[SHOW_COMPLETION] =
		g_signal_new ("show-completion",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceViewClass, show_completion),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	g_signal_set_va_marshaller (signals[SHOW_COMPLETION],
	                            G_TYPE_FROM_CLASS (klass),
	                            g_cclosure_marshal_VOID__VOIDv);

	/**
	 * GtkSourceView::line-mark-activated:
	 * @view: the #GtkSourceView
	 * @iter: a #GtkTextIter
	 * @button: the button that was pressed
	 * @state: the modifier state, if any
	 * @n_presses: the number of presses
	 *
	 * Emitted when a line mark has been activated (for instance when there
	 * was a button press in the line marks gutter).
	 *
	 * You can use @iter to determine on which line the activation took place.
	 */
	signals[LINE_MARK_ACTIVATED] =
		g_signal_new ("line-mark-activated",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (GtkSourceViewClass, line_mark_activated),
		              NULL, NULL,
		              _gtk_source_marshal_VOID__BOXED_UINT_FLAGS_INT,
		              G_TYPE_NONE,
		              4,
		              GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
		              G_TYPE_UINT,
		              GDK_TYPE_MODIFIER_TYPE,
		              G_TYPE_INT);
	g_signal_set_va_marshaller (signals[LINE_MARK_ACTIVATED],
	                            G_TYPE_FROM_CLASS (klass),
	                            _gtk_source_marshal_VOID__BOXED_UINT_FLAGS_INTv);

	/**
	 * GtkSourceView::move-lines:
	 * @view: the #GtkSourceView which received the signal.
	 * @down: %TRUE to move down, %FALSE to move up.
	 *
	 * The signal is a keybinding which gets emitted when the user initiates moving a line.
	 *
	 * The default binding key is Alt+Up/Down arrow. And moves the currently selected lines,
	 * or the current line up or down by one line.
	 */
	signals[MOVE_LINES] =
		g_signal_new ("move-lines",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceViewClass, move_lines),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1,
			      G_TYPE_BOOLEAN);
	g_signal_set_va_marshaller (signals[MOVE_LINES],
	                            G_TYPE_FROM_CLASS (klass),
	                            g_cclosure_marshal_VOID__BOOLEANv);

	/**
	 * GtkSourceView::move-words:
	 * @view: the #GtkSourceView which received the signal
	 * @count: the number of words to move over
	 *
	 * The signal is a keybinding which gets emitted when the user initiates moving a word.
	 *
	 * The default binding key is Alt+Left/Right Arrow and moves the current selection, or the current
	 * word by one word.
	 */
	signals[MOVE_WORDS] =
		g_signal_new ("move-words",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceViewClass, move_words),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);
	g_signal_set_va_marshaller (signals[MOVE_WORDS],
	                            G_TYPE_FROM_CLASS (klass),
	                            g_cclosure_marshal_VOID__INTv);

	/**
	 * GtkSourceView::push-snippet:
	 * @view: a #GtkSourceView
	 * @snippet: a #GtkSourceSnippet
	 * @location: (inout): a #GtkTextIter
	 *
	 * The signal is emitted to insert a new snippet into the view.
	 *
	 * If another snippet was active, it will be paused until all focus positions of @snippet have been exhausted.
	 *
	 * @location will be updated to point at the end of the snippet.
	 */
	signals[PUSH_SNIPPET] =
		g_signal_new ("push-snippet",
	                      G_TYPE_FROM_CLASS (klass),
	                      G_SIGNAL_RUN_LAST,
	                      G_STRUCT_OFFSET (GtkSourceViewClass, push_snippet),
	                      NULL, NULL,
	                      _gtk_source_marshal_VOID__OBJECT_BOXED,
	                      G_TYPE_NONE,
	                      2,
	                      GTK_SOURCE_TYPE_SNIPPET,
	                      GTK_TYPE_TEXT_ITER);
	g_signal_set_va_marshaller (signals[PUSH_SNIPPET],
	                            G_TYPE_FROM_CLASS (klass),
	                            _gtk_source_marshal_VOID__OBJECT_BOXEDv);

	/**
	 * GtkSourceView::smart-home-end:
	 * @view: the #GtkSourceView
	 * @iter: a #GtkTextIter
	 * @count: the count
	 *
	 * Emitted when a the cursor was moved according to the smart home end setting.
	 *
	 * The signal is emitted after the cursor is moved, but
	 * during the [signal@Gtk.TextView::move-cursor] action. This can be used to find
	 * out whether the cursor was moved by a normal home/end or by a smart
	 * home/end.
	 */
	signals[SMART_HOME_END] =
		g_signal_new ("smart-home-end",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              _gtk_source_marshal_VOID__BOXED_INT,
		              G_TYPE_NONE,
		              2,
		              GTK_TYPE_TEXT_ITER,
		              G_TYPE_INT);
	g_signal_set_va_marshaller (signals[SMART_HOME_END],
	                            G_TYPE_FROM_CLASS (klass),
	                            _gtk_source_marshal_VOID__BOXED_INTv);

	/**
	 * GtkSourceView::move-to-matching-bracket:
	 * @view: the #GtkSourceView
	 * @extend_selection: %TRUE if the move should extend the selection
	 *
	 * Keybinding signal to move the cursor to the matching bracket.
	 */
	signals[MOVE_TO_MATCHING_BRACKET] =
		/* we have to do it this way since we do not have any more vfunc slots */
		g_signal_new_class_handler ("move-to-matching-bracket",
		                            G_TYPE_FROM_CLASS (klass),
		                            G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		                            G_CALLBACK (gtk_source_view_move_to_matching_bracket),
		                            NULL, NULL,
		                            g_cclosure_marshal_VOID__BOOLEAN,
		                            G_TYPE_NONE,
		                            1,
		                            G_TYPE_BOOLEAN);
	g_signal_set_va_marshaller (signals[MOVE_TO_MATCHING_BRACKET],
	                            G_TYPE_FROM_CLASS (klass),
	                            g_cclosure_marshal_VOID__BOOLEANv);

	/**
	 * GtkSourceView::change-number:
	 * @view: the #GtkSourceView
	 * @count: the number to add to the number at the current position
	 *
	 * Keybinding signal to edit a number at the current cursor position.
	 */
	signals[CHANGE_NUMBER] =
		g_signal_new_class_handler ("change-number",
		                            G_TYPE_FROM_CLASS (klass),
		                            G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		                            G_CALLBACK (gtk_source_view_change_number),
		                            NULL, NULL,
		                            g_cclosure_marshal_VOID__INT,
		                            G_TYPE_NONE,
		                            1,
		                            G_TYPE_INT);
	g_signal_set_va_marshaller (signals[CHANGE_NUMBER],
	                            G_TYPE_FROM_CLASS (klass),
	                            g_cclosure_marshal_VOID__INTv);

	/**
	 * GtkSourceView::change-case:
	 * @view: the #GtkSourceView
	 * @case_type: the case to use
	 *
	 * Keybinding signal to change case of the text at the current cursor position.
	 */
	signals[CHANGE_CASE] =
		g_signal_new_class_handler ("change-case",
		                            G_TYPE_FROM_CLASS (klass),
		                            G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		                            G_CALLBACK (gtk_source_view_change_case),
		                            NULL, NULL,
		                            g_cclosure_marshal_VOID__ENUM,
		                            G_TYPE_NONE,
		                            1,
		                            GTK_SOURCE_TYPE_CHANGE_CASE_TYPE);
	g_signal_set_va_marshaller (signals[CHANGE_CASE],
	                            G_TYPE_FROM_CLASS (klass),
	                            g_cclosure_marshal_VOID__ENUMv);

	/**
	 * GtkSourceView::join-lines:
	 * @view: the #GtkSourceView
	 *
	 * Keybinding signal to join the lines currently selected.
	 */
	signals[JOIN_LINES] =
		g_signal_new_class_handler ("join-lines",
		                            G_TYPE_FROM_CLASS (klass),
		                            G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		                            G_CALLBACK (gtk_source_view_join_lines),
		                            NULL, NULL,
		                            g_cclosure_marshal_VOID__VOID,
		                            G_TYPE_NONE,
		                            0);
	g_signal_set_va_marshaller (signals[JOIN_LINES],
	                            G_TYPE_FROM_CLASS (klass),
	                            g_cclosure_marshal_VOID__VOIDv);

	gtk_widget_class_install_action (widget_class, "source.change-case", "s",
	                                 gtk_source_view_activate_change_case);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_space,
	                                     GDK_CONTROL_MASK,
	                                     "show-completion",
	                                     NULL);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_Up,
	                                     GDK_ALT_MASK,
	                                     "move-lines",
	                                     "(b)",
	                                     FALSE);
	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_KP_Up,
	                                     GDK_ALT_MASK,
	                                     "move-lines",
	                                     "(b)",
	                                     FALSE);
	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_Down,
	                                     GDK_ALT_MASK,
	                                     "move-lines",
	                                     "(b)",
	                                     TRUE);
	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_KP_Down,
	                                     GDK_ALT_MASK,
	                                     "move-lines",
	                                     "(b)",
	                                     TRUE);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_Left,
	                                     GDK_ALT_MASK,
	                                     "move-words",
	                                     "(i)",
	                                     -1);
	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_KP_Left,
	                                     GDK_ALT_MASK,
	                                     "move-words",
	                                     "(i)",
	                                     -1);
	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_Right,
	                                     GDK_ALT_MASK,
	                                     "move-words",
	                                     "(i)",
	                                     1);
	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_KP_Right,
	                                     GDK_ALT_MASK,
	                                     "move-words",
	                                     "(i)",
	                                     1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_Up,
	                                     GDK_ALT_MASK | GDK_SHIFT_MASK,
	                                     "move-viewport",
	                                     "(ii)",
	                                     GTK_SCROLL_STEPS,
	                                     -1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_KP_Up,
	                                     GDK_ALT_MASK | GDK_SHIFT_MASK,
	                                     "move-viewport",
	                                     "(ii)",
	                                     GTK_SCROLL_STEPS,
	                                     -1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_Down,
	                                     GDK_ALT_MASK | GDK_SHIFT_MASK,
	                                     "move-viewport",
	                                     "(ii)",
	                                     GTK_SCROLL_STEPS,
	                                     1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_KP_Down,
	                                     GDK_ALT_MASK | GDK_SHIFT_MASK,
	                                     "move-viewport",
	                                     "(ii)",
	                                     GTK_SCROLL_STEPS,
	                                     1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_Page_Up,
	                                     GDK_ALT_MASK | GDK_SHIFT_MASK,
	                                     "move-viewport",
	                                     "(ii)",
	                                     GTK_SCROLL_PAGES,
	                                     -1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_KP_Page_Up,
	                                     GDK_ALT_MASK | GDK_SHIFT_MASK,
	                                     "move-viewport",
	                                     "(ii)",
	                                     GTK_SCROLL_PAGES,
	                                     -1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_Page_Down,
	                                     GDK_ALT_MASK | GDK_SHIFT_MASK,
	                                     "move-viewport",
	                                     "(ii)",
	                                     GTK_SCROLL_PAGES,
	                                     1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_KP_Page_Down,
	                                     GDK_ALT_MASK | GDK_SHIFT_MASK,
	                                     "move-viewport",
	                                     "(ii)",
	                                     GTK_SCROLL_PAGES,
	                                     1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_Home,
	                                     GDK_ALT_MASK | GDK_SHIFT_MASK,
	                                     "move-viewport",
	                                     "(ii)",
	                                     GTK_SCROLL_ENDS,
	                                     -1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_KP_Home,
	                                     GDK_ALT_MASK | GDK_SHIFT_MASK,
	                                     "move-viewport",
	                                     "(ii)",
	                                     GTK_SCROLL_ENDS,
	                                     -1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_End,
	                                     GDK_ALT_MASK | GDK_SHIFT_MASK,
	                                     "move-viewport",
	                                     "(ii)",
	                                     GTK_SCROLL_ENDS,
	                                     1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_KP_End,
	                                     GDK_ALT_MASK | GDK_SHIFT_MASK,
	                                     "move-viewport",
	                                     "(ii)",
	                                     GTK_SCROLL_ENDS,
	                                     1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_percent,
	                                     GDK_CONTROL_MASK,
	                                     "move-to-matching-bracket",
	                                     "(b)",
	                                     FALSE);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_a,
	                                     GDK_CONTROL_MASK | GDK_SHIFT_MASK,
	                                     "change-number",
	                                     "(i)",
	                                     1);

	gtk_widget_class_add_binding_signal (widget_class,
	                                     GDK_KEY_x,
	                                     GDK_CONTROL_MASK | GDK_SHIFT_MASK,
	                                     "change-number",
	                                     "(i)",
	                                     -1);
}

static GObject *
gtk_source_view_buildable_get_internal_child (GtkBuildable *buildable,
					      GtkBuilder   *builder,
					      const gchar  *childname)
{
	GtkSourceView *view = GTK_SOURCE_VIEW (buildable);

	if (g_strcmp0 (childname, "completion") == 0)
	{
		return G_OBJECT (gtk_source_view_get_completion (view));
	}

	return NULL;
}

static void
gtk_source_view_buildable_interface_init (GtkBuildableIface *iface)
{
	iface->get_internal_child = gtk_source_view_buildable_get_internal_child;
}

static void
gtk_source_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
	GtkSourceView *view;

	g_return_if_fail (GTK_SOURCE_IS_VIEW (object));

	view = GTK_SOURCE_VIEW (object);

	switch (prop_id)
	{
		case PROP_SHOW_LINE_NUMBERS:
			gtk_source_view_set_show_line_numbers (view, g_value_get_boolean (value));
			break;

		case PROP_SHOW_LINE_MARKS:
			gtk_source_view_set_show_line_marks (view, g_value_get_boolean (value));
			break;

		case PROP_TAB_WIDTH:
			gtk_source_view_set_tab_width (view, g_value_get_uint (value));
			break;

		case PROP_INDENTER:
			gtk_source_view_set_indenter (view, g_value_get_object (value));
			break;

		case PROP_INDENT_WIDTH:
			gtk_source_view_set_indent_width (view, g_value_get_int (value));
			break;

		case PROP_AUTO_INDENT:
			gtk_source_view_set_auto_indent (view, g_value_get_boolean (value));
			break;

		case PROP_INSERT_SPACES:
			gtk_source_view_set_insert_spaces_instead_of_tabs (view, g_value_get_boolean (value));
			break;

		case PROP_SHOW_RIGHT_MARGIN:
			gtk_source_view_set_show_right_margin (view, g_value_get_boolean (value));
			break;

		case PROP_RIGHT_MARGIN_POSITION:
			gtk_source_view_set_right_margin_position (view, g_value_get_uint (value));
			break;

		case PROP_SMART_HOME_END:
			gtk_source_view_set_smart_home_end (view, g_value_get_enum (value));
			break;

		case PROP_HIGHLIGHT_CURRENT_LINE:
			gtk_source_view_set_highlight_current_line (view, g_value_get_boolean (value));
			break;

		case PROP_INDENT_ON_TAB:
			gtk_source_view_set_indent_on_tab (view, g_value_get_boolean (value));
			break;

		case PROP_BACKGROUND_PATTERN:
			gtk_source_view_set_background_pattern (view, g_value_get_enum (value));
			break;

		case PROP_SMART_BACKSPACE:
			gtk_source_view_set_smart_backspace (view, g_value_get_boolean (value));
			break;

		case PROP_ENABLE_SNIPPETS:
			gtk_source_view_set_enable_snippets (view, g_value_get_boolean (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
	GtkSourceView *view;

	g_return_if_fail (GTK_SOURCE_IS_VIEW (object));

	view = GTK_SOURCE_VIEW (object);

	switch (prop_id)
	{
		case PROP_COMPLETION:
			g_value_set_object (value, gtk_source_view_get_completion (view));
			break;

		case PROP_SHOW_LINE_NUMBERS:
			g_value_set_boolean (value, gtk_source_view_get_show_line_numbers (view));
			break;

		case PROP_SHOW_LINE_MARKS:
			g_value_set_boolean (value, gtk_source_view_get_show_line_marks (view));
			break;

		case PROP_TAB_WIDTH:
			g_value_set_uint (value, gtk_source_view_get_tab_width (view));
			break;

		case PROP_INDENTER:
			g_value_set_object (value, gtk_source_view_get_indenter (view));
			break;

		case PROP_INDENT_WIDTH:
			g_value_set_int (value, gtk_source_view_get_indent_width (view));
			break;

		case PROP_AUTO_INDENT:
			g_value_set_boolean (value, gtk_source_view_get_auto_indent (view));
			break;

		case PROP_INSERT_SPACES:
			g_value_set_boolean (value, gtk_source_view_get_insert_spaces_instead_of_tabs (view));
			break;

		case PROP_SHOW_RIGHT_MARGIN:
			g_value_set_boolean (value, gtk_source_view_get_show_right_margin (view));
			break;

		case PROP_RIGHT_MARGIN_POSITION:
			g_value_set_uint (value, gtk_source_view_get_right_margin_position (view));
			break;

		case PROP_SMART_HOME_END:
			g_value_set_enum (value, gtk_source_view_get_smart_home_end (view));
			break;

		case PROP_HIGHLIGHT_CURRENT_LINE:
			g_value_set_boolean (value, gtk_source_view_get_highlight_current_line (view));
			break;

		case PROP_INDENT_ON_TAB:
			g_value_set_boolean (value, gtk_source_view_get_indent_on_tab (view));
			break;

		case PROP_BACKGROUND_PATTERN:
			g_value_set_enum (value, gtk_source_view_get_background_pattern (view));
			break;

		case PROP_SMART_BACKSPACE:
			g_value_set_boolean (value, gtk_source_view_get_smart_backspace (view));
			break;

		case PROP_SPACE_DRAWER:
			g_value_set_object (value, gtk_source_view_get_space_drawer (view));
			break;

		case PROP_ANNOTATIONS:
			g_value_set_object (value, gtk_source_view_get_annotations (view));
			break;

		case PROP_ENABLE_SNIPPETS:
			g_value_set_boolean (value, gtk_source_view_get_enable_snippets (view));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
space_drawer_notify_cb (GtkSourceSpaceDrawer *space_drawer,
                        GParamSpec           *pspec,
                        GtkSourceView        *view)
{
	gtk_source_view_queue_draw (view);
}

static void
annotations_changed_cb (GtkSourceAnnotations *annotations,
                        GtkSourceView        *view)
{
	g_assert (GTK_SOURCE_IS_ANNOTATIONS (annotations));
	g_assert (GTK_SOURCE_IS_VIEW (view));

	gtk_source_view_queue_draw (view);
}

static void
notify_buffer_cb (GtkSourceView *view)
{
	set_source_buffer (view, gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
}

static void
gtk_source_view_init (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkEventController *key;
	GtkEventController *click;
	GtkEventController *focus;
	GtkDropTarget *dest;

	gtk_widget_add_css_class (GTK_WIDGET (view), "GtkSourceView");

	priv->tab_width = DEFAULT_TAB_WIDTH;
	priv->tabs_set = FALSE;
	priv->indent_width = -1;
	priv->indent_on_tab = TRUE;
	priv->smart_home_end = GTK_SOURCE_SMART_HOME_END_DISABLED;
	priv->right_margin_pos = DEFAULT_RIGHT_MARGIN_POSITION;
	priv->cached_right_margin_pos = -1;
	priv->indenter = _gtk_source_indenter_internal_new ();

	gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 2);
	gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 2);

	priv->right_margin_line_color_set = FALSE;
	priv->right_margin_overlay_color_set = FALSE;

	priv->space_drawer = gtk_source_space_drawer_new ();
	g_signal_connect_object (priv->space_drawer,
				 "notify",
				 G_CALLBACK (space_drawer_notify_cb),
				 view,
				 0);

	priv->annotations = g_object_new (GTK_SOURCE_TYPE_ANNOTATIONS, NULL);
	g_signal_connect_object (priv->annotations,
				 "changed",
				 G_CALLBACK (annotations_changed_cb),
				 view,
				 0);

	priv->mark_categories = g_hash_table_new_full (g_str_hash,
	                                               g_str_equal,
	                                               (GDestroyNotify) g_free,
	                                               (GDestroyNotify) mark_category_free);

	key = gtk_event_controller_key_new ();
	gtk_event_controller_set_propagation_phase (key, GTK_PHASE_CAPTURE);
	g_signal_connect_swapped (key,
	                          "key-pressed",
	                          G_CALLBACK (gtk_source_view_key_pressed),
	                          view);
	g_signal_connect_swapped (key,
	                          "key-released",
	                          G_CALLBACK (gtk_source_view_key_released),
	                          view);
	gtk_widget_add_controller (GTK_WIDGET (view), g_steal_pointer (&key));

	focus = gtk_event_controller_focus_new ();
	g_signal_connect_swapped (focus,
	                          "enter",
	                          G_CALLBACK (gtk_source_view_focus_changed),
	                          view);
	g_signal_connect_swapped (focus,
	                          "leave",
	                          G_CALLBACK (gtk_source_view_focus_changed),
	                          view);
	gtk_widget_add_controller (GTK_WIDGET (view), g_steal_pointer (&focus));

	click = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), 0);
	gtk_event_controller_set_propagation_phase (click, GTK_PHASE_BUBBLE);
	g_signal_connect_swapped (click,
	                          "pressed",
	                          G_CALLBACK (gtk_source_view_clicked),
	                          view);
	gtk_widget_add_controller (GTK_WIDGET (view), g_steal_pointer (&click));

	dest = gtk_drop_target_new (GDK_TYPE_RGBA, GDK_ACTION_COPY);
	gtk_drop_target_set_preload (dest, TRUE);
	g_signal_connect (dest, "drop", G_CALLBACK (gtk_source_view_rgba_drop), view);
	gtk_widget_add_controller (GTK_WIDGET (view), GTK_EVENT_CONTROLLER (dest));

	gtk_widget_set_has_tooltip (GTK_WIDGET (view), TRUE);

	g_signal_connect (view,
			  "notify::buffer",
			  G_CALLBACK (notify_buffer_cb),
			  NULL);

	gtk_widget_add_css_class (GTK_WIDGET (view), "sourceview");

	gtk_source_view_populate_extra_menu (view);

	_gtk_source_view_assistants_init (&priv->assistants, view);
}

static void
gtk_source_view_dispose (GObject *object)
{
	GtkSourceView *view = GTK_SOURCE_VIEW (object);
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	priv->im_commit_text[0] = 0;
	priv->im_commit_len = 0;

	if (priv->completion != NULL)
	{
		g_object_run_dispose (G_OBJECT (priv->completion));
		g_clear_object (&priv->completion);
	}

	if (priv->hover != NULL)
	{
		g_object_run_dispose (G_OBJECT (priv->hover));
		g_clear_object (&priv->hover);
	}

	g_clear_object (&priv->indenter);
	g_clear_object (&priv->style_scheme);
	g_clear_object (&priv->space_drawer);
	g_clear_object (&priv->annotations);

	remove_source_buffer (view);

	/* Release our snippet state. This is safe to call multiple times. */
	_gtk_source_view_snippets_shutdown (&priv->snippets);

	/* Disconnect notify buffer because the destroy of the textview will set
	 * the buffer to NULL, and we call get_buffer in the notify which would
	 * reinstate a buffer which we don't want.
	 * There is no problem calling g_signal_handlers_disconnect_by_func()
	 * several times (if dispose() is called several times).
	 */
	g_signal_handlers_disconnect_by_func (view, notify_buffer_cb, NULL);

	_gtk_source_view_assistants_shutdown (&priv->assistants);

	G_OBJECT_CLASS (gtk_source_view_parent_class)->dispose (object);
}

static void
gtk_source_view_finalize (GObject *object)
{
	GtkSourceView *view = GTK_SOURCE_VIEW (object);
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	if (priv->mark_categories)
	{
		g_hash_table_destroy (priv->mark_categories);
	}

	G_OBJECT_CLASS (gtk_source_view_parent_class)->finalize (object);
}

static void
get_visible_region (GtkTextView *text_view,
                    GtkTextIter *start,
                    GtkTextIter *end)
{
	GdkRectangle visible_rect;

	gtk_text_view_get_visible_rect (text_view, &visible_rect);

	gtk_text_view_get_line_at_y (text_view,
				     start,
				     visible_rect.y,
				     NULL);

	gtk_text_view_get_line_at_y (text_view,
				     end,
				     visible_rect.y + visible_rect.height,
				     NULL);

	gtk_text_iter_backward_line (start);
	gtk_text_iter_forward_line (end);
}

static void
highlight_updated_cb (GtkSourceBuffer *buffer,
                      GtkTextIter     *_start,
                      GtkTextIter     *_end,
                      GtkTextView     *text_view)
{
	GtkTextIter start;
	GtkTextIter end;
	GtkTextIter visible_start;
	GtkTextIter visible_end;
	GtkTextIter intersect_start;
	GtkTextIter intersect_end;

#if 0
	{
		static gint nth_call = 0;

		g_message ("%s(view=%p) %d [%d-%d]",
			   G_STRFUNC,
			   text_view,
			   ++nth_call,
			   gtk_text_iter_get_offset (_start),
			   gtk_text_iter_get_offset (_end));
	}
#endif

	start = *_start;
	end = *_end;
	gtk_text_iter_order (&start, &end);

	get_visible_region (text_view, &visible_start, &visible_end);

	if (gtk_text_iter_compare (&end, &visible_start) < 0 ||
	    gtk_text_iter_compare (&visible_end, &start) < 0)
	{
		return;
	}

	if (gtk_text_iter_compare (&start, &visible_start) < 0)
	{
		intersect_start = visible_start;
	}
	else
	{
		intersect_start = start;
	}

	if (gtk_text_iter_compare (&visible_end, &end) < 0)
	{
		intersect_end = visible_end;
	}
	else
	{
		intersect_end = end;
	}

	/* GtkSourceContextEngine sends the highlight-updated signal to notify
	 * the view, and in the view (here) we tell the ContextEngine to update
	 * the highlighting, but only in the visible area. It seems that the
	 * purpose is to reduce the number of tags that the ContextEngine
	 * applies to the buffer.
	 *
	 * A previous implementation of this signal handler queued a redraw on
	 * the view with gtk_widget_queue_draw_area(), instead of calling
	 * directly _gtk_source_buffer_update_syntax_highlight(). The ::draw
	 * handler also calls _gtk_source_buffer_update_syntax_highlight(), so
	 * this had the desired effect, but it was less clear.
	 * See the Git commit 949cd128064201935f90d999544e6a19f8e3baa6.
	 * And: https://bugzilla.gnome.org/show_bug.cgi?id=767565
	 */
	_gtk_source_buffer_update_syntax_highlight (buffer,
						    &intersect_start,
						    &intersect_end,
						    FALSE);
}

static void
search_start_cb (GtkSourceBufferInternal *buffer_internal,
                 GtkSourceSearchContext  *search_context,
                 GtkSourceView           *view)
{
	GtkTextIter visible_start;
	GtkTextIter visible_end;

	get_visible_region (GTK_TEXT_VIEW (view), &visible_start, &visible_end);

#ifndef G_DISABLE_ASSERT
	{
		GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
		GtkSourceBuffer *buffer_search = gtk_source_search_context_get_buffer (search_context);
		g_assert (buffer_search == priv->source_buffer);
	}
#endif

	_gtk_source_search_context_update_highlight (search_context,
						     &visible_start,
						     &visible_end,
						     FALSE);
}

static void
source_mark_updated_cb (GtkSourceBuffer *buffer,
                        GtkSourceMark   *mark,
                        GtkTextView     *text_view)
{
	/* TODO do something more intelligent here, namely
	 * invalidate only the area under the mark if possible */
	gtk_source_view_queue_draw (GTK_SOURCE_VIEW (text_view));
}

static void
buffer_style_scheme_changed_cb (GtkSourceBuffer *buffer,
                                GParamSpec      *pspec,
                                GtkSourceView   *view)
{
	gtk_source_view_update_style_scheme (view);
}

static void
buffer_has_selection_changed_cb (GtkSourceBuffer *buffer,
				 GParamSpec      *pspec,
				 GtkSourceView   *view)
{
	gtk_widget_action_set_enabled (GTK_WIDGET (view),
				       "source.change-case",
				       (gtk_text_view_get_editable (GTK_TEXT_VIEW (view)) &&
					gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (buffer))));
}

static void
buffer_insert_text_cb (GtkTextBuffer *buffer,
                       GtkTextIter   *iter,
                       const char    *text,
                       int            len,
                       GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_assert (GTK_IS_TEXT_BUFFER (buffer));
	g_assert (iter != NULL);
	g_assert (text != NULL);
	g_assert (GTK_SOURCE_IS_VIEW (view));

	if (len < 0)
	{
		len = _gtk_source_utils_strnlen (text, G_N_ELEMENTS (priv->im_commit_text));
	}

	if (len < G_N_ELEMENTS (priv->im_commit_text))
	{
		memcpy (priv->im_commit_text, text, len);
		priv->im_commit_text[len] = 0;
		priv->im_commit_len = len;
	}
	else
	{
		priv->im_commit_text[0] = 0;
		priv->im_commit_len = 0;
	}
}

static void
implicit_trailing_newline_changed_cb (GtkSourceBuffer *buffer,
                                      GParamSpec      *pspec,
                                      GtkSourceView   *view)
{
	/* For drawing or not a trailing newline. */
	gtk_source_view_queue_draw (view);
}

static void
remove_source_buffer (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	if (priv->source_buffer != NULL)
	{
		GtkSourceBufferInternal *buffer_internal;

		g_signal_handlers_disconnect_by_func (priv->source_buffer,
						      highlight_updated_cb,
						      view);

		g_signal_handlers_disconnect_by_func (priv->source_buffer,
						      source_mark_updated_cb,
						      view);

		g_signal_handlers_disconnect_by_func (priv->source_buffer,
						      buffer_style_scheme_changed_cb,
						      view);

		g_signal_handlers_disconnect_by_func (priv->source_buffer,
						      buffer_has_selection_changed_cb,
						      view);

		g_signal_handlers_disconnect_by_func (priv->source_buffer,
						      buffer_insert_text_cb,
						      view);

		g_signal_handlers_disconnect_by_func (priv->source_buffer,
						      implicit_trailing_newline_changed_cb,
						      view);

		buffer_internal = _gtk_source_buffer_internal_get_from_buffer (priv->source_buffer);

		g_signal_handlers_disconnect_by_func (buffer_internal,
						      search_start_cb,
						      view);

		_gtk_source_view_snippets_set_buffer (&priv->snippets, NULL);

		g_object_unref (priv->source_buffer);
		priv->source_buffer = NULL;
	}
}

static void
set_source_buffer (GtkSourceView *view,
                   GtkTextBuffer *buffer)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	if (buffer == (GtkTextBuffer*) priv->source_buffer)
	{
		return;
	}

	remove_source_buffer (view);

	if (GTK_SOURCE_IS_BUFFER (buffer))
	{
		GtkSourceBufferInternal *buffer_internal;

		priv->source_buffer = g_object_ref (GTK_SOURCE_BUFFER (buffer));

		g_signal_connect (buffer,
				  "highlight-updated",
				  G_CALLBACK (highlight_updated_cb),
				  view);

		g_signal_connect (buffer,
				  "source-mark-updated",
				  G_CALLBACK (source_mark_updated_cb),
				  view);

		g_signal_connect (buffer,
				  "notify::style-scheme",
				  G_CALLBACK (buffer_style_scheme_changed_cb),
				  view);

		g_signal_connect (buffer,
				  "notify::implicit-trailing-newline",
				  G_CALLBACK (implicit_trailing_newline_changed_cb),
				  view);

		g_signal_connect (buffer,
				  "notify::has-selection",
				  G_CALLBACK (buffer_has_selection_changed_cb),
				  view);

		g_signal_connect (buffer,
		                  "insert-text",
		                  G_CALLBACK (buffer_insert_text_cb),
		                  view);

		buffer_internal = _gtk_source_buffer_internal_get_from_buffer (priv->source_buffer);

		g_signal_connect (buffer_internal,
				  "search-start",
				  G_CALLBACK (search_start_cb),
				  view);

		buffer_has_selection_changed_cb (GTK_SOURCE_BUFFER (buffer), NULL, view);

		_gtk_source_view_snippets_set_buffer (&priv->snippets, priv->source_buffer);
	}

	gtk_source_view_update_style_scheme (view);
}

static void
gtk_source_view_show_completion_real (GtkSourceView *view)
{
	GtkSourceCompletion *completion = get_completion (view);
	gtk_source_completion_show (completion);
}

static void
gtk_source_view_populate_extra_menu (GtkSourceView *view)
{
	GMenuItem *item;
	GMenu *extra_menu;
	GMenu *section;

	extra_menu = g_menu_new ();

	/* create change case menu */
	section = g_menu_new ();

	item = g_menu_item_new (_("All _Upper Case"), "source.change-case('upper')");
	g_menu_append_item (section, item);
	g_object_unref (item);

	item = g_menu_item_new (_("All _Lower Case"), "source.change-case('lower')");
	g_menu_append_item (section, item);
	g_object_unref (item);

	item = g_menu_item_new (_("_Invert Case"), "source.change-case('toggle')");
	g_menu_append_item (section, item);
	g_object_unref (item);

	item = g_menu_item_new (_("_Title Case"), "source.change-case('title')");
	g_menu_append_item (section, item);
	g_object_unref (item);

	g_menu_append_submenu (extra_menu, _("C_hange Case"), G_MENU_MODEL (section));
	gtk_text_view_set_extra_menu (GTK_TEXT_VIEW (view), G_MENU_MODEL (extra_menu));
	g_object_unref (section);

	g_object_unref (extra_menu);
}

static void
move_cursor (GtkTextView       *text_view,
             const GtkTextIter *new_location,
             gboolean           extend_selection)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (text_view);
	GtkTextMark *insert = gtk_text_buffer_get_insert (buffer);

	if (extend_selection)
	{
		gtk_text_buffer_move_mark (buffer, insert, new_location);
	}
	else
	{
		gtk_text_buffer_place_cursor (buffer, new_location);
	}

	gtk_text_view_scroll_mark_onscreen (text_view, insert);
}

static void
move_to_first_char (GtkTextView *text_view,
                    GtkTextIter *iter,
                    gboolean     display_line)
{
	GtkTextIter last;

	last = *iter;

	if (display_line)
	{
		gtk_text_view_backward_display_line_start (text_view, iter);
		gtk_text_view_forward_display_line_end (text_view, &last);
	}
	else
	{
		gtk_text_iter_set_line_offset (iter, 0);

		if (!gtk_text_iter_ends_line (&last))
		{
			gtk_text_iter_forward_to_line_end (&last);
		}
	}


	while (gtk_text_iter_compare (iter, &last) < 0)
	{
		gunichar c;

		c = gtk_text_iter_get_char (iter);

		if (g_unichar_isspace (c))
		{
			if (!gtk_text_iter_forward_visible_cursor_position (iter))
			{
				break;
			}
		}
		else
		{
			break;
		}
	}
}

static void
move_to_last_char (GtkTextView *text_view,
                   GtkTextIter *iter,
                   gboolean     display_line)
{
	GtkTextIter first;

	first = *iter;

	if (display_line)
	{
		gtk_text_view_forward_display_line_end (text_view, iter);
		gtk_text_view_backward_display_line_start (text_view, &first);
	}
	else
	{
		if (!gtk_text_iter_ends_line (iter))
		{
			gtk_text_iter_forward_to_line_end (iter);
		}

		gtk_text_iter_set_line_offset (&first, 0);
	}

	while (gtk_text_iter_compare (iter, &first) > 0)
	{
		gunichar c;

		if (!gtk_text_iter_backward_visible_cursor_position (iter))
		{
			break;
		}

		c = gtk_text_iter_get_char (iter);

		if (!g_unichar_isspace (c))
		{
			/* We've gone one cursor position too far. */
			gtk_text_iter_forward_visible_cursor_position (iter);
			break;
		}
	}
}

static void
do_cursor_move_home_end (GtkTextView *text_view,
                         GtkTextIter *cur,
                         GtkTextIter *iter,
                         gboolean     extend_selection,
                         gint         count)
{
	/* if we are clearing selection, we need to move_cursor even
	 * if we are at proper iter because selection_bound may need
	 * to be moved */
	if (!gtk_text_iter_equal (cur, iter) || !extend_selection)
	{
		move_cursor (text_view, iter, extend_selection);
		g_signal_emit (text_view, signals[SMART_HOME_END], 0, iter, count);
	}
}

/* Returns %TRUE if handled. */
static gboolean
move_cursor_smart_home_end (GtkTextView     *text_view,
                            GtkMovementStep  step,
                            gint             count,
                            gboolean         extend_selection)
{
	GtkSourceView *source_view = GTK_SOURCE_VIEW (text_view);
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (source_view);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (text_view);
	gboolean move_display_line;
	GtkTextMark *mark;
	GtkTextIter cur;
	GtkTextIter iter;

	g_assert (step == GTK_MOVEMENT_DISPLAY_LINE_ENDS ||
		  step == GTK_MOVEMENT_PARAGRAPH_ENDS);

	move_display_line = step == GTK_MOVEMENT_DISPLAY_LINE_ENDS;

	mark = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, &cur, mark);
	iter = cur;

	if (count == -1)
	{
		gboolean at_home;

		move_to_first_char (text_view, &iter, move_display_line);

		if (move_display_line)
		{
			at_home = gtk_text_view_starts_display_line (text_view, &cur);
		}
		else
		{
			at_home = gtk_text_iter_starts_line (&cur);
		}

		switch (priv->smart_home_end)
		{
			case GTK_SOURCE_SMART_HOME_END_BEFORE:
				if (!gtk_text_iter_equal (&cur, &iter) || at_home)
				{
					do_cursor_move_home_end (text_view,
					                         &cur,
					                         &iter,
					                         extend_selection,
					                         count);
					return TRUE;
				}
				break;

			case GTK_SOURCE_SMART_HOME_END_AFTER:
				if (at_home)
				{
					do_cursor_move_home_end (text_view,
					                         &cur,
					                         &iter,
					                         extend_selection,
					                         count);
					return TRUE;
				}
				break;

			case GTK_SOURCE_SMART_HOME_END_ALWAYS:
				do_cursor_move_home_end (text_view,
				                         &cur,
				                         &iter,
				                         extend_selection,
				                         count);
				return TRUE;

			case GTK_SOURCE_SMART_HOME_END_DISABLED:
			default:
				break;
		}
	}
	else if (count == 1)
	{
		gboolean at_end;

		move_to_last_char (text_view, &iter, move_display_line);

		if (move_display_line)
		{
			GtkTextIter display_end;
			display_end = cur;

			gtk_text_view_forward_display_line_end (text_view, &display_end);
			at_end = gtk_text_iter_equal (&cur, &display_end);
		}
		else
		{
			at_end = gtk_text_iter_ends_line (&cur);
		}

		switch (priv->smart_home_end)
		{
			case GTK_SOURCE_SMART_HOME_END_BEFORE:
				if (!gtk_text_iter_equal (&cur, &iter) || at_end)
				{
					do_cursor_move_home_end (text_view,
					                         &cur,
					                         &iter,
					                         extend_selection,
					                         count);
					return TRUE;
				}
				break;

			case GTK_SOURCE_SMART_HOME_END_AFTER:
				if (at_end)
				{
					do_cursor_move_home_end (text_view,
					                         &cur,
					                         &iter,
					                         extend_selection,
					                         count);
					return TRUE;
				}
				break;

			case GTK_SOURCE_SMART_HOME_END_ALWAYS:
				do_cursor_move_home_end (text_view,
				                         &cur,
				                         &iter,
				                         extend_selection,
				                         count);
				return TRUE;

			case GTK_SOURCE_SMART_HOME_END_DISABLED:
			default:
				break;
		}
	}

	return FALSE;
}

static void
move_cursor_words (GtkTextView *text_view,
                   gint         count,
                   gboolean     extend_selection)
{
	GtkTextBuffer *buffer;
	GtkTextIter insert;
	GtkTextIter newplace;
	GtkTextIter line_start;
	GtkTextIter line_end;
	const gchar *ptr;
	gchar *line_text;

	buffer = gtk_text_view_get_buffer (text_view);

	gtk_text_buffer_get_iter_at_mark (buffer,
					  &insert,
					  gtk_text_buffer_get_insert (buffer));

	line_start = line_end = newplace = insert;

	/* Get the text of the current line for RTL analysis */
	gtk_text_iter_set_line_offset (&line_start, 0);
	gtk_text_iter_forward_line (&line_end);
	line_text = gtk_text_iter_get_visible_text (&line_start, &line_end);

	/* Swap direction for RTL to maintain visual cursor movement.
	 * Otherwise, cursor will move in opposite direction which is counter
	 * intuitve and causes confusion for RTL users.
	 *
	 * You would think we could iterate using the textiter, but we cannot
	 * since there is no way in GtkTextIter to check if it is visible (as
	 * that is not exposed by GtkTextBTree). So we use the allocated string
	 * contents instead.
	 */
	for (ptr = line_text; *ptr; ptr = g_utf8_next_char (ptr))
	{
		gunichar ch = g_utf8_get_char (ptr);
		FriBidiCharType ch_type = fribidi_get_bidi_type (ch);

		if (FRIBIDI_IS_STRONG (ch_type))
		{
			if (FRIBIDI_IS_RTL (ch_type))
			{
				count *= -1;
			}

			break;
		}
	}

	g_free (line_text);

	if (count < 0)
	{
		if (!_gtk_source_iter_backward_visible_word_starts (&newplace, -count))
		{
			gtk_text_iter_set_line_offset (&newplace, 0);
		}
	}
	else if (count > 0)
	{
		if (!_gtk_source_iter_forward_visible_word_ends (&newplace, count))
		{
			gtk_text_iter_forward_to_line_end (&newplace);
		}
	}

	move_cursor (text_view, &newplace, extend_selection);
}

static void
gtk_source_view_move_cursor (GtkTextView     *text_view,
                             GtkMovementStep  step,
                             gint             count,
                             gboolean         extend_selection)
{
	if (!gtk_text_view_get_cursor_visible (text_view))
	{
		goto chain_up;
	}

	gtk_text_view_reset_im_context (text_view);

	switch (step)
	{
		case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
		case GTK_MOVEMENT_PARAGRAPH_ENDS:
			if (move_cursor_smart_home_end (text_view, step, count, extend_selection))
			{
				return;
			}
			break;

		case GTK_MOVEMENT_WORDS:
			move_cursor_words (text_view, count, extend_selection);
			return;

		case GTK_MOVEMENT_LOGICAL_POSITIONS:
		case GTK_MOVEMENT_VISUAL_POSITIONS:
		case GTK_MOVEMENT_DISPLAY_LINES:
		case GTK_MOVEMENT_PARAGRAPHS:
		case GTK_MOVEMENT_PAGES:
		case GTK_MOVEMENT_BUFFER_ENDS:
		case GTK_MOVEMENT_HORIZONTAL_PAGES:
		default:
			break;
	}

chain_up:
	GTK_TEXT_VIEW_CLASS (gtk_source_view_parent_class)->move_cursor (text_view,
									 step,
									 count,
									 extend_selection);
}

static void
gtk_source_view_delete_from_cursor (GtkTextView   *text_view,
                                    GtkDeleteType  type,
                                    gint           count)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (text_view);
	GtkTextIter insert;
	GtkTextIter start;
	GtkTextIter end;

	if (type != GTK_DELETE_WORD_ENDS)
	{
		GTK_TEXT_VIEW_CLASS (gtk_source_view_parent_class)->delete_from_cursor (text_view,
											type,
											count);
		return;
	}

	gtk_text_view_reset_im_context (text_view);

	gtk_text_buffer_get_iter_at_mark (buffer,
					  &insert,
					  gtk_text_buffer_get_insert (buffer));

	start = insert;
	end = insert;

	if (count > 0)
	{
		if (!_gtk_source_iter_forward_visible_word_ends (&end, count))
		{
			gtk_text_iter_forward_to_line_end (&end);
		}
	}
	else
	{
		if (!_gtk_source_iter_backward_visible_word_starts (&start, -count))
		{
			gtk_text_iter_set_line_offset (&start, 0);
		}
	}

	gtk_text_buffer_delete_interactive (buffer, &start, &end,
					    gtk_text_view_get_editable (text_view));
}

static gboolean
gtk_source_view_extend_selection (GtkTextView            *text_view,
                                  GtkTextExtendSelection  granularity,
                                  const GtkTextIter      *location,
                                  GtkTextIter            *start,
                                  GtkTextIter            *end)
{
	if (granularity == GTK_TEXT_EXTEND_SELECTION_WORD)
	{
		_gtk_source_iter_extend_selection_word (location, start, end);
		return GDK_EVENT_STOP;
	}

	return GTK_TEXT_VIEW_CLASS (gtk_source_view_parent_class)->extend_selection (text_view,
										     granularity,
										     location,
										     start,
										     end);
}

static void
gtk_source_view_ensure_redrawn_rect_is_highlighted (GtkSourceView *view,
                                                    GdkRectangle  *clip)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkTextIter iter1, iter2;
	char *message = NULL;

	GTK_SOURCE_PROFILER_BEGIN_MARK;

	/* If there is nothing to update here in terms of highlighting, then we can
	 * avoid some expensive operations such as looking up iters by location.
	 * Inside of test-widget, this function can easily take .5msec according to
	 * profiling data.
	 */
	if (!gtk_source_buffer_get_highlight_syntax (priv->source_buffer) &&
	    !_gtk_source_buffer_has_search_highlights (priv->source_buffer))
	{
		return;
	}

	gtk_text_view_get_line_at_y (GTK_TEXT_VIEW (view), &iter1, clip->y, NULL);
	gtk_text_iter_backward_line (&iter1);
	gtk_text_view_get_line_at_y (GTK_TEXT_VIEW (view), &iter2, clip->y + clip->height, NULL);
	gtk_text_iter_forward_line (&iter2);

	_gtk_source_buffer_update_syntax_highlight (priv->source_buffer,
						    &iter1, &iter2, FALSE);
	_gtk_source_buffer_update_search_highlight (priv->source_buffer,
						    &iter1, &iter2, FALSE);

	if (GTK_SOURCE_PROFILER_ACTIVE)
		message = g_strdup_printf ("Area: Y=%d Height=%d BeginLine=%d EndLine=%d",
	                                 clip->y, clip->height,
	                                 gtk_text_iter_get_line (&iter1),
	                                 gtk_text_iter_get_line (&iter2));
	GTK_SOURCE_PROFILER_END_MARK ("GtkSourceView::IsHighlighted", message);
	g_free (message);
}

/* This function is taken from gtk+/tests/testtext.c */
static void
gtk_source_view_get_lines (GtkTextView *text_view,
                           gint         first_y,
                           gint         last_y,
                           GArray      *buffer_coords,
                           GArray      *line_heights,
                           GArray      *numbers,
                           gint        *countp)
{
	GtkTextIter iter;
	gint count;
	gint last_line_num = -1;

	g_array_set_size (buffer_coords, 0);
	g_array_set_size (numbers, 0);
	if (line_heights != NULL)
		g_array_set_size (line_heights, 0);

	/* Get iter at first y */
	gtk_text_view_get_line_at_y (text_view, &iter, first_y, NULL);

	/* For each iter, get its location and add it to the arrays.
	 * Stop when we pass last_y */
	count = 0;

	while (!gtk_text_iter_is_end (&iter))
	{
		gint y, height;

		gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

		g_array_append_val (buffer_coords, y);
		if (line_heights)
		{
			g_array_append_val (line_heights, height);
		}

		last_line_num = gtk_text_iter_get_line (&iter);
		g_array_append_val (numbers, last_line_num);

		++count;

		if ((y + height) >= last_y)
			break;

		gtk_text_iter_forward_line (&iter);
	}

	if (gtk_text_iter_is_end (&iter))
	{
		gint y, height;
		gint line_num;

		gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

		line_num = gtk_text_iter_get_line (&iter);

		if (line_num != last_line_num)
		{
			g_array_append_val (buffer_coords, y);
			if (line_heights)
				g_array_append_val (line_heights, height);
			g_array_append_val (numbers, line_num);
			++count;
		}
	}

	*countp = count;
}

/* Another solution to paint the line background is to use the
 * GtkTextTag:paragraph-background property. But there are several issues:
 * - GtkTextTags are per buffer, not per view. It's better to keep the line
 *   highlighting per view.
 * - There is a problem for empty lines: a text tag can not be applied to an
 *   empty region. And it can not be worked around easily for the last line.
 *
 * See https://bugzilla.gnome.org/show_bug.cgi?id=310847 for more details.
 */
static void
gtk_source_view_paint_line_background (GtkSourceView *view,
                                       GtkSnapshot   *snapshot,
                                       int            y, /* in buffer coordinates */
                                       int            height,
                                       const GdkRGBA *color)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GdkRectangle visible_rect;

	g_assert (GTK_SOURCE_IS_VIEW (view));

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &visible_rect);

	if ((y + height) <= visible_rect.y || y >= (visible_rect.y + visible_rect.height))
	{
		return;
	}

	gtk_snapshot_append_color (snapshot,
	                           color,
	                           &GRAPHENE_RECT_INIT (visible_rect.x,
	                                                y,
	                                                visible_rect.width,
	                                                height));

	/* If we premixed colors for the margin, we need to draw the line
	 * separator over the right-margin-position. We don't bother with
	 * drawing alpha over the right because in most cases it's so
	 * similar it's not worth the compositing cost.
	 */
	if (priv->show_right_margin &&
	    priv->right_margin_line_color_set &&
	    priv->cached_right_margin_pos >= 0)
	{
		int x = priv->cached_right_margin_pos + gtk_text_view_get_left_margin (GTK_TEXT_VIEW (view));

		if (x >= visible_rect.x &&
		    x < visible_rect.x + visible_rect.width)
		{
			gtk_snapshot_append_color (snapshot,
			                           &priv->right_margin_line_color,
			                           &GRAPHENE_RECT_INIT (x, y, 1, height));
		}
	}
}

static void
gtk_source_view_paint_marks_background (GtkSourceView *view,
                                        GtkSnapshot   *snapshot)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkTextView *text_view;
	GdkRectangle visible_rect;
	GArray *numbers;
	GArray *pixels;
	GArray *heights;
	gint y1, y2;
	gint count;
	gint i;

	if (priv->source_buffer == NULL ||
	    !_gtk_source_buffer_has_source_marks (priv->source_buffer))
	{
		return;
	}

	text_view = GTK_TEXT_VIEW (view);

	gtk_text_view_get_visible_rect (text_view, &visible_rect);

	y1 = visible_rect.y;
	y2 = y1 + visible_rect.height;

	numbers = g_array_new (FALSE, FALSE, sizeof (gint));
	pixels = g_array_new (FALSE, FALSE, sizeof (gint));
	heights = g_array_new (FALSE, FALSE, sizeof (gint));

	/* get the line numbers and y coordinates. */
	gtk_source_view_get_lines (text_view,
	                           y1,
	                           y2,
	                           pixels,
	                           heights,
	                           numbers,
	                           &count);

	if (count == 0)
	{
		gint n = 0;
		gint y;
		gint height;
		GtkTextIter iter;

		gtk_text_buffer_get_start_iter (gtk_text_view_get_buffer (text_view), &iter);
		gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

		g_array_append_val (pixels, y);
		g_array_append_val (pixels, height);
		g_array_append_val (numbers, n);
		count = 1;
	}

	GTK_SOURCE_PROFILER_BEGIN_MARK;

	for (i = 0; i < count; ++i)
	{
		gint line_to_paint;
		GSList *marks;
		GdkRGBA background;
		int priority;

		line_to_paint = g_array_index (numbers, gint, i);

		marks = gtk_source_buffer_get_source_marks_at_line (priv->source_buffer,
		                                                    line_to_paint,
		                                                    NULL);

		priority = -1;

		while (marks != NULL)
		{
			GtkSourceMarkAttributes *attrs;
			gint prio;
			GdkRGBA bg;

			attrs = gtk_source_view_get_mark_attributes (view,
			                                             gtk_source_mark_get_category (marks->data),
			                                             &prio);

			if (attrs != NULL &&
			    prio > priority &&
			    gtk_source_mark_attributes_get_background (attrs, &bg))
			{
				priority = prio;
				background = bg;
			}

			marks = g_slist_delete_link (marks, marks);
		}

		if (priority != -1)
		{
			gtk_source_view_paint_line_background (view,
			                                       snapshot,
			                                       g_array_index (pixels, gint, i),
			                                       g_array_index (heights, gint, i),
			                                       &background);
		}
	}

	GTK_SOURCE_PROFILER_END_MARK ("GtkSourceView::paint-marks-background", NULL);

	g_array_free (heights, TRUE);
	g_array_free (pixels, TRUE);
	g_array_free (numbers, TRUE);
}

static int
get_left_gutter_size (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	if (priv->left_gutter != NULL)
	{
		return gtk_widget_get_width (GTK_WIDGET (priv->left_gutter));
	}

	return 0;
}

static void
gtk_source_view_paint_right_margin (GtkSourceView *view,
                                    GtkSnapshot   *snapshot)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkTextView *text_view = GTK_TEXT_VIEW (view);
	GdkRectangle visible_rect;
	gdouble x;

	g_return_if_fail (priv->right_margin_line_color_set);

	GTK_SOURCE_PROFILER_BEGIN_MARK;

        gtk_text_view_get_visible_rect (text_view, &visible_rect);

	if (priv->cached_right_margin_pos < 0)
	{
		priv->cached_right_margin_pos =
			calculate_real_tab_width (view,
						  priv->right_margin_pos,
						  '_');
	}

	x = priv->cached_right_margin_pos + gtk_text_view_get_left_margin (text_view) + get_left_gutter_size (view);

	gtk_snapshot_append_color (snapshot,
	                           &priv->right_margin_line_color,
	                           &GRAPHENE_RECT_INIT (x - visible_rect.x,
	                                                0,
	                                                1,
	                                                visible_rect.height));

	if (priv->right_margin_overlay_color_set)
	{
		gtk_snapshot_append_color (snapshot,
		                           &priv->right_margin_overlay_color,
		                           &GRAPHENE_RECT_INIT (x - visible_rect.x + 1,
								0,
		                                                visible_rect.width,
		                                                visible_rect.height));
	}

	GTK_SOURCE_PROFILER_END_MARK ("GtkSourceView::paint-right-margin", NULL);
}

static inline int
realign (int offset,
	 int align)
{
	if (align > 0)
	{
		int padding = (align - (offset % align)) % align;
		return offset + padding;
	}

	return 0;
}

static void
gtk_source_view_paint_background_pattern_grid (GtkSourceView *view,
                                               GtkSnapshot   *snapshot)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GdkRectangle visible_rect;
	int x, y, x2, y2;
	PangoContext *context;
	PangoLayout *layout;
	GtkTextIter iter;
	int grid_half_height = 8;
	int grid_width = 16;
	int grid_height = 16;
	int line_y, line_height;
	int left_margin;

	left_margin = gtk_text_view_get_left_margin (GTK_TEXT_VIEW (view));

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &visible_rect);

	context = gtk_widget_get_pango_context (GTK_WIDGET (view));
	layout = pango_layout_new (context);
	pango_layout_set_text (layout, "X", 1);
	pango_layout_get_pixel_size (layout, &grid_width, &grid_height);
	g_object_unref (layout);

	/* Try to take CSS line-height into account */
	if (gtk_text_view_get_wrap_mode (GTK_TEXT_VIEW (view)) == GTK_WRAP_NONE)
	{
		gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter, visible_rect.x, visible_rect.y);
		gtk_text_view_get_line_yrange (GTK_TEXT_VIEW (view), &iter, &line_y, &line_height);

		if (line_height > grid_height)
		{
			grid_height = line_height;
		}
	}

	/* each character becomes 2 stacked boxes. */
	grid_height = MAX (1, grid_height);
	grid_half_height = grid_height / 2;
	grid_width = MAX (1, grid_width);

	/* Align our drawing position with a multiple of the grid size. */
	x = realign (visible_rect.x - grid_width, grid_width);
	y = realign (visible_rect.y - grid_half_height, grid_half_height);
	x2 = realign (x + visible_rect.width + grid_width * 2, grid_width);
	y2 = realign (y + visible_rect.height + grid_height, grid_height);

	gtk_snapshot_save (snapshot);
	gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (left_margin, 0));

	gtk_snapshot_push_repeat (snapshot,
	                          &GRAPHENE_RECT_INIT (x, y, x2 - x, y2 - y),
	                          &GRAPHENE_RECT_INIT (x, y, grid_width, grid_height));
	gtk_snapshot_append_color (snapshot,
	                           &priv->background_pattern_color,
	                           &GRAPHENE_RECT_INIT (x+1, y, 1, grid_height));
	gtk_snapshot_append_color (snapshot,
	                           &priv->background_pattern_color,
	                           &GRAPHENE_RECT_INIT (x, y, grid_width, 1));
	gtk_snapshot_append_color (snapshot,
	                           &priv->background_pattern_color,
	                           &GRAPHENE_RECT_INIT (x, y + grid_half_height, grid_width, 1));
	gtk_snapshot_pop (snapshot);

	gtk_snapshot_restore (snapshot);
}

static void
gtk_source_view_paint_current_line_highlight (GtkSourceView *view,
                                              GtkSnapshot   *snapshot)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkTextBuffer *buffer;
	GtkTextIter cur, sel;
	gint y;
	gint height;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	/* Don't paint line if the selection cross multiple lines */
	if (gtk_text_buffer_get_selection_bounds (buffer, &cur, &sel) &&
            gtk_text_iter_get_line (&cur) != gtk_text_iter_get_line (&sel))
	{
		return;
	}

	gtk_text_view_get_line_yrange (GTK_TEXT_VIEW (view), &cur, &y, &height);

	gtk_source_view_paint_line_background (view,
					       snapshot,
					       y, height,
					       &priv->current_line_background_color);
}

static void
gtk_source_view_snapshot_layer (GtkTextView      *text_view,
                                GtkTextViewLayer  layer,
                                GtkSnapshot      *snapshot)
{
	GtkSourceView *view = GTK_SOURCE_VIEW (text_view);
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	gtk_snapshot_save (snapshot);

	if (layer == GTK_TEXT_VIEW_LAYER_BELOW_TEXT)
	{
		GtkRoot *root;

		/* Now draw the background pattern, which might draw above the
		 * right-margin area for additional texture. We can't really optimize
		 * these too much since they move every scroll. Otherwise we'd move
		 * them into the snapshot of the GtkSourceView rather than a layer.
		 */
		if (priv->background_pattern == GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID &&
		    priv->background_pattern_color_set)
		{
			gtk_source_view_paint_background_pattern_grid (view, snapshot);
		}

		/* Only draw the line hightlight on the active window and if
		 * we are sensitive to keyboard input.
		 */
		if (gtk_widget_is_sensitive (GTK_WIDGET (view)) &&
		    priv->highlight_current_line &&
		    priv->current_line_background_color_set &&
		    (root = gtk_widget_get_root (GTK_WIDGET (view))) &&
		    GTK_IS_WINDOW (root) &&
		    gtk_window_is_active (GTK_WINDOW (root)))
		{
			gtk_source_view_paint_current_line_highlight (view, snapshot);
		}

		gtk_source_view_paint_marks_background (view, snapshot);
	}
	else if (layer == GTK_TEXT_VIEW_LAYER_ABOVE_TEXT)
	{
		if (priv->space_drawer != NULL)
		{
			_gtk_source_space_drawer_draw (priv->space_drawer, view, snapshot);
		}

		if (priv->annotations != NULL)
		{
			_gtk_source_annotations_draw (priv->annotations, view, snapshot);
		}
	}

	gtk_snapshot_restore (snapshot);
}

static void
gtk_source_view_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (GTK_SOURCE_VIEW (widget));
	GdkRectangle visible_rect;

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (widget), &visible_rect);
	gtk_source_view_ensure_redrawn_rect_is_highlighted (GTK_SOURCE_VIEW (widget), &visible_rect);

	/* Draw the right margin vertical line + background overlay. This is
	 * drawn from the GtkSourceView.snapshot() vfunc because that is the
	 * one place we can append the rectangular regions without a GSK
	 * translation transform being applied. This is very important from a
	 * performance perspective because once a transform is applied GSK will
	 * no longer elide the large rectangular regions meaning we draw many
	 * pixels multiple times.
	 *
	 * We already potentially draw over the right-margin twice (once for
	 * the textview background and once for the right-margin) so we
	 * additionally disable "textview text" from our high-priority CSS to
	 * save a third rectangular region draw. These framebuffer damages are
	 * very important to avoid from a scrolling performance perspective.
	 *
	 * Of course, this is all subject to change in GTK if we can manage to
	 * track transforms across gtk_snapshot_append_*() calls or elide
	 * rectangular regions when appending newer regions.
	 */
	if (priv->show_right_margin)
	{
		gtk_source_view_paint_right_margin (GTK_SOURCE_VIEW (widget), snapshot);
	}

	GTK_WIDGET_CLASS (gtk_source_view_parent_class)->snapshot (widget, snapshot);
}

/* This is a pretty important function... We call it when the tab_stop is changed,
 * and when the font is changed.
 * NOTE: You must change this with the default font for now...
 * It may be a good idea to set the tab_width for each GtkTextTag as well
 * based on the font that we set at creation time
 * something like style_cache_set_tabs_from_font or the like.
 * Now, this *may* not be necessary because most tabs won't be inside of another highlight,
 * except for things like multi-line comments (which will usually have an italic font which
 * may or may not be a different size than the standard one), or if some random language
 * definition decides that it would be spiffy to have a bg color for "start of line" whitespace
 * "^\(\t\| \)+" would probably do the trick for that.
 */
static gint
calculate_real_tab_width (GtkSourceView *view, guint tab_size, gchar c)
{
	PangoLayout *layout;
	gchar *tab_string;
	gint tab_width = 0;

	if (tab_size == 0)
	{
		return -1;
	}

	tab_string = g_strnfill (tab_size, c);
	layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), tab_string);
	g_free (tab_string);

	if (layout != NULL)
	{
		pango_layout_get_pixel_size (layout, &tab_width, NULL);
		g_object_unref (G_OBJECT (layout));
	}
	else
	{
		tab_width = -1;
	}

	return tab_width;
}

static GtkTextBuffer *
gtk_source_view_create_buffer (GtkTextView *text_view)
{
	return GTK_TEXT_BUFFER (gtk_source_buffer_new (NULL));
}


/* ----------------------------------------------------------------------
 * Public interface
 * ----------------------------------------------------------------------
 */

/**
 * gtk_source_view_new:
 *
 * Creates a new `GtkSourceView`.
 *
 * By default, an empty [class@Buffer] will be lazily created and can be
 * retrieved with [method@Gtk.TextView.get_buffer].
 *
 * If you want to specify your own buffer, either override the
 * [vfunc@Gtk.TextView.create_buffer] factory method, or use
 * [ctor@View.new_with_buffer].
 *
 * Returns: a new #GtkSourceView.
 */
GtkWidget *
gtk_source_view_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_VIEW, NULL);
}

/**
 * gtk_source_view_new_with_buffer:
 * @buffer: a #GtkSourceBuffer.
 *
 * Creates a new #GtkSourceView widget displaying the buffer @buffer.
 *
 * One buffer can be shared among many widgets.
 *
 * Returns: a new #GtkSourceView.
 */
GtkWidget *
gtk_source_view_new_with_buffer (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	return g_object_new (GTK_SOURCE_TYPE_VIEW,
			     "buffer", buffer,
			     NULL);
}

/**
 * gtk_source_view_get_show_line_numbers:
 * @view: a #GtkSourceView.
 *
 * Returns whether line numbers are displayed beside the text.
 *
 * Return value: %TRUE if the line numbers are displayed.
 */
gboolean
gtk_source_view_get_show_line_numbers (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	return priv->show_line_numbers;
}

/**
 * gtk_source_view_set_show_line_numbers:
 * @view: a #GtkSourceView.
 * @show: whether line numbers should be displayed.
 *
 * If %TRUE line numbers will be displayed beside the text.
 */
void
gtk_source_view_set_show_line_numbers (GtkSourceView *view,
                                       gboolean       show)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	show = show != FALSE;

	if (show == priv->show_line_numbers)
	{
		return;
	}

	if (priv->line_renderer == NULL)
	{
		GtkSourceGutter *gutter;

		gutter = gtk_source_view_get_gutter (view, GTK_TEXT_WINDOW_LEFT);

		priv->line_renderer = _gtk_source_gutter_renderer_lines_new ();
		g_object_set (priv->line_renderer,
		              "alignment-mode", GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_FIRST,
		              "yalign", 0.5,
		              "xalign", 1.0,
		              "xpad", 6,
		              NULL);

		gtk_source_gutter_insert (gutter,
		                          priv->line_renderer,
		                          GTK_SOURCE_VIEW_GUTTER_POSITION_LINES);
	}

	gtk_widget_set_visible (GTK_WIDGET (priv->line_renderer), show);
	priv->show_line_numbers = show;

	g_object_notify_by_pspec (G_OBJECT (view),
	                          properties [PROP_SHOW_LINE_NUMBERS]);
}

/**
 * gtk_source_view_get_show_line_marks:
 * @view: a #GtkSourceView.
 *
 * Returns whether line marks are displayed beside the text.
 *
 * Return value: %TRUE if the line marks are displayed.
 */
gboolean
gtk_source_view_get_show_line_marks (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	return priv->show_line_marks;
}

static void
gutter_renderer_marks_activate (GtkSourceGutterRenderer *renderer,
                                GtkTextIter             *iter,
                                const GdkRectangle      *area,
                                guint                    button,
                                GdkModifierType          state,
                                gint                     n_presses,
                                GtkSourceView           *view)
{
	g_signal_emit (view, signals[LINE_MARK_ACTIVATED], 0,
	               iter, button, state, n_presses);
}

/**
 * gtk_source_view_set_show_line_marks:
 * @view: a #GtkSourceView.
 * @show: whether line marks should be displayed.
 *
 * If %TRUE line marks will be displayed beside the text.
 */
void
gtk_source_view_set_show_line_marks (GtkSourceView *view,
                                     gboolean       show)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	show = show != FALSE;

	if (show == priv->show_line_marks)
	{
		return;
	}

	if (priv->marks_renderer == NULL)
	{
		GtkSourceGutter *gutter;

		gutter = gtk_source_view_get_gutter (view, GTK_TEXT_WINDOW_LEFT);

		priv->marks_renderer = gtk_source_gutter_renderer_marks_new ();

		gtk_source_gutter_insert (gutter,
		                          priv->marks_renderer,
		                          GTK_SOURCE_VIEW_GUTTER_POSITION_MARKS);

		g_signal_connect (priv->marks_renderer,
		                  "activate",
		                  G_CALLBACK (gutter_renderer_marks_activate),
		                  view);
	}

	gtk_widget_set_visible (GTK_WIDGET (priv->marks_renderer), show);
	priv->show_line_marks = show;

	g_object_notify_by_pspec (G_OBJECT (view),
	                          properties [PROP_SHOW_LINE_MARKS]);
}

static gboolean
set_tab_stops_internal (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	PangoTabArray *tab_array;
	gint real_tab_width;

	real_tab_width = calculate_real_tab_width (view, priv->tab_width, ' ');

	if (real_tab_width < 0)
	{
		return FALSE;
	}

	tab_array = pango_tab_array_new (1, TRUE);
	pango_tab_array_set_tab (tab_array, 0, PANGO_TAB_LEFT, real_tab_width);

	gtk_text_view_set_tabs (GTK_TEXT_VIEW (view),
				tab_array);
	priv->tabs_set = TRUE;

	pango_tab_array_free (tab_array);

	return TRUE;
}

/**
 * gtk_source_view_set_tab_width:
 * @view: a #GtkSourceView.
 * @width: width of tab in characters.
 *
 * Sets the width of tabulation in characters.
 *
 * The #GtkTextBuffer still contains `\t` characters,
 * but they can take a different visual width in a [class@View] widget.
 */
void
gtk_source_view_set_tab_width (GtkSourceView *view,
                               guint          width)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	guint save_width;

	g_return_if_fail (GTK_SOURCE_VIEW (view));
	g_return_if_fail (0 < width && width <= MAX_TAB_WIDTH);

	if (priv->tab_width == width)
	{
		return;
	}

	save_width = priv->tab_width;
	priv->tab_width = width;
	if (set_tab_stops_internal (view))
	{
		g_object_notify_by_pspec (G_OBJECT (view),
		                          properties [PROP_TAB_WIDTH]);
	}
	else
	{
		g_warning ("Impossible to set tab width.");
		priv->tab_width = save_width;
	}
}

/**
 * gtk_source_view_get_tab_width:
 * @view: a #GtkSourceView.
 *
 * Returns the width of tabulation in characters.
 *
 * Return value: width of tab.
 */
guint
gtk_source_view_get_tab_width (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), DEFAULT_TAB_WIDTH);

	return priv->tab_width;
}

/**
 * gtk_source_view_set_indent_width:
 * @view: a #GtkSourceView.
 * @width: indent width in characters.
 *
 * Sets the number of spaces to use for each step of indent when the tab key is
 * pressed.
 *
 * If @width is -1, the value of the [property@View:tab-width] property
 * will be used.
 *
 * The [property@View:indent-width] interacts with the
 * [property@View:insert-spaces-instead-of-tabs] property and
 * [property@View:tab-width]. An example will be clearer:
 *
 * If the [property@View:indent-width] is 4 and [property@View:tab-width] is 8 and
 * [property@View:insert-spaces-instead-of-tabs] is %FALSE, then pressing the tab
 * key at the beginning of a line will insert 4 spaces. So far so good. Pressing
 * the tab key a second time will remove the 4 spaces and insert a `\t` character
 * instead (since [property@View:tab-width] is 8). On the other hand, if
 * [property@View:insert-spaces-instead-of-tabs] is %TRUE, the second tab key
 * pressed will insert 4 more spaces for a total of 8 spaces in the
 * [class@Gtk.TextBuffer].
 *
 * The test-widget program (available in the GtkSourceView repository) may be
 * useful to better understand the indentation settings (enable the space
 * drawing!).
 */
void
gtk_source_view_set_indent_width (GtkSourceView *view,
                                  gint           width)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_VIEW (view));
	g_return_if_fail (width == -1 || (0 < width && width <= MAX_INDENT_WIDTH));

	if (priv->indent_width != width)
	{
		priv->indent_width = width;
		g_object_notify_by_pspec (G_OBJECT (view),
		                          properties [PROP_INDENT_WIDTH]);
	}
}

/**
 * gtk_source_view_get_indent_width:
 * @view: a #GtkSourceView.
 *
 * Returns the number of spaces to use for each step of indent.
 *
 * See [method@View.set_indent_width] for details.
 *
 * Return value: indent width.
 */
gint
gtk_source_view_get_indent_width (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), 0);

	return priv->indent_width;
}

static guint
get_real_indent_width (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	return priv->indent_width < 0 ?
	       priv->tab_width :
	       (guint) priv->indent_width;
}

static gchar *
get_indent_string (guint tabs,
                   guint spaces)
{
	gchar *str;

	str = g_malloc (tabs + spaces + 1);

	if (tabs > 0)
	{
		memset (str, '\t', tabs);
	}

	if (spaces > 0)
	{
		memset (str + tabs, ' ', spaces);
	}

	str[tabs + spaces] = '\0';

	return str;
}

/**
 * gtk_source_view_indent_lines:
 * @view: a #GtkSourceView.
 * @start: #GtkTextIter of the first line to indent
 * @end: #GtkTextIter of the last line to indent
 *
 * Inserts one indentation level at the beginning of the specified lines. The
 * empty lines are not indented.
 */
void
gtk_source_view_indent_lines (GtkSourceView *view,
                              GtkTextIter   *start,
                              GtkTextIter   *end)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkTextBuffer *buf;
	gboolean bracket_hl;
	GtkTextMark *start_mark, *end_mark;
	gint start_line, end_line;
	gchar *tab_buffer = NULL;
	guint tabs = 0;
	guint spaces = 0;
	gint i;

	if (priv->completion != NULL)
	{
		gtk_source_completion_block_interactive (priv->completion);
	}

	buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	bracket_hl = gtk_source_buffer_get_highlight_matching_brackets (GTK_SOURCE_BUFFER (buf));
	gtk_source_buffer_set_highlight_matching_brackets (GTK_SOURCE_BUFFER (buf), FALSE);

	start_mark = gtk_text_buffer_create_mark (buf, NULL, start, FALSE);
	end_mark = gtk_text_buffer_create_mark (buf, NULL, end, FALSE);

	start_line = gtk_text_iter_get_line (start);
	end_line = gtk_text_iter_get_line (end);

	if ((gtk_text_iter_get_visible_line_offset (end) == 0) &&
	    (end_line > start_line))
	{
		end_line--;
	}

	if (priv->insert_spaces)
	{
		spaces = get_real_indent_width (view);

		tab_buffer = g_strnfill (spaces, ' ');
	}
	else if (priv->indent_width > 0 &&
	         priv->indent_width != (gint)priv->tab_width)
	{
		guint indent_width;

		indent_width = get_real_indent_width (view);
		spaces = indent_width % priv->tab_width;
		tabs = indent_width / priv->tab_width;

		tab_buffer = get_indent_string (tabs, spaces);
	}
	else
	{
		tabs = 1;
		tab_buffer = g_strdup ("\t");
	}

	gtk_text_buffer_begin_user_action (buf);

	for (i = start_line; i <= end_line; i++)
	{
		GtkTextIter iter;
		GtkTextIter iter2;
		guint replaced_spaces = 0;

		gtk_text_buffer_get_iter_at_line (buf, &iter, i);

		/* Don't add indentation on completely empty lines, to not add
		 * trailing spaces.
		 * Note that non-empty lines containing only whitespaces are
		 * indented like any other non-empty line, because those lines
		 * already contain trailing spaces, some users use those
		 * whitespaces to more easily insert text at the right place
		 * without the need to insert the indentation each time.
		 */
		if (gtk_text_iter_ends_line (&iter))
		{
			continue;
		}

		/* add spaces always after tabs, to avoid the case
		 * where "\t" becomes "  \t" with no visual difference */
		while (gtk_text_iter_get_char (&iter) == '\t')
		{
			gtk_text_iter_forward_char (&iter);
		}

		/* if tabs are allowed try to merge the spaces
		 * with the tab we are going to insert (if any) */
		iter2 = iter;
		while (!priv->insert_spaces &&
		       (gtk_text_iter_get_char (&iter2) == ' ') &&
			replaced_spaces < priv->tab_width)
		{
			++replaced_spaces;

			gtk_text_iter_forward_char (&iter2);
		}

		if (replaced_spaces > 0)
		{
			gchar *indent_buf;
			guint t, s;

			t = tabs + (spaces + replaced_spaces) / priv->tab_width;
			s = (spaces + replaced_spaces) % priv->tab_width;
			indent_buf = get_indent_string (t, s);

			gtk_text_buffer_delete (buf, &iter, &iter2);
			gtk_text_buffer_insert (buf, &iter, indent_buf, -1);

			g_free (indent_buf);
		}
		else
		{
			gtk_text_buffer_insert (buf, &iter, tab_buffer, -1);
		}
	}

	gtk_text_buffer_end_user_action (buf);

	g_free (tab_buffer);

	gtk_source_buffer_set_highlight_matching_brackets (GTK_SOURCE_BUFFER (buf), bracket_hl);

	if (priv->completion != NULL)
	{
		gtk_source_completion_unblock_interactive (priv->completion);
	}

	gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view),
					    gtk_text_buffer_get_insert (buf));

	/* revalidate iters */
	gtk_text_buffer_get_iter_at_mark (buf, start, start_mark);
	gtk_text_buffer_get_iter_at_mark (buf, end, end_mark);

	gtk_text_buffer_delete_mark (buf, start_mark);
	gtk_text_buffer_delete_mark (buf, end_mark);
}

/**
 * gtk_source_view_unindent_lines:
 * @view: a #GtkSourceView.
 * @start: #GtkTextIter of the first line to indent
 * @end: #GtkTextIter of the last line to indent
 *
 * Removes one indentation level at the beginning of the
 * specified lines.
 */
void
gtk_source_view_unindent_lines (GtkSourceView *view,
                                GtkTextIter   *start,
                                GtkTextIter   *end)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkTextBuffer *buf;
	gboolean bracket_hl;
	GtkTextMark *start_mark, *end_mark;
	gint start_line, end_line;
	gint tab_width;
	gint indent_width;
	gint i;

	if (priv->completion != NULL)
	{
		gtk_source_completion_block_interactive (priv->completion);
	}

	buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	bracket_hl = gtk_source_buffer_get_highlight_matching_brackets (GTK_SOURCE_BUFFER (buf));
	gtk_source_buffer_set_highlight_matching_brackets (GTK_SOURCE_BUFFER (buf), FALSE);

	start_mark = gtk_text_buffer_create_mark (buf, NULL, start, FALSE);
	end_mark = gtk_text_buffer_create_mark (buf, NULL, end, FALSE);

	start_line = gtk_text_iter_get_line (start);
	end_line = gtk_text_iter_get_line (end);

	if ((gtk_text_iter_get_visible_line_offset (end) == 0) &&
	    (end_line > start_line))
	{
		end_line--;
	}

	tab_width = priv->tab_width;
	indent_width = get_real_indent_width (view);

	gtk_text_buffer_begin_user_action (buf);

	for (i = start_line; i <= end_line; i++)
	{
		GtkTextIter iter, iter2;
		gint to_delete = 0;
		gint to_delete_equiv = 0;

		gtk_text_buffer_get_iter_at_line (buf, &iter, i);

		iter2 = iter;

		while (!gtk_text_iter_ends_line (&iter2) &&
		       to_delete_equiv < indent_width)
		{
			gunichar c;

			c = gtk_text_iter_get_char (&iter2);
			if (c == '\t')
			{
				to_delete_equiv += tab_width - to_delete_equiv % tab_width;
				++to_delete;
			}
			else if (c == ' ')
			{
				++to_delete_equiv;
				++to_delete;
			}
			else
			{
				break;
			}

			gtk_text_iter_forward_char (&iter2);
		}

		if (to_delete > 0)
		{
			gtk_text_iter_set_line_offset (&iter2, to_delete);
			gtk_text_buffer_delete (buf, &iter, &iter2);
		}
	}

	gtk_text_buffer_end_user_action (buf);

	gtk_source_buffer_set_highlight_matching_brackets (GTK_SOURCE_BUFFER (buf), bracket_hl);

	if (priv->completion != NULL)
	{
		gtk_source_completion_unblock_interactive (priv->completion);
	}

	gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view),
					    gtk_text_buffer_get_insert (buf));

	/* revalidate iters */
	gtk_text_buffer_get_iter_at_mark (buf, start, start_mark);
	gtk_text_buffer_get_iter_at_mark (buf, end, end_mark);

	gtk_text_buffer_delete_mark (buf, start_mark);
	gtk_text_buffer_delete_mark (buf, end_mark);
}

static gint
get_line_offset_in_equivalent_spaces (GtkSourceView *view,
                                      GtkTextIter   *iter)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkTextIter i;
	gint tab_width;
	gint n = 0;

	tab_width = priv->tab_width;

	i = *iter;
	gtk_text_iter_set_line_offset (&i, 0);

	while (!gtk_text_iter_equal (&i, iter))
	{
		gunichar c;

		c = gtk_text_iter_get_char (&i);
		if (c == '\t')
		{
			n += tab_width - n % tab_width;
		}
		else
		{
			++n;
		}

		gtk_text_iter_forward_char (&i);
	}

	return n;
}

static void
insert_tab_or_spaces (GtkSourceView *view,
                      GtkTextIter   *start,
                      GtkTextIter   *end)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkTextBuffer *buf;
	gchar *tab_buf;
	gint cursor_offset = 0;

	if (priv->insert_spaces)
	{
		gint indent_width;
		gint pos;
		gint spaces;

		indent_width = get_real_indent_width (view);

		/* CHECK: is this a performance problem? */
		pos = get_line_offset_in_equivalent_spaces (view, start);
		spaces = indent_width - pos % indent_width;

		tab_buf = g_strnfill (spaces, ' ');
	}
	else if (priv->indent_width > 0 &&
	         priv->indent_width != (gint)priv->tab_width)
	{
		GtkTextIter iter;
		gint i;
		gint tab_width;
		gint indent_width;
		gint from;
		gint to;
		gint preceding_spaces = 0;
		gint following_tabs = 0;
		gint equiv_spaces;
		gint tabs;
		gint spaces;

		tab_width = priv->tab_width;
		indent_width = get_real_indent_width (view);

		/* CHECK: is this a performance problem? */
		from = get_line_offset_in_equivalent_spaces (view, start);
		to = indent_width * (1 + from / indent_width);
		equiv_spaces = to - from;

		/* extend the selection to include
		 * preceding spaces so that if indentation
		 * width < tab width, two conseutive indentation
		 * width units get compressed into a tab */
		iter = *start;
		for (i = 0; i < tab_width; ++i)
		{
			gtk_text_iter_backward_char (&iter);

			if (gtk_text_iter_get_char (&iter) == ' ')
			{
				++preceding_spaces;
			}
			else
			{
				break;
			}
		}

		gtk_text_iter_backward_chars (start, preceding_spaces);

		/* now also extend the selection to the following tabs
		 * since we do not want to insert spaces before a tab
		 * since it may have no visual effect */
		while (gtk_text_iter_get_char (end) == '\t')
		{
			++following_tabs;
			gtk_text_iter_forward_char (end);
		}

		tabs = (preceding_spaces + equiv_spaces) / tab_width;
		spaces = (preceding_spaces + equiv_spaces) % tab_width;

		tab_buf = get_indent_string (tabs + following_tabs, spaces);

		cursor_offset = gtk_text_iter_get_offset (start) +
			        tabs +
		                (following_tabs > 0 ? 1 : spaces);
	}
	else
	{
		tab_buf = g_strdup ("\t");
	}

	buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gtk_text_buffer_begin_user_action (buf);

	gtk_text_buffer_delete (buf, start, end);
	gtk_text_buffer_insert (buf, start, tab_buf, -1);

	/* adjust cursor position if needed */
	if (cursor_offset > 0)
	{
		GtkTextIter iter;

		gtk_text_buffer_get_iter_at_offset (buf, &iter, cursor_offset);
		gtk_text_buffer_place_cursor (buf, &iter);
	}

	gtk_text_buffer_end_user_action (buf);

	g_free (tab_buf);
}

static void
gtk_source_view_move_words (GtkSourceView *view,
                            gint           step)
{
	GtkTextBuffer *buf;
	GtkTextIter s, e, ns, ne;
	GtkTextMark *nsmark, *nemark;
	gchar *old_text, *new_text;

	buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (step == 0 || gtk_text_view_get_editable (GTK_TEXT_VIEW (view)) == FALSE)
	{
		return;
	}

	gtk_text_buffer_get_selection_bounds (buf, &s, &e);

	if (gtk_text_iter_compare (&s, &e) == 0)
	{
		if (!gtk_text_iter_starts_word (&s))
		{
			if (!gtk_text_iter_inside_word (&s) && !gtk_text_iter_ends_word (&s))
			{
				return;
			}
			else
			{
				gtk_text_iter_backward_word_start (&s);
			}
		}

		if (!gtk_text_iter_starts_word (&s))
		{
			return;
		}

		e = s;

		if (!gtk_text_iter_ends_word (&e))
		{
			if (!gtk_text_iter_forward_word_end (&e))
			{
				gtk_text_iter_forward_to_end (&e);
			}

			if (!gtk_text_iter_ends_word (&e))
			{
				return;
			}
		}
	}

	/* Swap the selection with the next or previous word, based on step */
	if (step > 0)
	{
		ne = e;

		if (!gtk_text_iter_forward_word_ends (&ne, step))
		{
			gtk_text_iter_forward_to_end (&ne);
		}

		if (!gtk_text_iter_ends_word (&ne) || gtk_text_iter_equal (&ne, &e))
		{
			return;
		}

		ns = ne;

		if (!gtk_text_iter_backward_word_start (&ns))
		{
			return;
		}
	}
	else
	{
		ns = s;

		if (!gtk_text_iter_backward_word_starts (&ns, -step))
		{
			return;
		}

		ne = ns;

		if (!gtk_text_iter_forward_word_end (&ne))
		{
			return;
		}
	}

	if (gtk_text_iter_in_range (&ns, &s, &e) ||
	    (!gtk_text_iter_equal (&s, &ne) &&
	     gtk_text_iter_in_range (&ne, &s, &e)))
	{
		return;
	}

	old_text = gtk_text_buffer_get_text (buf, &s, &e, TRUE);
	new_text = gtk_text_buffer_get_text (buf, &ns, &ne, TRUE);

	gtk_text_buffer_begin_user_action (buf);

	nsmark = gtk_text_buffer_create_mark (buf, NULL, &ns, step < 0);
	nemark = gtk_text_buffer_create_mark (buf, NULL, &ne, step < 0);

	gtk_text_buffer_delete (buf, &s, &e);
	gtk_text_buffer_insert (buf, &s, new_text, -1);

	gtk_text_buffer_get_iter_at_mark (buf, &ns, nsmark);
	gtk_text_buffer_get_iter_at_mark (buf, &ne, nemark);

	gtk_text_buffer_delete (buf, &ns, &ne);
	gtk_text_buffer_insert (buf, &ns, old_text, -1);

	ne = ns;
	gtk_text_buffer_get_iter_at_mark (buf, &ns, nsmark);

	gtk_text_buffer_select_range (buf, &ns, &ne);

	gtk_text_buffer_delete_mark (buf, nsmark);
	gtk_text_buffer_delete_mark (buf, nemark);

	gtk_text_buffer_end_user_action (buf);

	gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view),
	                                    gtk_text_buffer_get_insert (buf));

	g_free (old_text);
	g_free (new_text);
}

/* FIXME could be a function of GtkSourceBuffer, it's also useful for the
 * FileLoader.
 */
static void
remove_trailing_newline (GtkTextBuffer *buffer)
{
	GtkTextIter start;
	GtkTextIter end;

	gtk_text_buffer_get_end_iter (buffer, &end);
	start = end;

	gtk_text_iter_set_line_offset (&start, 0);

	if (gtk_text_iter_ends_line (&start) &&
	    gtk_text_iter_backward_line (&start))
	{
		if (!gtk_text_iter_ends_line (&start))
		{
			gtk_text_iter_forward_to_line_end (&start);
		}

		gtk_text_buffer_delete (buffer, &start, &end);
	}
}

static void
move_lines_up (GtkTextBuffer *buffer)
{
	GtkTextIter selection_iter_start;
	GtkTextIter selection_iter_end;
	GtkTextMark *start_mark;
	gboolean trailing_newline_inserted = FALSE;
	gchar *text;
	GtkTextIter insert_pos;

	/* start and end are set in ascending order. */
	gtk_text_buffer_get_selection_bounds (buffer, &selection_iter_start, &selection_iter_end);

	/* Move to start of line for the beginning of the selection.
	 * Entire lines must be moved.
	 */
	gtk_text_iter_set_line_offset (&selection_iter_start, 0);

	if (gtk_text_iter_is_start (&selection_iter_start))
	{
		/* Nothing to do, and the undo/redo history must remain unchanged. */
		return;
	}

	/* Get the entire lines, including the paragraph terminator. */
	if (!gtk_text_iter_starts_line (&selection_iter_end) ||
	    gtk_text_iter_get_line (&selection_iter_start) == gtk_text_iter_get_line (&selection_iter_end))
	{
		gtk_text_iter_forward_line (&selection_iter_end);
	}

	gtk_text_buffer_begin_user_action (buffer);

	/* We must be careful about what operations we do on the GtkTextBuffer,
	 * for the undo/redo.
	 */

	/* Insert a trailing newline, but only if necessary. */
	if (gtk_text_iter_is_end (&selection_iter_end) &&
	    (gtk_text_iter_get_line (&selection_iter_start) == gtk_text_iter_get_line (&selection_iter_end) ||
	     !gtk_text_iter_starts_line (&selection_iter_end)))
	{
		start_mark = gtk_text_buffer_create_mark (buffer, NULL, &selection_iter_start, TRUE);

		gtk_text_buffer_insert (buffer, &selection_iter_end, "\n", -1);
		trailing_newline_inserted = TRUE;

		gtk_text_buffer_get_iter_at_mark (buffer, &selection_iter_start, start_mark);
		gtk_text_buffer_delete_mark (buffer, start_mark);
		start_mark = NULL;
	}

	text = gtk_text_buffer_get_text (buffer, &selection_iter_start, &selection_iter_end, TRUE);

	gtk_text_buffer_delete (buffer, &selection_iter_start, &selection_iter_end);

	insert_pos = selection_iter_start;
	gtk_text_iter_backward_line (&insert_pos);

	start_mark = gtk_text_buffer_create_mark (buffer, NULL, &insert_pos, TRUE);

	gtk_text_buffer_insert (buffer, &insert_pos, text, -1);
	g_free (text);

	/* Select the moved text. */
	gtk_text_buffer_get_iter_at_mark (buffer, &selection_iter_start, start_mark);
	gtk_text_buffer_delete_mark (buffer, start_mark);
	start_mark = NULL;

	gtk_text_buffer_select_range (buffer, &selection_iter_start, &insert_pos);

	if (trailing_newline_inserted)
	{
		remove_trailing_newline (buffer);
	}

	gtk_text_buffer_end_user_action (buffer);
}

static gboolean
can_move_lines_down (GtkTextBuffer     *buffer,
		     const GtkTextIter *selection_iter_start,
		     const GtkTextIter *selection_iter_end)
{
	GtkTextIter end_iter;

	gtk_text_buffer_get_end_iter (buffer, &end_iter);

	if (gtk_text_iter_get_line (selection_iter_end) != gtk_text_iter_get_line (&end_iter))
	{
		return TRUE;
	}

	/* Now we know that 'selection_iter_end' is on the last line. */

	return (gtk_text_iter_get_line (selection_iter_start) != gtk_text_iter_get_line (selection_iter_end) &&
		gtk_text_iter_starts_line (selection_iter_end));
}

static void
move_lines_down (GtkTextBuffer *buffer)
{
	GtkTextIter selection_iter_start;
	GtkTextIter selection_iter_end;
	gchar *text;
	GtkTextIter insert_pos;
	GtkTextIter end_iter;
	GtkTextMark *start_mark;
	gboolean trailing_newline_inserted = FALSE;

	/* start and end are set in ascending order. */
	gtk_text_buffer_get_selection_bounds (buffer, &selection_iter_start, &selection_iter_end);

	if (!can_move_lines_down (buffer, &selection_iter_start, &selection_iter_end))
	{
		/* Nothing to do, and the undo/redo history must remain unchanged. */
		return;
	}

	/* Move to start of line for the beginning of the selection.
	 * Entire lines must be moved.
	 */
	gtk_text_iter_set_line_offset (&selection_iter_start, 0);

	/* Get the entire lines, including the paragraph terminator. */
	if (!gtk_text_iter_starts_line (&selection_iter_end) ||
	    gtk_text_iter_get_line (&selection_iter_start) == gtk_text_iter_get_line (&selection_iter_end))
	{
		gtk_text_iter_forward_line (&selection_iter_end);
	}

	gtk_text_buffer_begin_user_action (buffer);

	/* We must be careful about what operations we do on the GtkTextBuffer,
	 * for the undo/redo.
	 */

	text = gtk_text_buffer_get_text (buffer, &selection_iter_start, &selection_iter_end, TRUE);

	gtk_text_buffer_delete (buffer, &selection_iter_start, &selection_iter_end);

	insert_pos = selection_iter_end;

	/* Insert a trailing newline, but only if necessary. */
	gtk_text_buffer_get_end_iter (buffer, &end_iter);
	if (gtk_text_iter_get_line (&insert_pos) == gtk_text_iter_get_line (&end_iter))
	{
		start_mark = gtk_text_buffer_create_mark (buffer, NULL, &insert_pos, TRUE);

		gtk_text_buffer_insert (buffer, &end_iter, "\n", -1);
		trailing_newline_inserted = TRUE;

		gtk_text_buffer_get_iter_at_mark (buffer, &insert_pos, start_mark);
		gtk_text_buffer_delete_mark (buffer, start_mark);
		start_mark = NULL;
	}

	gtk_text_iter_forward_line (&insert_pos);

	start_mark = gtk_text_buffer_create_mark (buffer, NULL, &insert_pos, TRUE);

	gtk_text_buffer_insert (buffer, &insert_pos, text, -1);
	g_free (text);

	/* Select the moved text. */
	gtk_text_buffer_get_iter_at_mark (buffer, &selection_iter_start, start_mark);
	gtk_text_buffer_delete_mark (buffer, start_mark);
	start_mark = NULL;

	gtk_text_buffer_select_range (buffer, &selection_iter_start, &insert_pos);

	if (trailing_newline_inserted)
	{
		remove_trailing_newline (buffer);
	}

	gtk_text_buffer_end_user_action (buffer);
}

static void
gtk_source_view_move_lines (GtkSourceView *view,
			    gboolean       down)
{
	GtkTextBuffer *buffer;

	if (!gtk_text_view_get_editable (GTK_TEXT_VIEW (view)))
	{
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	/* Split the two cases, otherwise the code is messier. */
	if (down)
	{
		move_lines_down (buffer);
	}
	else
	{
		move_lines_up (buffer);
	}

	gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view),
					    gtk_text_buffer_get_insert (buffer));
}

static gboolean
do_smart_backspace (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkTextBuffer *buffer;
	gboolean default_editable;
	GtkTextIter insert;
	GtkTextIter end;
	GtkTextIter leading_end;
	guint visual_column;
	gint indent_width;

	buffer = GTK_TEXT_BUFFER (priv->source_buffer);
	default_editable = gtk_text_view_get_editable (GTK_TEXT_VIEW (view));

	if (gtk_text_buffer_get_selection_bounds (buffer, &insert, &end))
	{
		return FALSE;
	}

	/* If the line isn't empty up to our cursor, ignore. */
	_gtk_source_iter_get_leading_spaces_end_boundary (&insert, &leading_end);
	if (gtk_text_iter_compare (&leading_end, &insert) < 0)
	{
		return FALSE;
	}

	visual_column = gtk_source_view_get_visual_column (view, &insert);
	indent_width = priv->indent_width;
	if (indent_width <= 0)
	{
		indent_width = priv->tab_width;
	}

	g_return_val_if_fail (indent_width > 0, FALSE);

	/* If the cursor is not at an indent_width boundary, it probably means
	 * that we want to adjust the spaces.
	 */
	if ((gint)visual_column < indent_width)
	{
		return FALSE;
	}

	if ((visual_column % indent_width) == 0)
	{
		guint target_column;

		g_assert ((gint)visual_column >= indent_width);
		target_column = visual_column - indent_width;

		while (gtk_source_view_get_visual_column (view, &insert) > target_column)
		{
			gtk_text_iter_backward_cursor_position (&insert);
		}

		gtk_text_buffer_begin_user_action (buffer);
		gtk_text_buffer_delete_interactive (buffer, &insert, &end, default_editable);
		while (gtk_source_view_get_visual_column (view, &insert) < target_column)
		{
			if (!gtk_text_buffer_insert_interactive (buffer, &insert, " ", 1, default_editable))
			{
				break;
			}
		}
		gtk_text_buffer_end_user_action (buffer);

		return TRUE;
	}

	return FALSE;
}

static gboolean
do_ctrl_backspace (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkTextBuffer *buffer;
	GtkTextIter insert;
	GtkTextIter end;
	GtkTextIter leading_end;
	gboolean default_editable;

	buffer = GTK_TEXT_BUFFER (priv->source_buffer);
	default_editable = gtk_text_view_get_editable (GTK_TEXT_VIEW (view));

	if (gtk_text_buffer_get_selection_bounds (buffer, &insert, &end))
	{
		return FALSE;
	}

	/* A <Control>BackSpace at the beginning of the line should only move us to the
	 * end of the previous line. Anything more than that is non-obvious because it requires
	 * looking in a position other than where the cursor is.
	 */
	if ((gtk_text_iter_get_line_offset (&insert) == 0) &&
	    (gtk_text_iter_get_line (&insert) > 0))
	{
		gtk_text_iter_backward_cursor_position (&insert);
		gtk_text_buffer_delete_interactive (buffer, &insert, &end, default_editable);
		return TRUE;
	}

	/* If only leading whitespaces are on the left of the cursor, delete up
	 * to the zero position.
	 */
	_gtk_source_iter_get_leading_spaces_end_boundary (&insert, &leading_end);
	if (gtk_text_iter_compare (&insert, &leading_end) <= 0)
	{
		gtk_text_iter_set_line_offset (&insert, 0);
		gtk_text_buffer_delete_interactive (buffer, &insert, &end, default_editable);
		return TRUE;
	}

	return FALSE;
}

static gboolean
gtk_source_view_key_pressed (GtkSourceView         *view,
                             guint                  key,
                             guint                  keycode,
                             guint                  state,
                             GtkEventControllerKey *controller)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	gboolean retval = GDK_EVENT_PROPAGATE;
	GtkTextBuffer *buf;
	GtkTextIter cur;
	GtkTextMark *mark;
	gint64 insertion_count;
	guint modifiers;
	gboolean editable;

	g_assert (GTK_SOURCE_IS_VIEW (view));
	g_assert (GTK_IS_EVENT_CONTROLLER (controller));

	buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	insertion_count = _gtk_source_buffer_get_insertion_count (priv->source_buffer);
	editable = gtk_text_view_get_editable (GTK_TEXT_VIEW (view));

	/* Be careful when testing for modifier state equality:
	 * caps lock, num lock,etc need to be taken into account */
	modifiers = gtk_accelerator_get_default_mod_mask ();

	mark = gtk_text_buffer_get_insert (buf);
	gtk_text_buffer_get_iter_at_mark (buf, &cur, mark);

	if (editable)
	{
		GdkEvent *event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (controller));

		g_assert (event != NULL);
		g_assert (gdk_event_get_event_type (event) == GDK_KEY_PRESS);

		priv->im_commit_text[0] = 0;
		priv->im_commit_len = 0;

		/* We need to query the input-method first as we might be using
		 * ibus or similar with pinyin, etc.
		 */
		if (gtk_text_view_im_context_filter_keypress (GTK_TEXT_VIEW (view), event))
		{
			gunichar expected = gdk_keyval_to_unicode (key);
			char keyval_str[8];

			retval = GDK_EVENT_STOP;
			keyval_str[g_unichar_to_utf8 (expected, keyval_str)] = '\0';

			if (!g_str_equal (priv->im_commit_text, keyval_str))
			{
				priv->im_commit_text[0] = 0;
				priv->im_commit_len = 0;

				return retval;
			}
		}

		priv->im_commit_text[0] = 0;
		priv->im_commit_len = 0;
	}

	if (editable &&
	    priv->auto_indent &&
	    priv->indenter != NULL &&
	    gtk_source_indenter_is_trigger (priv->indenter, view, &cur, state, key))
	{
		gunichar expected;
		gboolean did_insert;

		/* To make this work as close to how GTK will commit text to the
		 * buffer as possible, we deliver the event information to the input
		 * method who then might commit the text to the GtkSourceBuffer. To
		 * do anything else would put some difficult work on the indenter to
		 * translate GDK keyvals into text which is incredibly complicated
		 * when input methods are in play.
		 *
		 * Since we don't have direct access to the input method, we check
		 * the location of the input and see if it changed after filtering
		 * the key press event.
		 *
		 * If we detect that something was actually inserted (and not filtered
		 * into a compose sequence or similar) then we ask the indenter to
		 * indent the line (starting from the location directly after the
		 * inserted character).
		 */
		expected = gdk_keyval_to_unicode (key);

		/* If our change count incremented, then something was inserted.
		 * The change count is not incremented if only pre-edit changed.
		 */
		did_insert = insertion_count != _gtk_source_buffer_get_insertion_count (priv->source_buffer);

		/* If we didn't filter with
                 * gtk_text_view_im_context_filter_keypress(), then GTK would
                 * have inserted a \n for Return/KP_Enter if it's key-press
                 * handler would have fired.
		 * We need to emulate that.
		 */
		gtk_text_buffer_begin_user_action (buf);

		if (key == GDK_KEY_Return || key == GDK_KEY_KP_Enter)
		{
			gtk_text_buffer_get_iter_at_mark (buf, &cur, mark);
			gtk_text_buffer_insert (buf, &cur, "\n", 1);
			did_insert = TRUE;
			expected = '\n';
		}

		gtk_text_buffer_end_user_action (buf);

		/* If we inserted something, then we are free to query the
		 * indenter, so long as what was entered is what we expected
		 * to insert based on the keyval. Some input-methods may not
		 * do that, such as < getting inserted as 《.
		 */
		if (did_insert)
		{
			GtkTextIter prev;
			gunichar ch;

			gtk_text_buffer_get_iter_at_mark (buf, &prev, mark);
			gtk_text_iter_backward_char (&prev);
			ch = gtk_text_iter_get_char (&prev);

			if (ch == expected)
			{
				gtk_text_buffer_begin_user_action (buf);
				gtk_text_buffer_get_iter_at_mark (buf, &cur, mark);
				gtk_source_indenter_indent (priv->indenter, view, &cur);
				gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view), mark);
				gtk_text_buffer_end_user_action (buf);
			}
		}

		return GDK_EVENT_STOP;
	}

	if (priv->enable_snippets &&
	    _gtk_source_view_snippets_key_pressed (&priv->snippets, key, keycode, state))
	{
		return GDK_EVENT_STOP;
	}

	/* if tab or shift+tab:
	 * with shift+tab key is GDK_ISO_Left_Tab (yay! on win32 and mac too!)
	 */
	if ((key == GDK_KEY_Tab || key == GDK_KEY_KP_Tab || key == GDK_KEY_ISO_Left_Tab) &&
	    ((state & modifiers) == 0 ||
	     (state & modifiers) == GDK_SHIFT_MASK) &&
	    editable &&
	    gtk_text_view_get_accepts_tab (GTK_TEXT_VIEW (view)))
	{
		GtkTextIter s, e;
		gboolean has_selection;

		has_selection = gtk_text_buffer_get_selection_bounds (buf, &s, &e);

		if (priv->indent_on_tab)
		{
			/* shift+tab: always unindent */
			if (state & GDK_SHIFT_MASK)
			{
				_gtk_source_buffer_save_and_clear_selection (GTK_SOURCE_BUFFER (buf));
				gtk_source_view_unindent_lines (view, &s, &e);
				_gtk_source_buffer_restore_selection (GTK_SOURCE_BUFFER (buf));
				return GDK_EVENT_STOP;
			}

			/* tab: if we have a selection which spans one whole line
			 * or more, we mass indent, if the selection spans less then
			 * the full line just replace the text with \t
			 */
			if (has_selection &&
			    ((gtk_text_iter_starts_line (&s) && gtk_text_iter_ends_line (&e)) ||
			     (gtk_text_iter_get_line (&s) != gtk_text_iter_get_line (&e))))
			{
				_gtk_source_buffer_save_and_clear_selection (GTK_SOURCE_BUFFER (buf));
				gtk_source_view_indent_lines (view, &s, &e);
				_gtk_source_buffer_restore_selection (GTK_SOURCE_BUFFER (buf));
				return GDK_EVENT_STOP;
			}
		}

		insert_tab_or_spaces (view, &s, &e);
		return GDK_EVENT_STOP;
	}

	if (key == GDK_KEY_BackSpace)
	{
		if ((state & modifiers) == 0)
		{
			if (priv->smart_backspace && do_smart_backspace (view))
			{
				return GDK_EVENT_STOP;
			}
		}
		else if ((state & modifiers) == GDK_CONTROL_MASK)
		{
			if (do_ctrl_backspace (view))
			{
				return GDK_EVENT_STOP;
			}
		}
	}

	return retval;
}

static gboolean
gtk_source_view_key_released (GtkSourceView         *view,
                              guint                  key,
                              guint                  keycode,
                              guint                  state,
                              GtkEventControllerKey *controller)
{
	g_assert (GTK_SOURCE_IS_VIEW (view));
	g_assert (GTK_IS_EVENT_CONTROLLER_KEY (controller));

	if (gtk_text_view_get_editable (GTK_TEXT_VIEW (view)))
	{
		GdkEvent *event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (controller));

		g_assert (event != NULL);
		g_assert (gdk_event_get_event_type (event) == GDK_KEY_RELEASE);

		if (gtk_text_view_im_context_filter_keypress (GTK_TEXT_VIEW (view), event))
		{
			return GDK_EVENT_STOP;
		}
	}

	return GDK_EVENT_PROPAGATE;
}

static void
gtk_source_view_update_adjustment_connections (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkAdjustment *vadj, *hadj;
	GtkAdjustment *old_vadj = priv->vadj;
	GtkAdjustment *old_hadj = priv->hadj;

	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (view));
	hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (view));

	if (old_vadj == vadj && old_hadj == hadj)
		return;

	if (old_vadj != NULL)
	{
		g_signal_handlers_disconnect_by_func (old_vadj,
		                                      G_CALLBACK (gtk_source_view_adjustment_changed),
		                                      view);
		g_object_unref (old_vadj);
	}

	if (old_hadj != NULL)
	{
		g_signal_handlers_disconnect_by_func (old_hadj,
		                                      G_CALLBACK (gtk_source_view_adjustment_changed),
		                                      view);
		g_object_unref (old_hadj);
	}

	if (vadj != NULL)
	{
		g_signal_connect_swapped (vadj, "value-changed",
		                          G_CALLBACK (gtk_source_view_adjustment_changed),
		                          view);
		priv->vadj = g_object_ref (vadj);
	}
	else
	{
		priv->vadj = NULL;
	}

	if (hadj != NULL)
	{
		g_signal_connect_swapped (hadj, "value-changed",
		                          G_CALLBACK (gtk_source_view_adjustment_changed),
		                          view);
		priv->hadj = g_object_ref (hadj);
	}
	else
	{
		priv->hadj = NULL;
	}
}

static void
gtk_source_view_adj_property_changed  (GObject    *object,
                                       GParamSpec *pspec,
                                       gpointer    user_data)
{
	GtkSourceView *view = GTK_SOURCE_VIEW (object);

	gtk_source_view_update_adjustment_connections (view);
}

static void
gtk_source_view_adjustment_changed (GtkSourceView *view,
                                    GtkAdjustment *adjustment)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_assert (GTK_SOURCE_IS_VIEW (view));
	g_assert (GTK_IS_ADJUSTMENT (adjustment));

	_gtk_source_view_assistants_update_all (&priv->assistants);
}

static void
gtk_source_view_clicked (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_assert (GTK_SOURCE_IS_VIEW (view));

	_gtk_source_view_assistants_hide_all (&priv->assistants);
}

/**
 * gtk_source_view_get_auto_indent:
 * @view: a #GtkSourceView.
 *
 * Returns whether auto-indentation of text is enabled.
 *
 * Returns: %TRUE if auto indentation is enabled.
 */
gboolean
gtk_source_view_get_auto_indent (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	return priv->auto_indent;
}

/**
 * gtk_source_view_set_auto_indent:
 * @view: a #GtkSourceView.
 * @enable: whether to enable auto indentation.
 *
 * If %TRUE auto-indentation of text is enabled.
 *
 * When Enter is pressed to create a new line, the auto-indentation inserts the
 * same indentation as the previous line. This is **not** a
 * "smart indentation" where an indentation level is added or removed depending
 * on the context.
 */
void
gtk_source_view_set_auto_indent (GtkSourceView *view,
                                 gboolean       enable)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	enable = enable != FALSE;

	if (priv->auto_indent != enable)
	{
		priv->auto_indent = enable;
		g_object_notify_by_pspec (G_OBJECT (view),
		                          properties [PROP_AUTO_INDENT]);
	}
}

/**
 * gtk_source_view_get_insert_spaces_instead_of_tabs:
 * @view: a #GtkSourceView.
 *
 * Returns whether when inserting a tabulator character it should
 * be replaced by a group of space characters.
 *
 * Returns: %TRUE if spaces are inserted instead of tabs.
 */
gboolean
gtk_source_view_get_insert_spaces_instead_of_tabs (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	return priv->insert_spaces;
}

/**
 * gtk_source_view_set_insert_spaces_instead_of_tabs:
 * @view: a #GtkSourceView.
 * @enable: whether to insert spaces instead of tabs.
 *
 * If %TRUE a tab key pressed is replaced by a group of space characters.
 *
 * Of course it is still possible to insert a real `\t` programmatically with the
 * [class@Gtk.TextBuffer] API.
 */
void
gtk_source_view_set_insert_spaces_instead_of_tabs (GtkSourceView *view,
                                                   gboolean       enable)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	enable = enable != FALSE;

	if (priv->insert_spaces != enable)
	{
		priv->insert_spaces = enable;
		g_object_notify_by_pspec (G_OBJECT (view),
		                          properties [PROP_INSERT_SPACES]);
	}
}

/**
 * gtk_source_view_get_indent_on_tab:
 * @view: a #GtkSourceView.
 *
 * Returns whether when the tab key is pressed the current selection
 * should get indented instead of replaced with the `\t` character.
 *
 * Return value: %TRUE if the selection is indented when tab is pressed.
 */
gboolean
gtk_source_view_get_indent_on_tab (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	return priv->indent_on_tab;
}

/**
 * gtk_source_view_set_indent_on_tab:
 * @view: a #GtkSourceView.
 * @enable: whether to indent a block when tab is pressed.
 *
 * If %TRUE, when the tab key is pressed when several lines are selected, the
 * selected lines are indented of one level instead of being replaced with a `\t`
 * character. Shift+Tab unindents the selection.
 *
 * If the first or last line is not selected completely, it is also indented or
 * unindented.
 *
 * When the selection doesn't span several lines, the tab key always replaces
 * the selection with a normal `\t` character.
 */
void
gtk_source_view_set_indent_on_tab (GtkSourceView *view,
                                   gboolean       enable)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	enable = enable != FALSE;

	if (priv->indent_on_tab != enable)
	{
		priv->indent_on_tab = enable;
		g_object_notify_by_pspec (G_OBJECT (view),
		                          properties [PROP_INDENT_ON_TAB]);
	}
}

static void
insert_rgba_at_iter (GtkSourceView *view,
                     const GdkRGBA *rgba,
                     GtkTextIter   *iter)
{
	GtkTextBuffer *buffer;
	gchar *str;

	if (rgba->alpha == 1.0)
	{
		str = g_strdup_printf ("#%02X%02X%02X",
				       (gint)(rgba->red * 256),
				       (gint)(rgba->green * 256),
				       (gint)(rgba->blue * 256));
	}
	else
	{
		str = gdk_rgba_to_string (rgba);
	}

	buffer = gtk_text_iter_get_buffer (iter);
	gtk_text_buffer_insert (buffer, iter, str, -1);
	gtk_text_buffer_place_cursor (buffer, iter);

	/*
	 * FIXME: Check if the iter is inside a selection
	 * If it is, remove the selection and then insert at
	 * the cursor position - Paolo
	 */

	g_free (str);
}

static gboolean
gtk_source_view_rgba_drop (GtkDropTarget *dest,
                           const GValue  *value,
                           int            x,
                           int            y,
                           GtkSourceView *view)
{
	const GdkRGBA *rgba;
	GtkTextIter pos;

	rgba = g_value_get_boxed (value);
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view),
	                                       GTK_TEXT_WINDOW_WIDGET,
	                                       x, y, &x, &y);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &pos, x, y);
	insert_rgba_at_iter (view, rgba, &pos);

	return TRUE;
}

/**
 * gtk_source_view_get_highlight_current_line:
 * @view: a #GtkSourceView.
 *
 * Returns whether the current line is highlighted.
 *
 * Return value: %TRUE if the current line is highlighted.
 */
gboolean
gtk_source_view_get_highlight_current_line (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	return priv->highlight_current_line;
}

/**
 * gtk_source_view_set_highlight_current_line:
 * @view: a #GtkSourceView.
 * @highlight: whether to highlight the current line.
 *
 * If @highlight is %TRUE the current line will be highlighted.
 */
void
gtk_source_view_set_highlight_current_line (GtkSourceView *view,
                                            gboolean       highlight)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	highlight = highlight != FALSE;

	if (priv->highlight_current_line != highlight)
	{
		priv->highlight_current_line = highlight;

		gtk_source_view_queue_draw (view);

		g_object_notify_by_pspec (G_OBJECT (view),
		                          properties [PROP_HIGHLIGHT_CURRENT_LINE]);
	}
}

/**
 * gtk_source_view_get_show_right_margin:
 * @view: a #GtkSourceView.
 *
 * Returns whether a right margin is displayed.
 *
 * Return value: %TRUE if the right margin is shown.
 */
gboolean
gtk_source_view_get_show_right_margin (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	return priv->show_right_margin;
}

/**
 * gtk_source_view_set_show_right_margin:
 * @view: a #GtkSourceView.
 * @show: whether to show a right margin.
 *
 * If %TRUE a right margin is displayed.
 */
void
gtk_source_view_set_show_right_margin (GtkSourceView *view,
                                       gboolean       show)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	show = show != FALSE;

	if (priv->show_right_margin != show)
	{
		priv->show_right_margin = show;

		gtk_source_view_queue_draw (view);

		g_object_notify_by_pspec (G_OBJECT (view),
		                          properties [PROP_SHOW_RIGHT_MARGIN]);
	}
}

/**
 * gtk_source_view_get_right_margin_position:
 * @view: a #GtkSourceView.
 *
 * Gets the position of the right margin in the given @view.
 *
 * Return value: the position of the right margin.
 */
guint
gtk_source_view_get_right_margin_position (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), DEFAULT_RIGHT_MARGIN_POSITION);

	return priv->right_margin_pos;
}

/**
 * gtk_source_view_set_right_margin_position:
 * @view: a #GtkSourceView.
 * @pos: the width in characters where to position the right margin.
 *
 * Sets the position of the right margin in the given @view.
 */
void
gtk_source_view_set_right_margin_position (GtkSourceView *view,
                                           guint          pos)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));
	g_return_if_fail (1 <= pos && pos <= MAX_RIGHT_MARGIN_POSITION);

	if (priv->right_margin_pos != pos)
	{
		priv->right_margin_pos = pos;
		priv->cached_right_margin_pos = -1;

		gtk_source_view_queue_draw (view);

		g_object_notify_by_pspec (G_OBJECT (view),
		                          properties [PROP_RIGHT_MARGIN_POSITION]);
	}
}

/**
 * gtk_source_view_set_smart_backspace:
 * @view: a #GtkSourceView.
 * @smart_backspace: whether to enable smart Backspace handling.
 *
 * When set to %TRUE, pressing the Backspace key will try to delete spaces
 * up to the previous tab stop.
 */
void
gtk_source_view_set_smart_backspace (GtkSourceView *view,
                                     gboolean       smart_backspace)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	smart_backspace = smart_backspace != FALSE;

	if (smart_backspace != priv->smart_backspace)
	{
		priv->smart_backspace = smart_backspace;
		g_object_notify_by_pspec (G_OBJECT (view),
		                          properties [PROP_SMART_BACKSPACE]);
	}
}

/**
 * gtk_source_view_get_smart_backspace:
 * @view: a #GtkSourceView.
 *
 * Returns %TRUE if pressing the Backspace key will try to delete spaces
 * up to the previous tab stop.
 *
 * Returns: %TRUE if smart Backspace handling is enabled.
 */
gboolean
gtk_source_view_get_smart_backspace (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	return priv->smart_backspace;
}

/**
 * gtk_source_view_set_smart_home_end:
 * @view: a #GtkSourceView.
 * @smart_home_end: the desired behavior among #GtkSourceSmartHomeEndType.
 *
 * Set the desired movement of the cursor when HOME and END keys
 * are pressed.
 */
void
gtk_source_view_set_smart_home_end (GtkSourceView             *view,
                                    GtkSourceSmartHomeEndType  smart_home_end)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	if (priv->smart_home_end != smart_home_end)
	{
		priv->smart_home_end = smart_home_end;
		g_object_notify_by_pspec (G_OBJECT (view),
		                          properties [PROP_SMART_HOME_END]);
	}
}

/**
 * gtk_source_view_get_smart_home_end:
 * @view: a #GtkSourceView.
 *
 * Returns a [enum@SmartHomeEndType] end value specifying
 * how the cursor will move when HOME and END keys are pressed.
 *
 * Returns: a #GtkSourceSmartHomeEndType value.
 */
GtkSourceSmartHomeEndType
gtk_source_view_get_smart_home_end (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	return priv->smart_home_end;
}

/**
 * gtk_source_view_get_visual_column:
 * @view: a #GtkSourceView.
 * @iter: a position in @view.
 *
 * Determines the visual column at @iter taking into consideration the
 * [property@View:tab-width] of @view.
 *
 * Returns: the visual column at @iter.
 */
guint
gtk_source_view_get_visual_column (GtkSourceView     *view,
                                   const GtkTextIter *iter)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkTextIter position;
	guint column;
	guint tab_width;

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), 0);
	g_return_val_if_fail (iter != NULL, 0);

	column = 0;
	tab_width = priv->tab_width;

	position = *iter;
	gtk_text_iter_set_line_offset (&position, 0);

	while (!gtk_text_iter_equal (&position, iter))
	{
		if (gtk_text_iter_get_char (&position) == '\t')
		{
			column += (tab_width - (column % tab_width));
		}
		else
		{
			++column;
		}

		/* FIXME: this does not handle invisible text correctly, but
		 * gtk_text_iter_forward_visible_cursor_position is too slow */
		if (!gtk_text_iter_forward_char (&position))
		{
			break;
		}
	}

	return column;
}

static void
update_background_pattern_color (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	if (priv->style_scheme == NULL)
	{
		priv->background_pattern_color_set = FALSE;
		return;
	}

	priv->background_pattern_color_set =
		_gtk_source_style_scheme_get_background_pattern_color (priv->style_scheme,
								       &priv->background_pattern_color);
}

static void
update_current_line_color (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	if (priv->style_scheme != NULL)
	{
		priv->current_line_background_color_set =
			_gtk_source_style_scheme_get_current_line_background_color (priv->style_scheme,
									            &priv->current_line_background_color);
		priv->current_line_number_background_color_set =
			_gtk_source_style_scheme_get_current_line_number_background_color (priv->style_scheme,
			                                                                   &priv->current_line_number_background_color);
		priv->current_line_number_color_set =
			_gtk_source_style_scheme_get_current_line_number_color (priv->style_scheme,
			                                                        &priv->current_line_number_color);
		priv->current_line_number_bold =
			_gtk_source_style_scheme_get_current_line_number_bold (priv->style_scheme);
	}
	else
	{
		priv->current_line_background_color_set = FALSE;
		priv->current_line_number_background_color_set = FALSE;
		priv->current_line_number_color_set = FALSE;
		priv->current_line_number_bold = FALSE;
	}

	/* If we failed to get a highlight-current-line color, then premix the foreground
	 * and the background to give something relatively useful (and avoid alpha-composite
	 * if we can w/ premix).
	 */
	if (!priv->current_line_background_color_set)
	{
		gboolean has_bg = FALSE;
		GdkRGBA fg, bg;

		if (priv->style_scheme != NULL)
			has_bg = _gtk_source_style_scheme_get_background_color (priv->style_scheme, &bg);

		gtk_widget_get_color (GTK_WIDGET (view), &fg);

		premix_colors (&priv->current_line_background_color, &fg, &bg, has_bg, 0.05);
		priv->current_line_background_color_set = TRUE;
	}
}

static void
update_right_margin_colors (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkWidget *widget = GTK_WIDGET (view);

	priv->right_margin_line_color_set = FALSE;
	priv->right_margin_overlay_color_set = FALSE;

	if (priv->style_scheme != NULL)
	{
		GtkSourceStyle *right_margin_style;
		GtkSourceStyle *text_style;

		right_margin_style = _gtk_source_style_scheme_get_right_margin_style (priv->style_scheme);
		text_style = gtk_source_style_scheme_get_style (priv->style_scheme, "text");

		if (right_margin_style != NULL)
		{
			char *text_background_str = NULL;
			char *color_str = NULL;
			gboolean text_background_set = FALSE;
			gboolean color_set;
			GdkRGBA color;
			GdkRGBA background;

			g_object_get (right_margin_style,
				      "foreground", &color_str,
				      "foreground-set", &color_set,
				      NULL);

			if (text_style != NULL)
			{
				g_object_get (text_style,
				              "background", &text_background_str,
				              "background-set", &text_background_set,
				              NULL);

				text_background_set = (text_background_set &&
				                       text_background_str != NULL &&
				                       gdk_rgba_parse (&background, text_background_str));
			}

			if (color_set &&
			    color_str != NULL &&
			    gdk_rgba_parse (&color, color_str))
			{
				premix_colors (&priv->right_margin_line_color, &color, &background,
				               text_background_set,
				               RIGHT_MARGIN_LINE_ALPHA / 255.0);
				priv->right_margin_line_color_set = TRUE;
			}

			g_clear_pointer (&color_str, g_free);

			g_object_get (right_margin_style,
				      "background", &color_str,
				      "background-set", &color_set,
				      NULL);

			if (color_set &&
			    color_str != NULL &&
			    gdk_rgba_parse (&color, color_str))
			{
				premix_colors (&priv->right_margin_overlay_color, &color, &background,
				               text_background_set,
				               RIGHT_MARGIN_OVERLAY_ALPHA / 255.0);
				priv->right_margin_overlay_color_set = TRUE;
			}

			g_clear_pointer (&color_str, g_free);
			g_clear_pointer (&text_background_str, g_free);
		}
	}

	if (!priv->right_margin_line_color_set)
	{
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS

		GtkStyleContext *context;
		GdkRGBA color;

		context = gtk_widget_get_style_context (widget);
		gtk_style_context_save (context);
		gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
		gtk_style_context_get_color (context, &color);
		gtk_style_context_restore (context);

		priv->right_margin_line_color = color;
		priv->right_margin_line_color.alpha = RIGHT_MARGIN_LINE_ALPHA / 255.;
		priv->right_margin_line_color_set = TRUE;

		G_GNUC_END_IGNORE_DEPRECATIONS
	}
}

static void
update_style (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	update_background_pattern_color (view);
	update_current_line_color (view);
	update_right_margin_colors (view);

	if (priv->space_drawer != NULL)
	{
		_gtk_source_space_drawer_update_color (priv->space_drawer, view);
	}

	if (priv->annotations != NULL)
	{
		_gtk_source_annotations_update_color (priv->annotations, view);
	}

	gtk_source_view_queue_draw (view);
}

static void
gtk_source_view_update_style_scheme (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	GtkTextBuffer *buffer;
	GtkSourceStyleScheme *new_scheme = NULL;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (GTK_SOURCE_IS_BUFFER (buffer))
	{
		new_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));
	}

	if (priv->style_scheme == new_scheme)
	{
		return;
	}

	if (priv->style_scheme != NULL)
	{
		_gtk_source_style_scheme_unapply (priv->style_scheme, GTK_WIDGET (view));
		_gtk_source_gutter_unapply_scheme (priv->left_gutter, priv->style_scheme);
		_gtk_source_gutter_unapply_scheme (priv->right_gutter, priv->style_scheme);
	}

	g_set_object (&priv->style_scheme, new_scheme);

	if (priv->style_scheme != NULL)
	{
		_gtk_source_style_scheme_apply (priv->style_scheme, GTK_WIDGET (view));
		_gtk_source_gutter_apply_scheme (priv->left_gutter, priv->style_scheme);
		_gtk_source_gutter_apply_scheme (priv->right_gutter, priv->style_scheme);
	}

	update_style (view);
}

static void
gtk_source_view_css_changed (GtkWidget         *widget,
                             GtkCssStyleChange *change)
{
	GtkSourceView *view = GTK_SOURCE_VIEW (widget);
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	if (GTK_WIDGET_CLASS (gtk_source_view_parent_class)->css_changed)
	{
		GTK_WIDGET_CLASS (gtk_source_view_parent_class)->css_changed (widget, change);
	}

	/* Re-set tab stops, but only if we already modified them, i.e.
	 * do nothing with good old 8-space tabs.
	 */
	if (priv->tabs_set)
	{
		set_tab_stops_internal (view);
	}

	/* Make sure the margin position is recalculated on next redraw. */
	priv->cached_right_margin_pos = -1;

	update_style (view);

	if (priv->completion != NULL)
	{
		_gtk_source_completion_css_changed (priv->completion, change);
	}

	if (priv->left_gutter != NULL)
	{
		_gtk_source_gutter_css_changed (priv->left_gutter, change);
	}

	if (priv->right_gutter != NULL)
	{
		_gtk_source_gutter_css_changed (priv->right_gutter, change);
	}
}

static MarkCategory *
mark_category_new (GtkSourceMarkAttributes *attributes,
                   gint                     priority)
{
	MarkCategory* category = g_slice_new (MarkCategory);

	category->attributes = g_object_ref (attributes);
	category->priority = priority;

	return category;
}

static void
mark_category_free (MarkCategory *category)
{
	if (category != NULL)
	{
		g_object_unref (category->attributes);
		g_slice_free (MarkCategory, category);
	}
}

/**
 * gtk_source_view_get_completion:
 * @view: a #GtkSourceView.
 *
 * Gets the [class@Completion] associated with @view.
 *
 * The returned object is guaranteed to be the same for the lifetime of @view.
 * Each `GtkSourceView` object has a different [class@Completion].
 *
 * Returns: (transfer none): the #GtkSourceCompletion associated with @view.
 */
GtkSourceCompletion *
gtk_source_view_get_completion (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), NULL);

	if (priv->completion == NULL)
	{
		priv->completion = _gtk_source_completion_new (view);
	}

	return priv->completion;
}

/**
 * gtk_source_view_get_hover:
 * @view: a #GtkSourceView.
 *
 * Gets the [class@Hover] associated with @view.
 *
 * The returned object is guaranteed to be the same for the lifetime of @view.
 * Each [class@View] object has a different [class@Hover].
 *
 * Returns: (transfer none): a #GtkSourceHover associated with @view.
 */
GtkSourceHover *
gtk_source_view_get_hover (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), NULL);

	if (priv->hover == NULL)
	{
		priv->hover = _gtk_source_hover_new (view);
	}

	return priv->hover;
}

/**
 * gtk_source_view_get_gutter:
 * @view: a #GtkSourceView.
 * @window_type: the gutter window type.
 *
 * Returns the [class@Gutter] object associated with @window_type for @view.
 *
 * Only %GTK_TEXT_WINDOW_LEFT and %GTK_TEXT_WINDOW_RIGHT are supported,
 * respectively corresponding to the left and right gutter. The line numbers
 * and mark category icons are rendered in the left gutter.
 *
 * Returns: (transfer none): the #GtkSourceGutter.
 */
GtkSourceGutter *
gtk_source_view_get_gutter (GtkSourceView     *view,
                            GtkTextWindowType  window_type)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), NULL);
	g_return_val_if_fail (window_type == GTK_TEXT_WINDOW_LEFT ||
	                      window_type == GTK_TEXT_WINDOW_RIGHT, NULL);

	if (window_type == GTK_TEXT_WINDOW_LEFT)
	{
		if (priv->left_gutter == NULL)
		{
			priv->left_gutter = _gtk_source_gutter_new (window_type, view);
			gtk_text_view_set_gutter (GTK_TEXT_VIEW (view),
			                          GTK_TEXT_WINDOW_LEFT,
			                          GTK_WIDGET (priv->left_gutter));
			if (priv->style_scheme != NULL)
			{
				_gtk_source_style_scheme_apply (priv->style_scheme,
				                                GTK_WIDGET (priv->left_gutter));
			}
		}

		return priv->left_gutter;
	}
	else if (window_type == GTK_TEXT_WINDOW_RIGHT)
	{
		if (priv->right_gutter == NULL)
		{
			priv->right_gutter = _gtk_source_gutter_new (window_type, view);
			gtk_text_view_set_gutter (GTK_TEXT_VIEW (view),
			                          GTK_TEXT_WINDOW_RIGHT,
			                          GTK_WIDGET (priv->right_gutter));
			if (priv->style_scheme != NULL)
			{
				_gtk_source_style_scheme_apply (priv->style_scheme,
				                                GTK_WIDGET (priv->right_gutter));
			}
		}

		return priv->right_gutter;
	}

	g_return_val_if_reached (NULL);
}

/**
 * gtk_source_view_set_mark_attributes:
 * @view: a #GtkSourceView.
 * @category: the category.
 * @attributes: mark attributes.
 * @priority: priority of the category.
 *
 * Sets attributes and priority for the @category.
 */
void
gtk_source_view_set_mark_attributes (GtkSourceView           *view,
                                     const gchar             *category,
                                     GtkSourceMarkAttributes *attributes,
                                     gint                     priority)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	MarkCategory *mark_category;

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));
	g_return_if_fail (category != NULL);
	g_return_if_fail (GTK_SOURCE_IS_MARK_ATTRIBUTES (attributes));
	g_return_if_fail (priority >= 0);

	mark_category = mark_category_new (attributes, priority);
	g_hash_table_replace (priv->mark_categories,
	                      g_strdup (category),
	                      mark_category);
}

/**
 * gtk_source_view_get_mark_attributes:
 * @view: a #GtkSourceView.
 * @category: the category.
 * @priority: place where priority of the category will be stored.
 *
 * Gets attributes and priority for the @category.
 *
 * Returns: (transfer none): #GtkSourceMarkAttributes for the @category.
 * The object belongs to @view, so it must not be unreffed.
 */
GtkSourceMarkAttributes *
gtk_source_view_get_mark_attributes (GtkSourceView *view,
                                     const gchar   *category,
                                     gint          *priority)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);
	MarkCategory *mark_category;

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), NULL);
	g_return_val_if_fail (category != NULL, NULL);

	mark_category = g_hash_table_lookup (priv->mark_categories,
	                                     category);

	if (mark_category != NULL)
	{
		if (priority != NULL)
		{
			*priority = mark_category->priority;
		}

		return mark_category->attributes;
	}

	return NULL;
}

/**
 * gtk_source_view_set_background_pattern:
 * @view: a #GtkSourceView.
 * @background_pattern: the #GtkSourceBackgroundPatternType.
 *
 * Set if and how the background pattern should be displayed.
 */
void
gtk_source_view_set_background_pattern (GtkSourceView                  *view,
                                        GtkSourceBackgroundPatternType  background_pattern)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	if (priv->background_pattern != background_pattern)
	{
		priv->background_pattern = background_pattern;

		gtk_source_view_queue_draw (view);

		g_object_notify_by_pspec (G_OBJECT (view),
		                          properties [PROP_BACKGROUND_PATTERN]);
	}
}

/**
 * gtk_source_view_get_background_pattern:
 * @view: a #GtkSourceView
 *
 * Returns the #GtkSourceBackgroundPatternType specifying if and how
 * the background pattern should be displayed for this @view.
 *
 * Returns: the #GtkSourceBackgroundPatternType.
 */
GtkSourceBackgroundPatternType
gtk_source_view_get_background_pattern (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE);

	return priv->background_pattern;
}

/**
 * gtk_source_view_get_space_drawer:
 * @view: a #GtkSourceView.
 *
 * Gets the [class@SpaceDrawer] associated with @view.
 *
 * The returned object is guaranteed to be the same for the lifetime of @view.
 * Each [class@View] object has a different [class@SpaceDrawer].
 *
 * Returns: (transfer none): the #GtkSourceSpaceDrawer associated with @view.
 */
GtkSourceSpaceDrawer *
gtk_source_view_get_space_drawer (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), NULL);

	return priv->space_drawer;
}

/**
 * gtk_source_view_get_annotations:
 * @view: a #GtkSourceView.
 *
 * Gets the [class@Annotations] associated with @view.
 *
 * The returned object is guaranteed to be the same for the lifetime of @view.
 * Each [class@View] object has a different [class@Annotations].
 *
 * Returns: (transfer none): the #GtkSourceAnnotations associated with @view.
 *
 * Since: 5.18
 */
GtkSourceAnnotations *
gtk_source_view_get_annotations (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), NULL);

	if (priv->hover == NULL)
	{
		priv->hover = _gtk_source_hover_new (view);
	}

	return priv->annotations;
}

static void
gtk_source_view_queue_draw (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	gtk_widget_queue_draw (GTK_WIDGET (view));

	if (priv->left_gutter != NULL)
	{
		_gtk_source_gutter_queue_draw (priv->left_gutter);
	}

	if (priv->right_gutter != NULL)
	{
		_gtk_source_gutter_queue_draw (priv->right_gutter);
	}
}

void
_gtk_source_view_add_assistant (GtkSourceView      *view,
                                GtkSourceAssistant *assistant)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));
	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT (assistant));

	_gtk_source_view_assistants_add (&priv->assistants, assistant);
}

void
_gtk_source_view_remove_assistant (GtkSourceView      *view,
                                   GtkSourceAssistant *assistant)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));
	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT (assistant));

	_gtk_source_view_assistants_remove (&priv->assistants, assistant);
}

static gchar *
get_line_prefix (const GtkTextIter *iter)
{
	GtkTextIter begin;
	GString *str;

	g_assert (iter != NULL);

	if (gtk_text_iter_starts_line (iter))
	{
		return NULL;
	}

	begin = *iter;
	gtk_text_iter_set_line_offset (&begin, 0);

	str = g_string_new (NULL);

	do
	{
		gunichar c = gtk_text_iter_get_char (&begin);

		switch (c)
		{
		case '\t':
		case ' ':
			g_string_append_unichar (str, c);
			break;

		default:
			g_string_append_c (str, ' ');
			break;
		}
	}
	while (gtk_text_iter_forward_char (&begin) &&
	       (gtk_text_iter_compare (&begin, iter) < 0));

	return g_string_free (str, FALSE);
}

static void
gtk_source_view_real_push_snippet (GtkSourceView    *view,
                                   GtkSourceSnippet *snippet,
                                   GtkTextIter      *location)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_assert (GTK_SOURCE_IS_VIEW (view));
	g_assert (GTK_SOURCE_IS_SNIPPET (snippet));
	g_assert (location != NULL);

	_gtk_source_view_snippets_push (&priv->snippets, snippet, location);
}

/**
 * gtk_source_view_push_snippet:
 * @view: a #GtkSourceView
 * @snippet: a #GtkSourceSnippet
 * @location: (nullable): a #GtkTextIter or %NULL for the cursor position
 *
 * Inserts a new snippet at @location
 *
 * If another snippet was already active, it will be paused and the new
 * snippet will become active. Once the focus positions of @snippet have
 * been exhausted, editing will return to the previous snippet.
 */
void
gtk_source_view_push_snippet (GtkSourceView    *view,
                              GtkSourceSnippet *snippet,
                              GtkTextIter      *location)
{
	GtkSourceSnippetContext *context;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gboolean use_spaces;
	gchar *prefix;
	gint tab_width;

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (location == NULL)
	{
		gtk_text_buffer_get_iter_at_mark (buffer,
		                                  &iter,
		                                  gtk_text_buffer_get_insert (buffer));
		location = &iter;
	}

	g_return_if_fail (gtk_text_iter_get_buffer (location) == buffer);

	context = gtk_source_snippet_get_context (snippet);

	use_spaces = gtk_source_view_get_insert_spaces_instead_of_tabs (view);
	gtk_source_snippet_context_set_use_spaces (context, use_spaces);

	tab_width = gtk_source_view_get_tab_width (view);
	gtk_source_snippet_context_set_tab_width (context, tab_width);

	prefix = get_line_prefix (location);
	gtk_source_snippet_context_set_line_prefix (context, prefix);
	g_free (prefix);

	g_signal_emit (view, signals [PUSH_SNIPPET], 0, snippet, location);
}

/**
 * gtk_source_view_get_enable_snippets:
 * @view: a #GtkSourceView
 *
 * Gets the [property@View:enable-snippets] property.
 *
 * If %TRUE, matching snippets found in the [class@SnippetManager]
 * may be expanded when the user presses Tab after a word in the [class@View].
 *
 * Returns: %TRUE if enabled
 */
gboolean
gtk_source_view_get_enable_snippets (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	return priv->enable_snippets;
}

/**
 * gtk_source_view_set_enable_snippets:
 * @view: a #GtkSourceView
 * @enable_snippets: if snippets should be enabled
 *
 * Sets the [property@View:enable-snippets] property.
 *
 * If @enable_snippets is %TRUE, matching snippets found in the
 * [class@SnippetManager] may be expanded when the user presses
 * Tab after a word in the [class@View].
 */
void
gtk_source_view_set_enable_snippets (GtkSourceView *view,
                                     gboolean       enable_snippets)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	enable_snippets = !!enable_snippets;

	if (enable_snippets != priv->enable_snippets)
	{
		priv->enable_snippets = enable_snippets;
		_gtk_source_view_snippets_pop_all (&priv->snippets);
		g_object_notify_by_pspec (G_OBJECT (view),
		                          properties [PROP_ENABLE_SNIPPETS]);
	}
}

/**
 * gtk_source_view_get_indenter:
 * @view: a #GtkSourceView
 *
 * Gets the [property@View:indenter] property.
 *
 * Returns: (transfer none) (nullable): a #GtkSourceIndenter or %NULL
 */
GtkSourceIndenter *
gtk_source_view_get_indenter (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), NULL);

	return priv->indenter;
}

/**
 * gtk_source_view_set_indenter:
 * @view: a #GtkSourceView
 * @indenter: (nullable): a #GtkSourceIndenter or %NULL
 *
 * Sets the indenter for @view to @indenter.
 *
 * Note that the indenter will not be used unless #GtkSourceView:auto-indent
 * has been set to %TRUE.
 */
void
gtk_source_view_set_indenter (GtkSourceView     *view,
                              GtkSourceIndenter *indenter)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));
	g_return_if_fail (!indenter || GTK_SOURCE_IS_INDENTER (indenter));

	if (g_set_object (&priv->indenter, indenter))
	{
		if (priv->indenter == NULL)
		{
			priv->indenter = _gtk_source_indenter_internal_new ();
		}

		g_object_notify_by_pspec (G_OBJECT (view), properties [PROP_INDENTER]);
	}
}

gboolean
_gtk_source_view_get_current_line_background (GtkSourceView *view,
                                              GdkRGBA       *rgba)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	if (rgba != NULL)
	{
		*rgba = priv->current_line_background_color;
	}

	return priv->current_line_background_color_set;
}

gboolean
_gtk_source_view_get_current_line_number_background (GtkSourceView *view,
                                                     GdkRGBA       *rgba)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	if (rgba != NULL)
	{
		*rgba = priv->current_line_number_background_color;
	}

	return priv->current_line_number_background_color_set;
}

gboolean
_gtk_source_view_get_current_line_number_color (GtkSourceView *view,
                                                GdkRGBA       *rgba)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	if (rgba != NULL)
	{
		*rgba = priv->current_line_number_color;
	}

	return priv->current_line_number_color_set;
}

gboolean
_gtk_source_view_get_current_line_number_bold (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	return priv->current_line_number_bold;
}

gboolean
_gtk_source_view_has_snippet (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);

	return priv->snippets.queue.length > 0;
}

void
_gtk_source_view_hide_completion (GtkSourceView *view)
{
	GtkSourceViewPrivate *priv = gtk_source_view_get_instance_private (view);

	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	if (priv->completion != NULL)
	{
		gtk_source_completion_hide (priv->completion);
	}
}
