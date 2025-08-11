/*
 * This file is part of GtkSourceView
 *
 * Copyright 2000, 2001 Chema Celorio
 * Copyright 2003  Gustavo Giráldez
 * Copyright 2004  Red Hat, Inc.
 * Copyright 2001-2007  Paolo Maggi
 * Copyright 2008  Paolo Maggi, Paolo Borelli and Yevgen Muntyan
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

#include <string.h>
#include <time.h>

#include "gtksourceprintcompositor.h"
#include "gtksourceview.h"
#include "gtksourcebuffer.h"
#include "gtksourcebuffer-private.h"
#include "gtksourcetrace.h"

/**
 * GtkSourcePrintCompositor:
 * 
 * Compose a [class@Buffer] for printing.
 *
 * The `GtkSourcePrintCompositor` object is used to compose a [class@Buffer]
 * for printing. You can set various configuration options to customize the
 * printed output. `GtkSourcePrintCompositor` is designed to be used with the
 * high-level printing API of gtk+, i.e. [class@Gtk.PrintOperation].
 *
 * The margins specified in this object are the layout margins: they define the
 * blank space bordering the printed area of the pages. They must not be
 * confused with the "print margins", i.e. the parts of the page that the
 * printer cannot print on, defined in the [class@Gtk.PageSetup] objects. If the
 * specified layout margins are smaller than the "print margins", the latter
 * ones are used as a fallback by the `GtkSourcePrintCompositor` object, so that
 * the printed area is not clipped.
 */

#define DEFAULT_TAB_WIDTH 		8
#define MAX_TAB_WIDTH			32

#define DEFAULT_FONT_NAME   "Monospace 10"

/* 5 mm */
#define NUMBERS_TEXT_SEPARATION convert_from_mm (5, GTK_UNIT_POINTS)

#define HEADER_FOOTER_SIZE_FACTOR 2.2
#define SEPARATOR_SPACING_FACTOR  0.4
#define SEPARATOR_LINE_WIDTH      0.7

/* Number of pages paginated on each invocation of the paginate() method. */
#define PAGINATION_CHUNK_SIZE 3

typedef enum _PaginatorState
{
	/* Initial state: properties can be changed only when the paginator
	   is in the INIT state */
	INIT,

	/* Paginating state: paginator goes in this state when the paginate
	   function is called for the first time */
	PAGINATING,

	/* Done state: paginator goes in this state when the entire document
	   has been paginated */
	DONE
} PaginatorState;

typedef struct
{
	GtkSourceBuffer         *buffer;

	/* Properties */
	guint			 tab_width;
	GtkWrapMode		 wrap_mode;
	gboolean                 highlight_syntax;
	guint                    print_line_numbers;

	PangoFontDescription    *body_font;
	PangoFontDescription    *line_numbers_font;
	PangoFontDescription    *header_font;
	PangoFontDescription    *footer_font;

	/* Paper size, stored in points */
	gdouble                  paper_width;
	gdouble                  paper_height;

	/* These are stored in mm */
	gdouble                  margin_top;
	gdouble                  margin_bottom;
	gdouble                  margin_left;
	gdouble                  margin_right;

	gboolean                 print_header;
	gboolean                 print_footer;

	gchar                   *header_format_left;
	gchar                   *header_format_center;
	gchar                   *header_format_right;
	gboolean                 header_separator;
	gchar                   *footer_format_left;
	gchar                   *footer_format_center;
	gchar                   *footer_format_right;
	gboolean                 footer_separator;

	/* State */
	PaginatorState           state;

	GArray                  *pages; /* pages[i] contains the begin offset
	                                   of i-th  */

	guint                    paginated_lines;
	gint                     n_pages;
	gint                     current_page;

	/* Stored in points */
	gdouble                  header_height;
	gdouble                  footer_height;
	gdouble                  line_numbers_width;
	gdouble                  line_numbers_height;

	gdouble                  footer_font_descent;

	/* layout objects */
	PangoLayout             *layout;
	PangoLayout             *line_numbers_layout;
	PangoLayout             *header_layout;
	PangoLayout             *footer_layout;

	gdouble                  real_margin_top;
	gdouble                  real_margin_bottom;
	gdouble                  real_margin_left;
	gdouble                  real_margin_right;

	gdouble                  page_margin_top;
	gdouble                  page_margin_left;

	PangoLanguage           *language; /* must not be freed */

	GtkTextMark             *pagination_mark;

	GHashTable              *ignored_tags;

#ifdef GTK_SOURCE_PROFILER_ENABLED
	gint64                   pagination_timer;
#endif
} GtkSourcePrintCompositorPrivate;

enum
{
	PROP_0,
	PROP_BUFFER,
	PROP_TAB_WIDTH,
	PROP_WRAP_MODE,
	PROP_HIGHLIGHT_SYNTAX,
	PROP_PRINT_LINE_NUMBERS,
	PROP_PRINT_HEADER,
	PROP_PRINT_FOOTER,
	PROP_BODY_FONT_NAME,
	PROP_LINE_NUMBERS_FONT_NAME,
	PROP_HEADER_FONT_NAME,
	PROP_FOOTER_FONT_NAME,
	PROP_N_PAGES,
	N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourcePrintCompositor, gtk_source_print_compositor, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

#define MM_PER_INCH 25.4
#define POINTS_PER_INCH 72

static gdouble
convert_to_mm (gdouble len,
               GtkUnit unit)
{
	switch (unit)
	{
		case GTK_UNIT_MM:
			return len;

		case GTK_UNIT_INCH:
			return len * MM_PER_INCH;

		default:
		case GTK_UNIT_PIXEL:
			g_warning ("Unsupported unit");
			/* Fall through */

		case GTK_UNIT_POINTS:
			return len * (MM_PER_INCH / POINTS_PER_INCH);
    	}
}

static gdouble
convert_from_mm (gdouble len,
                 GtkUnit unit)
{
	switch (unit)
	{
		case GTK_UNIT_MM:
			return len;

		case GTK_UNIT_INCH:
			return len / MM_PER_INCH;

		default:
		case GTK_UNIT_PIXEL:
			g_warning ("Unsupported unit");
			/* Fall through */

		case GTK_UNIT_POINTS:
			return len / (MM_PER_INCH / POINTS_PER_INCH);
	}
}

static void
gtk_source_print_compositor_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
	GtkSourcePrintCompositor *compositor = GTK_SOURCE_PRINT_COMPOSITOR (object);
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	switch (prop_id)
	{
		case PROP_BUFFER:
			g_value_set_object (value, priv->buffer);
			break;
		case PROP_TAB_WIDTH:
			g_value_set_uint (value,
					  gtk_source_print_compositor_get_tab_width (compositor));
			break;
		case PROP_WRAP_MODE:
			g_value_set_enum (value,
					  gtk_source_print_compositor_get_wrap_mode (compositor));
			break;
		case PROP_HIGHLIGHT_SYNTAX:
			g_value_set_boolean (value,
					     gtk_source_print_compositor_get_highlight_syntax (compositor));
			break;
		case PROP_PRINT_LINE_NUMBERS:
			g_value_set_uint (value,
					  gtk_source_print_compositor_get_print_line_numbers (compositor));
			break;
		case PROP_PRINT_HEADER:
			g_value_set_boolean (value,
					     gtk_source_print_compositor_get_print_header (compositor));
			break;
		case PROP_PRINT_FOOTER:
			g_value_set_boolean (value,
					     gtk_source_print_compositor_get_print_footer (compositor));
			break;
		case PROP_BODY_FONT_NAME:
			g_value_set_string (value,
					    gtk_source_print_compositor_get_body_font_name (compositor));
			break;
		case PROP_LINE_NUMBERS_FONT_NAME:
			g_value_set_string (value,
					    gtk_source_print_compositor_get_line_numbers_font_name (compositor));
			break;
		case PROP_HEADER_FONT_NAME:
			g_value_set_string (value,
					    gtk_source_print_compositor_get_header_font_name (compositor));
			break;
		case PROP_FOOTER_FONT_NAME:
			g_value_set_string (value,
					    gtk_source_print_compositor_get_footer_font_name (compositor));
			break;
		case PROP_N_PAGES:
			g_value_set_int (value,
					 gtk_source_print_compositor_get_n_pages (compositor));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_print_compositor_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
	GtkSourcePrintCompositor *compositor = GTK_SOURCE_PRINT_COMPOSITOR (object);
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	switch (prop_id)
	{
		case PROP_BUFFER:
			priv->buffer = GTK_SOURCE_BUFFER (g_value_dup_object (value));
			if (priv->buffer)
			{
				GtkTextTag *tag = _gtk_source_buffer_get_bracket_match_tag (priv->buffer);

				if (tag)
				{
					gtk_source_print_compositor_ignore_tag (compositor, tag);
				}
			}
			break;
		case PROP_TAB_WIDTH:
			gtk_source_print_compositor_set_tab_width (compositor,
								   g_value_get_uint (value));
			break;
		case PROP_WRAP_MODE:
			gtk_source_print_compositor_set_wrap_mode (compositor,
								   g_value_get_enum (value));
			break;
		case PROP_HIGHLIGHT_SYNTAX:
			gtk_source_print_compositor_set_highlight_syntax (compositor,
									  g_value_get_boolean (value));
			break;
		case PROP_PRINT_LINE_NUMBERS:
			gtk_source_print_compositor_set_print_line_numbers (compositor,
									    g_value_get_uint (value));
			break;
		case PROP_PRINT_HEADER:
			gtk_source_print_compositor_set_print_header (compositor, g_value_get_boolean (value));
			break;

		case PROP_PRINT_FOOTER:
			gtk_source_print_compositor_set_print_footer (compositor, g_value_get_boolean (value));
			break;
		case PROP_BODY_FONT_NAME:
			gtk_source_print_compositor_set_body_font_name (compositor,
									g_value_get_string (value));
			break;
		case PROP_LINE_NUMBERS_FONT_NAME:
			gtk_source_print_compositor_set_line_numbers_font_name (compositor,
										g_value_get_string (value));
			break;
		case PROP_HEADER_FONT_NAME:
			gtk_source_print_compositor_set_header_font_name (compositor,
									g_value_get_string (value));
			break;
		case PROP_FOOTER_FONT_NAME:
			gtk_source_print_compositor_set_footer_font_name (compositor,
									g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_print_compositor_finalize (GObject *object)
{
	GtkSourcePrintCompositor *compositor = GTK_SOURCE_PRINT_COMPOSITOR (object);
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_clear_pointer (&priv->ignored_tags, g_hash_table_unref);

	if (priv->pages != NULL)
		g_array_free (priv->pages, TRUE);

	if (priv->layout != NULL)
		g_object_unref (priv->layout);

	if (priv->line_numbers_layout != NULL)
		g_object_unref (priv->line_numbers_layout);

	if (priv->header_layout != NULL)
		g_object_unref (priv->header_layout);

	if (priv->footer_layout != NULL)
		g_object_unref (priv->footer_layout);

	pango_font_description_free (priv->body_font);

	if (priv->line_numbers_font != NULL)
		pango_font_description_free (priv->line_numbers_font);

	if (priv->header_font != NULL)
		pango_font_description_free (priv->header_font);

	if (priv->footer_font != NULL)
		pango_font_description_free (priv->footer_font);

	g_free (priv->header_format_left);
	g_free (priv->header_format_right);
	g_free (priv->header_format_center);
	g_free (priv->footer_format_left);
	g_free (priv->footer_format_right);
	g_free (priv->footer_format_center);

	G_OBJECT_CLASS (gtk_source_print_compositor_parent_class)->finalize (object);
}

static void
gtk_source_print_compositor_dispose (GObject *object)
{
	GtkSourcePrintCompositor *compositor = GTK_SOURCE_PRINT_COMPOSITOR (object);
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_clear_object (&priv->buffer);

	G_OBJECT_CLASS (gtk_source_print_compositor_parent_class)->dispose (object);
}

static void
gtk_source_print_compositor_class_init (GtkSourcePrintCompositorClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = gtk_source_print_compositor_get_property;
	object_class->set_property = gtk_source_print_compositor_set_property;
	object_class->finalize = gtk_source_print_compositor_finalize;
	object_class->dispose = gtk_source_print_compositor_dispose;

	/**
	 * GtkSourcePrintCompositor:buffer:
	 *
	 * The [class@Buffer] object to print.
	 */
	properties[PROP_BUFFER] =
		g_param_spec_object ("buffer",
		                     "Source Buffer",
		                     "The GtkSourceBuffer object to print",
		                     GTK_SOURCE_TYPE_BUFFER,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_CONSTRUCT_ONLY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourcePrintCompositor:tab-width:
	 *
	 * Width of a tab character expressed in spaces.
	 *
	 * The value of this property cannot be changed anymore after the first
	 * call to the [method@PrintCompositor.paginate] function.
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
	 * GtkSourcePrintCompositor:wrap-mode:
	 *
	 * Whether to wrap lines never, at word boundaries, or at character boundaries.
	 *
 	 * The value of this property cannot be changed anymore after the first
	 * call to the [method@PrintCompositor.paginate] function.
	 */
	properties [PROP_WRAP_MODE] =
		g_param_spec_enum ("wrap-mode",
		                   "Wrap Mode",
		                   "",
		                   GTK_TYPE_WRAP_MODE,
		                   GTK_WRAP_NONE,
		                   (G_PARAM_READWRITE |
		                    G_PARAM_EXPLICIT_NOTIFY |
		                    G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourcePrintCompositor:highlight-syntax:
	 *
	 * Whether to print the document with highlighted syntax.
	 *
 	 * The value of this property cannot be changed anymore after the first
	 * call to the [method@PrintCompositor.paginate] function.
	 */
	properties [PROP_HIGHLIGHT_SYNTAX] =
		g_param_spec_boolean ("highlight-syntax",
		                      "Highlight Syntax",
		                      "",
		                      TRUE,
		                      (G_PARAM_READWRITE |
		                       G_PARAM_EXPLICIT_NOTIFY |
		                       G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourcePrintCompositor:print-line-numbers:
	 *
	 * Interval of printed line numbers. 
	 * 
	 * If this property is set to 0 no numbers will be printed.
	 * If greater than 0, a number will be printed every "print-line-numbers"
	 * lines (i.e. 1 will print all line numbers).
	 *
	 * The value of this property cannot be changed anymore after the first
	 * call to the [method@PrintCompositor.paginate] function.
	 */
	properties [PROP_PRINT_LINE_NUMBERS] =
		g_param_spec_uint ("print-line-numbers",
		                   "Print Line Numbers",
		                   "",
		                   0, 100, 1,
		                   (G_PARAM_READWRITE |
		                    G_PARAM_EXPLICIT_NOTIFY |
		                    G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourcePrintCompositor:print-header:
	 *
	 * Whether to print a header in each page.
	 *
	 * Note that by default the header format is unspecified, and if it is
	 * unspecified the header will not be printed, regardless of the value of
	 * this property.
	 *
	 * The value of this property cannot be changed anymore after the first
	 * call to the [method@PrintCompositor.paginate] function.
	 */
	properties [PROP_PRINT_HEADER] =
		g_param_spec_boolean ("print-header",
		                      "Print Header",
		                      "",
		                      FALSE,
		                      (G_PARAM_READWRITE |
		                       G_PARAM_EXPLICIT_NOTIFY |
		                       G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourcePrintCompositor:print-footer:
	 *
	 * Whether to print a footer in each page.
	 *
	 * Note that by default the footer format is unspecified, and if it is
	 * unspecified the footer will not be printed, regardless of the value of
	 * this property.
	 *
	 * The value of this property cannot be changed anymore after the first
	 * call to the [method@PrintCompositor.paginate] function.
	 */
	properties [PROP_PRINT_FOOTER] =
		g_param_spec_boolean ("print-footer",
		                      "Print Footer",
		                      "",
		                      FALSE,
		                      (G_PARAM_READWRITE |
		                       G_PARAM_EXPLICIT_NOTIFY |
		                       G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourcePrintCompositor:body-font-name:
	 *
	 * Name of the font used for the text body.
	 *
	 * Accepted values are strings representing a font description Pango can understand.
	 * (e.g. &quot;Monospace 10&quot;). See [func@Pango.FontDescription.from_string]
	 * for a description of the format of the string representation.
	 *
	 * The value of this property cannot be changed anymore after the first
	 * call to the [method@PrintCompositor.paginate] function.
	 */
	properties [PROP_BODY_FONT_NAME] =
		g_param_spec_string ("body-font-name",
		                     "Body Font Name",
		                     "",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourcePrintCompositor:line-numbers-font-name:
	 *
	 * Name of the font used to print line numbers on the left margin.
	 * If this property is unspecified, the text body font is used.
	 *
	 * Accepted values are strings representing a font description Pango can understand.
	 * (e.g. &quot;Monospace 10&quot;). See [func@Pango.FontDescription.from_string]
	 * for a description of the format of the string representation.
	 *
	 * The value of this property cannot be changed anymore after the first
	 * call to the [method@PrintCompositor.paginate] function.
	 */
	properties [PROP_LINE_NUMBERS_FONT_NAME] =
		g_param_spec_string ("line-numbers-font-name",
		                     "Line Numbers Font Name",
		                     "",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourcePrintCompositor:header-font-name:
	 *
	 * Name of the font used to print page header.
	 * If this property is unspecified, the text body font is used.
	 *
	 * Accepted values are strings representing a font description Pango can understand.
	 * (e.g. &quot;Monospace 10&quot;). See [func@Pango.FontDescription.from_string]
	 * for a description of the format of the string representation.
	 *
	 * The value of this property cannot be changed anymore after the first
	 * call to the [method@PrintCompositor.paginate] function.
	 */
	properties [PROP_HEADER_FONT_NAME] =
		g_param_spec_string ("header-font-name",
		                     "Header Font Name",
		                     "",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourcePrintCompositor:footer-font-name:
	 *
	 * Name of the font used to print page footer.
	 * If this property is unspecified, the text body font is used.
	 *
	 * Accepted values are strings representing a font description Pango can understand.
	 * (e.g. &quot;Monospace 10&quot;). See [func@Pango.FontDescription.from_string]
	 * for a description of the format of the string representation.
	 *
	 * The value of this property cannot be changed anymore after the first
	 * call to the [method@PrintCompositor.paginate] function.
	 */
	properties [PROP_FOOTER_FONT_NAME] =
		g_param_spec_string ("footer-font-name",
		                     "Footer Font Name",
		                     "",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourcePrintCompositor:n-pages:
	 *
	 * The number of pages in the document or <code>-1</code> if the
 	 * document has not been completely paginated.
 	 */
	properties [PROP_N_PAGES] =
		g_param_spec_int ("n-pages",
		                  "Number of pages",
		                  "",
		                  -1, G_MAXINT, -1,
		                  (G_PARAM_READABLE |
		                   G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_print_compositor_init (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	priv->buffer = NULL;
	priv->ignored_tags = g_hash_table_new (NULL, NULL);

	priv->tab_width = DEFAULT_TAB_WIDTH;
	priv->wrap_mode = GTK_WRAP_NONE;
	priv->highlight_syntax = TRUE;
	priv->print_line_numbers = 0;

	priv->body_font = pango_font_description_from_string (DEFAULT_FONT_NAME);
	priv->line_numbers_font = NULL;
	priv->header_font = NULL;
	priv->footer_font = NULL;

	priv->paper_width = 0.0;
	priv->paper_height = 0.0;

	priv->margin_top = 0.0;
	priv->margin_bottom = 0.0;
	priv->margin_left = 0.0;
	priv->margin_right = 0.0;

	priv->print_header = FALSE;
	priv->print_footer = FALSE;

	priv->header_format_left = NULL;
	priv->header_format_center = NULL;
	priv->header_format_right = NULL;
	priv->header_separator = FALSE;

	priv->footer_format_left = NULL;
	priv->footer_format_center = NULL;
	priv->footer_format_right = NULL;
	priv->footer_separator = FALSE;

	priv->state = INIT;

	priv->pages = NULL;

	priv->paginated_lines = 0;
	priv->n_pages = -1;
	priv->current_page = -1;

	priv->layout = NULL;
	priv->line_numbers_layout = NULL;

	priv->language = gtk_get_default_language ();

	/* Negative values mean uninitialized */
	priv->header_height = -1.0;
	priv->footer_height = -1.0;
	priv->line_numbers_width = -1.0;
	priv->line_numbers_height = -1.0;
}

/**
 * gtk_source_print_compositor_new:
 * @buffer: the #GtkSourceBuffer to print.
 *
 * Creates a new print compositor that can be used to print @buffer.
 *
 * Return value: a new print compositor object.
 **/
GtkSourcePrintCompositor *
gtk_source_print_compositor_new (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	return g_object_new (GTK_SOURCE_TYPE_PRINT_COMPOSITOR,
			     "buffer", buffer,
			     NULL);
}

/**
 * gtk_source_print_compositor_new_from_view:
 * @view: a #GtkSourceView to get configuration from.
 *
 * Creates a new print compositor that can be used to print the buffer
 * associated with @view.
 *
 * This constructor sets some configuration properties to make the
 * printed output match @view as much as possible.  The properties set are
 * [property@PrintCompositor:tab-width], [property@PrintCompositor:highlight-syntax],
 * [property@PrintCompositor:wrap-mode], [property@PrintCompositor:body-font-name] and
 * [property@PrintCompositor:print-line-numbers].
 *
 * Return value: a new print compositor object.
 **/
GtkSourcePrintCompositor *
gtk_source_print_compositor_new_from_view (GtkSourceView *view)
{
	GtkSourceBuffer *buffer = NULL;
	PangoContext *pango_context;
	PangoFontDescription* font_desc;
	GtkSourcePrintCompositor *compositor;
	GtkSourcePrintCompositorPrivate *priv;

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), NULL);
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view))), NULL);

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	compositor = GTK_SOURCE_PRINT_COMPOSITOR (
			g_object_new (GTK_SOURCE_TYPE_PRINT_COMPOSITOR,
				     "buffer", buffer,
				     "tab-width", gtk_source_view_get_tab_width (view),
				     "highlight-syntax", gtk_source_buffer_get_highlight_syntax (buffer) != FALSE,
				     "wrap-mode", gtk_text_view_get_wrap_mode (GTK_TEXT_VIEW (view)),
				     "print-line-numbers", (gtk_source_view_get_show_line_numbers (view) == FALSE) ? 0 : 1,
				     NULL));
	priv = gtk_source_print_compositor_get_instance_private (compositor);

	/* Set the body font directly since the property get a name while body_font is a PangoFontDescription */
	pango_context = gtk_widget_get_pango_context (GTK_WIDGET (view));

	font_desc = pango_font_description_copy (pango_context_get_font_description (pango_context));

	g_clear_pointer (&priv->body_font, pango_font_description_free);
	priv->body_font = font_desc;

	return compositor;
}

/**
 * gtk_source_print_compositor_get_buffer:
 * @compositor: a #GtkSourcePrintCompositor.
 *
 * Gets the [class@Buffer] associated with the compositor. 
 * 
 * The returned object reference is owned by the compositor object and
 * should not be unreferenced.
 *
 * Return value: (transfer none): the #GtkSourceBuffer associated with the compositor.
 **/
GtkSourceBuffer *
gtk_source_print_compositor_get_buffer (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), NULL);

	return priv->buffer;
}

/**
 * gtk_source_print_compositor_set_tab_width:
 * @compositor: a #GtkSourcePrintCompositor.
 * @width: width of tab in characters.
 *
 * Sets the width of tabulation in characters for printed text.
 *
 * This function cannot be called anymore after the first call to the
 * [method@PrintCompositor.paginate] function.
 */
void
gtk_source_print_compositor_set_tab_width (GtkSourcePrintCompositor *compositor,
                                           guint                     width)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (width > 0 && width <= MAX_TAB_WIDTH);
	g_return_if_fail (priv->state == INIT);

	if (width == priv->tab_width)
		return;

	priv->tab_width = width;

	g_object_notify_by_pspec (G_OBJECT (compositor),
	                          properties [PROP_TAB_WIDTH]);
}

/**
 * gtk_source_print_compositor_get_tab_width:
 * @compositor: a #GtkSourcePrintCompositor.
 *
 * Returns the width of tabulation in characters for printed text.
 *
 * Return value: width of tab.
 */
guint
gtk_source_print_compositor_get_tab_width (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), DEFAULT_TAB_WIDTH);

	return priv->tab_width;
}

/**
 * gtk_source_print_compositor_set_wrap_mode:
 * @compositor: a #GtkSourcePrintCompositor.
 * @wrap_mode: a #GtkWrapMode.
 *
 * Sets the line wrapping mode for the printed text.
 *
 * This function cannot be called anymore after the first call to the
 * [method@PrintCompositor.paginate] function.
 */
void
gtk_source_print_compositor_set_wrap_mode (GtkSourcePrintCompositor *compositor,
                                           GtkWrapMode               wrap_mode)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (priv->state == INIT);

	if (wrap_mode == priv->wrap_mode)
		return;

	priv->wrap_mode = wrap_mode;

	g_object_notify_by_pspec (G_OBJECT (compositor),
	                          properties [PROP_WRAP_MODE]);
}

/**
 * gtk_source_print_compositor_get_wrap_mode:
 * @compositor: a #GtkSourcePrintCompositor.
 *
 * Gets the line wrapping mode for the printed text.
 *
 * Return value: the line wrap mode.
 */
GtkWrapMode
gtk_source_print_compositor_get_wrap_mode (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), GTK_WRAP_NONE);

	return priv->wrap_mode;
}

/**
 * gtk_source_print_compositor_set_highlight_syntax:
 * @compositor: a #GtkSourcePrintCompositor.
 * @highlight: whether syntax should be highlighted.
 *
 * Sets whether the printed text will be highlighted according to the
 * buffer rules.  Both color and font style are applied.
 *
 * This function cannot be called anymore after the first call to the
 * [method@PrintCompositor.paginate] function.
 **/
void
gtk_source_print_compositor_set_highlight_syntax (GtkSourcePrintCompositor *compositor,
                                                  gboolean                  highlight)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (priv->state == INIT);

	highlight = (highlight != FALSE);

	if (highlight == priv->highlight_syntax)
		return;

	priv->highlight_syntax = highlight;

	g_object_notify_by_pspec (G_OBJECT (compositor),
	                          properties [PROP_HIGHLIGHT_SYNTAX]);
}

/**
 * gtk_source_print_compositor_get_highlight_syntax:
 * @compositor: a #GtkSourcePrintCompositor.
 *
 * Determines whether the printed text will be highlighted according to the
 * buffer rules. 
 *
 * Note that highlighting will happen only if the buffer to print has highlighting activated.
 *
 * Return value: %TRUE if the printed output will be highlighted.
 **/
gboolean
gtk_source_print_compositor_get_highlight_syntax (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), FALSE);

	return priv->highlight_syntax;
}

/**
 * gtk_source_print_compositor_set_print_line_numbers:
 * @compositor: a #GtkSourcePrintCompositor.
 * @interval: interval for printed line numbers.
 *
 * Sets the interval for printed line numbers. 
 * 
 * If @interval is 0 no numbers will be printed. If greater than 0, a number will be
 * printed every @interval lines (i.e. 1 will print all line numbers).
 *
 * Maximum accepted value for @interval is 100.
 *
 * This function cannot be called anymore after the first call to the
 * [method@PrintCompositor.paginate] function.
 **/
void
gtk_source_print_compositor_set_print_line_numbers (GtkSourcePrintCompositor *compositor,
                                                    guint                     interval)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (priv->state == INIT);
	g_return_if_fail (interval <= 100);

	if (interval == priv->print_line_numbers)
		return;

	priv->print_line_numbers = interval;

	g_object_notify_by_pspec (G_OBJECT (compositor),
	                          properties [PROP_PRINT_LINE_NUMBERS]);
}

/**
 * gtk_source_print_compositor_set_print_header:
 * @compositor: a #GtkSourcePrintCompositor.
 * @print: %TRUE if you want the header to be printed.
 *
 * Sets whether you want to print a header in each page.
 * 
 * The header consists of three pieces of text and an optional line
 * separator, configurable with [method@PrintCompositor.set_header_format].
 *
 * Note that by default the header format is unspecified, and if it's
 * empty it will not be printed, regardless of this setting.
 *
 * This function cannot be called anymore after the first call to the
 * [method@PrintCompositor.paginate] function.
 **/
void
gtk_source_print_compositor_set_print_header (GtkSourcePrintCompositor *compositor,
                                              gboolean                  print)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (priv->state == INIT);

	print = (print != FALSE);

	if (print == priv->print_header)
		return;

	priv->print_header = print;

	g_object_notify_by_pspec (G_OBJECT (compositor),
	                          properties [PROP_PRINT_HEADER]);
}

/**
 * gtk_source_print_compositor_get_print_header:
 * @compositor: a #GtkSourcePrintCompositor.
 *
 * Determines if a header is set to be printed for each page. 
 * 
 * A header will be printed if this function returns %TRUE
 * **and** some format strings have been specified
 * with [method@PrintCompositor.set_header_format].
 *
 * Return value: %TRUE if the header is set to be printed.
 **/
gboolean
gtk_source_print_compositor_get_print_header (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), FALSE);

	return priv->print_header;
}

/**
 * gtk_source_print_compositor_set_print_footer:
 * @compositor: a #GtkSourcePrintCompositor.
 * @print: %TRUE if you want the footer to be printed.
 *
 * Sets whether you want to print a footer in each page.
 * 
 * The footer consists of three pieces of text and an optional line
 * separator, configurable with
 * [method@PrintCompositor.set_footer_format].
 *
 * Note that by default the footer format is unspecified, and if it's
 * empty it will not be printed, regardless of this setting.
 *
 * This function cannot be called anymore after the first call to the
 * [method@PrintCompositor.paginate] function.
 **/
void
gtk_source_print_compositor_set_print_footer (GtkSourcePrintCompositor *compositor,
                                              gboolean                  print)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (priv->state == INIT);

	print = (print != FALSE);

	if (print == priv->print_footer)
		return;

	priv->print_footer = print;

	g_object_notify_by_pspec (G_OBJECT (compositor),
	                          properties [PROP_PRINT_FOOTER]);
}

/**
 * gtk_source_print_compositor_get_print_footer:
 * @compositor: a #GtkSourcePrintCompositor.
 *
 * Determines if a footer is set to be printed for each page. 
 * 
 * A footer will be printed if this function returns %TRUE
 * **and** some format strings have been specified
 * with [method@PrintCompositor.set_footer_format].
 *
 * Return value: %TRUE if the footer is set to be printed.
 **/
gboolean
gtk_source_print_compositor_get_print_footer (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), FALSE);

	return priv->print_footer;
}

/**
 * gtk_source_print_compositor_set_header_format:
 * @compositor: a #GtkSourcePrintCompositor.
 * @separator: %TRUE if you want a separator line to be printed.
 * @left: (nullable): a format string to print on the left of the header.
 * @center: (nullable): a format string to print on the center of the header.
 * @right: (nullable): a format string to print on the right of the header.
 *
 * Sets strftime like header format strings, to be printed on the
 * left, center and right of the top of each page. 
 * 
 * The strings may include strftime(3) codes which will be expanded at print time.
 * A subset of strftime() codes are accepted, see [method@GLib.DateTime.format]
 * for more details on the accepted format specifiers.
 * Additionally the following format specifiers are accepted:
 *
 * - #N: the page number
 * - #Q: the page count.
 *
 * @separator specifies if a solid line should be drawn to separate
 * the header from the document text.
 *
 * If %NULL is given for any of the three arguments, that particular
 * string will not be printed.
 *
 * For the header to be printed, in
 * addition to specifying format strings, you need to enable header
 * printing with [method@PrintCompositor.set_print_header].
 *
 * This function cannot be called anymore after the first call to the
 * [method@PrintCompositor.paginate] function.
 **/
void
gtk_source_print_compositor_set_header_format (GtkSourcePrintCompositor *compositor,
                                               gboolean                  separator,
                                               const gchar              *left,
                                               const gchar              *center,
                                               const gchar              *right)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (priv->state == INIT);

	/* FIXME: validate given strings? */
	g_free (priv->header_format_left);
	g_free (priv->header_format_center);
	g_free (priv->header_format_right);

	priv->header_separator = separator;

	priv->header_format_left = g_strdup (left);
	priv->header_format_center = g_strdup (center);
	priv->header_format_right = g_strdup (right);
}

/**
 * gtk_source_print_compositor_set_footer_format:
 * @compositor: a #GtkSourcePrintCompositor.
 * @separator: %TRUE if you want a separator line to be printed.
 * @left: (nullable): a format string to print on the left of the footer.
 * @center: (nullable): a format string to print on the center of the footer.
 * @right: (nullable): a format string to print on the right of the footer.
 *
 * See [method@PrintCompositor.set_header_format] for more information
 * about the parameters.
 **/
void
gtk_source_print_compositor_set_footer_format (GtkSourcePrintCompositor *compositor,
                                               gboolean                  separator,
                                               const gchar              *left,
                                               const gchar              *center,
                                               const gchar              *right)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (priv->state == INIT);

	/* FIXME: validate given strings? */
	g_free (priv->footer_format_left);
	g_free (priv->footer_format_center);
	g_free (priv->footer_format_right);

	priv->footer_separator = separator;

	priv->footer_format_left = g_strdup (left);
	priv->footer_format_center = g_strdup (center);
	priv->footer_format_right = g_strdup (right);
}

/**
 * gtk_source_print_compositor_get_print_line_numbers:
 * @compositor: a #GtkSourcePrintCompositor.
 *
 * Returns the interval used for line number printing.
 *
 * If the value is 0, no line numbers will be printed. The default value is
 * 1 (i.e. numbers printed in all lines).
 *
 * Return value: the interval of printed line numbers.
 **/
guint
gtk_source_print_compositor_get_print_line_numbers (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), 0);

	return priv->print_line_numbers;
}

static gboolean
set_font_description_from_name (GtkSourcePrintCompositor  *compositor,
                                PangoFontDescription     **font,
                                const gchar               *font_name)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	PangoFontDescription *new;

	if (font_name != NULL)
		new = pango_font_description_from_string (font_name);
	else
	{
		g_return_val_if_fail (priv->body_font != NULL, FALSE);
		new = pango_font_description_copy (priv->body_font);
	}

	if (*font == NULL || !pango_font_description_equal (*font, new))
	{
		if (*font != NULL)
			pango_font_description_free (*font);
		*font = new;

		return TRUE;
	}
	else
	{
		pango_font_description_free (new);

		return FALSE;
	}
}

/**
 * gtk_source_print_compositor_set_body_font_name:
 * @compositor: a #GtkSourcePrintCompositor.
 * @font_name: the name of the default font for the body text.
 *
 * Sets the default font for the printed text.
 *
 * @font_name should be a
 * string representation of a font description Pango can understand.
 * (e.g. &quot;Monospace 10&quot;). See [func@Pango.FontDescription.from_string]
 * for a description of the format of the string representation.
 *
 * This function cannot be called anymore after the first call to the
 * [method@PrintCompositor.paginate] function.
 */
void
gtk_source_print_compositor_set_body_font_name (GtkSourcePrintCompositor *compositor,
                                                const gchar              *font_name)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (font_name != NULL);
	g_return_if_fail (priv->state == INIT);

	if (set_font_description_from_name (compositor,
					    &priv->body_font,
					    font_name))
	{
		g_object_notify_by_pspec (G_OBJECT (compositor),
		                          properties [PROP_BODY_FONT_NAME]);
	}
}

/**
 * gtk_source_print_compositor_get_body_font_name:
 * @compositor: a #GtkSourcePrintCompositor.
 *
 * Returns the name of the font used to print the text body. 
 * 
 * The returned string must be freed with g_free().
 *
 * Return value: a new string containing the name of the font used to print the
 * text body.
 */
gchar *
gtk_source_print_compositor_get_body_font_name (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), NULL);

	return pango_font_description_to_string (priv->body_font);
}

/**
 * gtk_source_print_compositor_set_line_numbers_font_name:
 * @compositor: a #GtkSourcePrintCompositor.
 * @font_name: (nullable): the name of the font for line numbers, or %NULL.
 *
 * Sets the font for printing line numbers on the left margin.
 * 
 * If %NULL is supplied, the default font (i.e. the one being used for the
 * text) will be used instead.
 *
 * @font_name should be a
 * string representation of a font description Pango can understand.
 * (e.g. &quot;Monospace 10&quot;). See [func@Pango.FontDescription.from_string]
 * for a description of the format of the string representation.
 *
 * This function cannot be called anymore after the first call to the
 * [method@PrintCompositor.paginate] function.
 */
void
gtk_source_print_compositor_set_line_numbers_font_name (GtkSourcePrintCompositor *compositor,
                                                        const gchar              *font_name)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (font_name != NULL);
	g_return_if_fail (priv->state == INIT);

	if (set_font_description_from_name (compositor,
					    &priv->line_numbers_font,
					    font_name))
	{
		g_object_notify_by_pspec (G_OBJECT (compositor),
		                          properties [PROP_LINE_NUMBERS_FONT_NAME]);
	}
}

/**
 * gtk_source_print_compositor_get_line_numbers_font_name:
 * @compositor: a #GtkSourcePrintCompositor.
 *
 * Returns the name of the font used to print line numbers on the left margin.
 *
 * The returned string must be freed with g_free().
 *
 * Return value: a new string containing the name of the font used to print
 * line numbers on the left margin.
 */
gchar *
gtk_source_print_compositor_get_line_numbers_font_name (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), NULL);

	if (priv->line_numbers_font == NULL)
	{
		g_return_val_if_fail (priv->body_font != NULL, NULL);
		priv->line_numbers_font = pango_font_description_copy (priv->body_font);
	}

	return pango_font_description_to_string (priv->line_numbers_font);
}

/**
 * gtk_source_print_compositor_set_header_font_name:
 * @compositor: a #GtkSourcePrintCompositor.
 * @font_name: (nullable): the name of the font for header text, or %NULL.
 *
 * Sets the font for printing the page header. 
 *
 * If %NULL is supplied, the default font (i.e. the one being used for the
 * text) will be used instead.
 *
 * @font_name should be a
 * string representation of a font description Pango can understand.
 * (e.g. &quot;Monospace 10&quot;). See [func@Pango.FontDescription.from_string]
 * for a description of the format of the string representation.
 *
 * This function cannot be called anymore after the first call to the
 * [method@PrintCompositor.paginate] function.
 */
void
gtk_source_print_compositor_set_header_font_name (GtkSourcePrintCompositor *compositor,
                                                  const gchar              *font_name)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (font_name != NULL);
	g_return_if_fail (priv->state == INIT);

	if (set_font_description_from_name (compositor,
					    &priv->header_font,
					    font_name))

	{
		g_object_notify_by_pspec (G_OBJECT (compositor),
		                          properties [PROP_HEADER_FONT_NAME]);
	}
}

/**
 * gtk_source_print_compositor_get_header_font_name:
 * @compositor: a #GtkSourcePrintCompositor.
 *
 * Returns the name of the font used to print the page header.
 *
 * The returned string must be freed with g_free().
 *
 * Return value: a new string containing the name of the font used to print
 * the page header.
 */
gchar *
gtk_source_print_compositor_get_header_font_name (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), NULL);

	if (priv->header_font == NULL)
	{
		g_return_val_if_fail (priv->body_font != NULL, NULL);
		priv->header_font = pango_font_description_copy (priv->body_font);
	}

	return pango_font_description_to_string (priv->header_font);
}

/**
 * gtk_source_print_compositor_set_footer_font_name:
 * @compositor: a #GtkSourcePrintCompositor.
 * @font_name: (nullable): the name of the font for the footer text, or %NULL.
 *
 * Sets the font for printing the page footer. 
 * 
 * If %NULL is supplied, the default font (i.e. the one being used for the
 * text) will be used instead.
 *
 * @font_name should be a
 * string representation of a font description Pango can understand.
 * (e.g. &quot;Monospace 10&quot;). See [func@Pango.FontDescription.from_string]
 * for a description of the format of the string representation.
 *
 * This function cannot be called anymore after the first call to the
 * [method@PrintCompositor.paginate] function.
 */
void
gtk_source_print_compositor_set_footer_font_name (GtkSourcePrintCompositor *compositor,
                                                  const gchar              *font_name)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (font_name != NULL);
	g_return_if_fail (priv->state == INIT);

	if (set_font_description_from_name (compositor,
					    &priv->footer_font,
					    font_name))

	{
		g_object_notify_by_pspec (G_OBJECT (compositor),
		                          properties [PROP_FOOTER_FONT_NAME]);
	}
}

/**
 * gtk_source_print_compositor_get_footer_font_name:
 * @compositor: a #GtkSourcePrintCompositor.
 *
 * Returns the name of the font used to print the page footer.
 *
 * The returned string must be freed with g_free().
 *
 * Return value: a new string containing the name of the font used to print
 * the page footer.
 */
gchar *
gtk_source_print_compositor_get_footer_font_name (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), NULL);

	if (priv->footer_font == NULL)
	{
		g_return_val_if_fail (priv->body_font != NULL, NULL);
		priv->footer_font = pango_font_description_copy (priv->body_font);
	}

	return pango_font_description_to_string (priv->footer_font);
}

/**
 * gtk_source_print_compositor_set_top_margin:
 * @compositor: a #GtkSourcePrintCompositor.
 * @margin: the new top margin in units of @unit
 * @unit: the units for @margin
 *
 * Sets the top margin used by @compositor.
 */
void
gtk_source_print_compositor_set_top_margin (GtkSourcePrintCompositor *compositor,
                                            gdouble                   margin,
                                            GtkUnit                   unit)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));

	priv->margin_top = convert_to_mm (margin, unit);
}

/**
 * gtk_source_print_compositor_get_top_margin:
 * @compositor: a #GtkSourcePrintCompositor.
 * @unit: the unit for the return value.
 *
 * Gets the top margin in units of @unit.
 *
 * Return value: the top margin.
 */
gdouble
gtk_source_print_compositor_get_top_margin (GtkSourcePrintCompositor *compositor,
                                            GtkUnit                   unit)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), 0);

	return convert_from_mm (priv->margin_top, unit);
}

/**
 * gtk_source_print_compositor_set_bottom_margin:
 * @compositor: a #GtkSourcePrintCompositor.
 * @margin: the new bottom margin in units of @unit.
 * @unit: the units for @margin.
 *
 * Sets the bottom margin used by @compositor.
 */
void
gtk_source_print_compositor_set_bottom_margin (GtkSourcePrintCompositor *compositor,
                                               gdouble                   margin,
                                               GtkUnit                   unit)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));

	priv->margin_bottom = convert_to_mm (margin, unit);
}

/**
 * gtk_source_print_compositor_get_bottom_margin:
 * @compositor: a #GtkSourcePrintCompositor.
 * @unit: the unit for the return value.
 *
 * Gets the bottom margin in units of @unit.
 *
 * Return value: the bottom margin.
 */
gdouble
gtk_source_print_compositor_get_bottom_margin (GtkSourcePrintCompositor *compositor,
                                               GtkUnit                   unit)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), 0);

	return convert_from_mm (priv->margin_bottom, unit);
}

/**
 * gtk_source_print_compositor_set_left_margin:
 * @compositor: a #GtkSourcePrintCompositor.
 * @margin: the new left margin in units of @unit.
 * @unit: the units for @margin.
 *
 * Sets the left margin used by @compositor.
 */
void
gtk_source_print_compositor_set_left_margin (GtkSourcePrintCompositor *compositor,
                                             gdouble                   margin,
                                             GtkUnit                   unit)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));

	priv->margin_left = convert_to_mm (margin, unit);
}

/**
 * gtk_source_print_compositor_get_left_margin:
 * @compositor: a #GtkSourcePrintCompositor.
 * @unit: the unit for the return value.
 *
 * Gets the left margin in units of @unit.
 *
 * Return value: the left margin
 */
gdouble
gtk_source_print_compositor_get_left_margin (GtkSourcePrintCompositor *compositor,
                                             GtkUnit                   unit)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), 0);

	return convert_from_mm (priv->margin_left, unit);
}

/**
 * gtk_source_print_compositor_set_right_margin:
 * @compositor: a #GtkSourcePrintCompositor.
 * @margin: the new right margin in units of @unit.
 * @unit: the units for @margin.
 *
 * Sets the right margin used by @compositor.
 */
void
gtk_source_print_compositor_set_right_margin (GtkSourcePrintCompositor *compositor,
                                              gdouble                   margin,
                                              GtkUnit                   unit)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));

	priv->margin_right = convert_to_mm (margin, unit);
}

/**
 * gtk_source_print_compositor_get_right_margin:
 * @compositor: a #GtkSourcePrintCompositor.
 * @unit: the unit for the return value.
 *
 * Gets the right margin in units of @unit.
 *
 * Return value: the right margin.
 */
gdouble
gtk_source_print_compositor_get_right_margin (GtkSourcePrintCompositor *compositor,
                                              GtkUnit                   unit)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), 0);

	return convert_from_mm (priv->margin_right, unit);
}

/**
 * gtk_source_print_compositor_get_n_pages:
 * @compositor: a #GtkSourcePrintCompositor.
 *
 * Returns the number of pages in the document or <code>-1</code> if the
 * document has not been completely paginated.
 *
 * Return value: the number of pages in the document or <code>-1</code> if the
 * document has not been completely paginated.
 */
gint
gtk_source_print_compositor_get_n_pages (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), -1);

	if (priv->state != DONE)
		return -1;

	return priv->n_pages;
}

/* utility functions to deal with coordinates (returns) */

static gdouble
get_text_x (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	gdouble x;

	x = priv->real_margin_left;

	if (priv->print_line_numbers)
	     x += priv->line_numbers_width + NUMBERS_TEXT_SEPARATION;

	return x;
}

static gdouble
get_text_y (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	gdouble y;

	y = priv->real_margin_top + priv->header_height;

	return y;
}

static gdouble
get_line_numbers_x (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	gdouble x;

	x = priv->real_margin_left;

	return x;
}

static gdouble
get_text_width (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	gdouble w;

	w = priv->paper_width -
	    priv->real_margin_left -
	    priv->real_margin_right;

	if (priv->print_line_numbers)
		w -= (priv->line_numbers_width + NUMBERS_TEXT_SEPARATION);

	if (w < convert_from_mm (50, GTK_UNIT_POINTS)) {
		g_warning ("Printable page width too little.");
		return convert_from_mm (50, GTK_UNIT_POINTS);
	}

	return w;
}

static gdouble
get_text_height (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	double h;

	h = priv->paper_height -
	    priv->real_margin_top -
	    priv->real_margin_bottom -
	    priv->header_height -
	    priv->footer_height;

	if (h < convert_from_mm (50, GTK_UNIT_POINTS)) {
		g_warning ("Printable page height too little.");
		return convert_from_mm (50, GTK_UNIT_POINTS);
	}

	return h;
}

static gboolean
is_header_to_print (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	return (priv->print_header &&
	       ((priv->header_format_left != NULL) ||
	        (priv->header_format_center != NULL) ||
	        (priv->header_format_right != NULL)));
}

static gboolean
is_footer_to_print (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	return (priv->print_footer &&
	       ((priv->footer_format_left != NULL) ||
	        (priv->footer_format_center != NULL) ||
	        (priv->footer_format_right != NULL)));
}

static void
set_layout_tab_width (GtkSourcePrintCompositor *compositor,
                      PangoLayout              *layout)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	gchar *str;
	gint tab_width = 0;

	str = g_strnfill (priv->tab_width, ' ');
	pango_layout_set_text (layout, str, -1);
	g_free (str);

	pango_layout_get_size (layout, &tab_width, NULL);

	if (tab_width > 0)
	{
		PangoTabArray *tab_array;

		tab_array = pango_tab_array_new (1, FALSE);

		pango_tab_array_set_tab (tab_array,
					 0,
					 PANGO_TAB_LEFT,
					 tab_width);
		pango_layout_set_tabs (layout, tab_array);

		pango_tab_array_free (tab_array);
	}
}

static void
setup_pango_layouts (GtkSourcePrintCompositor *compositor,
                     GtkPrintContext          *context)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	PangoLayout *layout;

	/* Layout for the text */
	layout = gtk_print_context_create_pango_layout (context);
	pango_layout_set_font_description (layout, priv->body_font);

	switch (priv->wrap_mode)
	{
		case GTK_WRAP_CHAR:
			pango_layout_set_wrap (layout, PANGO_WRAP_CHAR);
			break;
		case GTK_WRAP_WORD:
			pango_layout_set_wrap (layout, PANGO_WRAP_WORD);
			break;
		case GTK_WRAP_WORD_CHAR:
			pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
			break;
		case GTK_WRAP_NONE:
			/* FIXME: hack
			 * Ellipsize the paragraph when text wrapping is disabled.
			 * Another possibility would be to set the width so the text
			 * breaks into multiple lines, and paginate/render just the
			 * first one.
			 * See also Comment #23 by Owen on bug #143874.
			 */
			pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
			break;
		default:
			g_return_if_reached ();
	}

	set_layout_tab_width (compositor, layout);

	g_return_if_fail (priv->layout == NULL);
	priv->layout = layout;

	/* Layout for line numbers */
	if (priv->print_line_numbers > 0)
	{
		layout = gtk_print_context_create_pango_layout (context);

		if (priv->line_numbers_font == NULL)
			priv->line_numbers_font = pango_font_description_copy_static (priv->body_font);
		pango_layout_set_font_description (layout, priv->line_numbers_font);
		pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);

		g_return_if_fail (priv->line_numbers_layout == NULL);
		priv->line_numbers_layout = layout;
	}

	/* Layout for header */
	if (is_header_to_print (compositor))
	{
		layout = gtk_print_context_create_pango_layout (context);

		if (priv->header_font == NULL)
			priv->header_font = pango_font_description_copy_static (priv->body_font);

		pango_layout_set_font_description (layout, priv->header_font);

		g_return_if_fail (priv->header_layout == NULL);
		priv->header_layout = layout;
	}

	/* Layout for footer */
	if (is_footer_to_print (compositor))
	{
		layout = gtk_print_context_create_pango_layout (context);

		if (priv->footer_font == NULL)
			priv->footer_font = pango_font_description_copy_static (priv->body_font);

		pango_layout_set_font_description (layout, priv->footer_font);

		g_return_if_fail (priv->footer_layout == NULL);
		priv->footer_layout = layout;
	}
}

static gchar *
evaluate_format_string (GtkSourcePrintCompositor *compositor,
                        const gchar              *format)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	GDateTime *now;
	GString *eval;
	gchar *eval_str, *retval;
	gunichar ch;

	now = g_date_time_new_now_local ();

	/* analyze format string and replace the codes we know */
	eval = g_string_new_len (NULL, strlen (format));
	ch = g_utf8_get_char (format);
	while (ch != 0)
	{
		if (ch == '%')
		{
			format = g_utf8_next_char (format);
			ch = g_utf8_get_char (format);
			if (ch == 'N')
				g_string_append_printf (eval, "%d", priv->current_page + 1);
			else if (ch == 'Q')
				g_string_append_printf (eval, "%d", priv->n_pages);
			else
			{
				g_string_append_c (eval, '%');
				g_string_append_unichar (eval, ch);
			}
		}
		else
		{
			g_string_append_unichar (eval, ch);
		}

		format = g_utf8_next_char (format);
		ch = g_utf8_get_char (format);
	}

	eval_str = g_string_free (eval, FALSE);
	retval = g_date_time_format (now, eval_str);
	g_free (eval_str);

	g_date_time_unref (now);

	return retval;
}

static void
get_layout_size (PangoLayout *layout,
                 double      *width,
                 double      *height)
{
	PangoRectangle rect;

	pango_layout_get_extents (layout, NULL, &rect);

	if (width)
		*width = (double) rect.width / (double) PANGO_SCALE;

	if (height)
		*height = (double) rect.height / (double) PANGO_SCALE;
}

static gsize
get_n_digits (guint n)
{
	gsize d = 1;

	while (n /= 10)
		d++;

	return d;
}

static void
calculate_line_numbers_layout_size (GtkSourcePrintCompositor *compositor,
                                    GtkPrintContext          *context)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	gint line_count;
	gint n_digits;
	gchar *str;

	if (priv->print_line_numbers == 0)
	{
		priv->line_numbers_width = 0.0;
		priv->line_numbers_height = 0.0;

		GTK_SOURCE_PROFILER_LOG ("line_numbers_width: %f points (%f mm), line_numbers_height: %f points (%f mm)",
		                         priv->line_numbers_width,
		                         convert_to_mm (priv->line_numbers_width, GTK_UNIT_POINTS),
		                         priv->line_numbers_height,
		                         convert_to_mm (priv->line_numbers_height, GTK_UNIT_POINTS));

		return;
	}

	line_count = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (priv->buffer));
	n_digits = get_n_digits (line_count);
	str = g_strnfill (n_digits, '9');
	pango_layout_set_text (priv->line_numbers_layout, str, -1);
	g_free (str);

	get_layout_size (priv->line_numbers_layout,
			 &priv->line_numbers_width,
			 &priv->line_numbers_height);

	GTK_SOURCE_PROFILER_LOG ("line_numbers_width: %f points (%f mm), line_numbers_height: %f points (%f mm)",
	                         priv->line_numbers_width,
	                         convert_to_mm (priv->line_numbers_width, GTK_UNIT_POINTS),
	                         priv->line_numbers_height,
	                         convert_to_mm (priv->line_numbers_height, GTK_UNIT_POINTS));
}

static gdouble
calculate_header_footer_height (GtkSourcePrintCompositor *compositor,
                                GtkPrintContext          *context,
                                PangoFontDescription     *font,
                                gdouble                  *d)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	PangoContext *pango_context;
	PangoFontMetrics* font_metrics;
	gdouble ascent;
	gdouble descent;

	pango_context = gtk_print_context_create_pango_context (context);
	pango_context_set_font_description (pango_context, font);

	font_metrics = pango_context_get_metrics (pango_context,
						  font,
						  priv->language);

	ascent = (gdouble) pango_font_metrics_get_ascent (font_metrics) / PANGO_SCALE;
	descent = (gdouble) pango_font_metrics_get_descent (font_metrics) / PANGO_SCALE;

	pango_font_metrics_unref (font_metrics);
	g_object_unref (pango_context);

	if (d != NULL)
		*d = descent;

	return HEADER_FOOTER_SIZE_FACTOR * (ascent + descent);
}

static void
calculate_header_height (GtkSourcePrintCompositor *compositor,
                         GtkPrintContext          *context)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	if (!is_header_to_print(compositor))
	{
		priv->header_height = 0.0;

		GTK_SOURCE_PROFILER_LOG ("header_height: %f points (%f mm)",
		                         priv->header_height,
		                         convert_to_mm (priv->header_height, GTK_UNIT_POINTS));

		return;
	}

	g_return_if_fail (priv->header_font != NULL);

	priv->header_height = calculate_header_footer_height (compositor,
									  context,
									  priv->header_font,
									  NULL);

	GTK_SOURCE_PROFILER_LOG ("header_height: %f points (%f mm)",
	                         priv->header_height,
	                         convert_to_mm (priv->header_height, GTK_UNIT_POINTS));
}

static void
calculate_footer_height (GtkSourcePrintCompositor *compositor,
                         GtkPrintContext          *context)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	if (!is_footer_to_print (compositor))
	{
		priv->footer_height = 0.0;

		GTK_SOURCE_PROFILER_LOG ("footer_height: %f points (%f mm)",
		                         priv->footer_height,
		                         convert_to_mm (priv->footer_height, GTK_UNIT_POINTS));

		return;
	}

	if (priv->footer_font == NULL)
		priv->footer_font = pango_font_description_copy_static (priv->body_font);

	priv->footer_height = calculate_header_footer_height (compositor,
									  context,
									  priv->footer_font,
									  &priv->footer_font_descent);

	GTK_SOURCE_PROFILER_LOG ("footer_height: %f points (%f mm)",
	                         priv->footer_height,
	                         convert_to_mm (priv->footer_height, GTK_UNIT_POINTS));
}

static void
calculate_page_size_and_margins (GtkSourcePrintCompositor *compositor,
                                 GtkPrintContext          *context)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	GtkPageSetup *page_setup;

	/* calculate_line_numbers_layout_size and calculate_header_footer_height
	   functions must be called before calculate_page_size_and_margins */
	g_return_if_fail (priv->line_numbers_width >= 0.0);
	g_return_if_fail (priv->header_height >= 0.0);
	g_return_if_fail (priv->footer_height >= 0.0);

	page_setup = gtk_print_context_get_page_setup (context);

	priv->page_margin_top = gtk_page_setup_get_top_margin (page_setup, GTK_UNIT_POINTS);
	priv->page_margin_left = gtk_page_setup_get_left_margin (page_setup, GTK_UNIT_POINTS);

	/* Calculate real margins: the margins specified in the GtkPageSetup object are the "print margins".
	   they are used to determine the minimal size for the layout margins. */
	priv->real_margin_top = MAX (priv->page_margin_top,
						 convert_from_mm (priv->margin_top, GTK_UNIT_POINTS));
	priv->real_margin_bottom = MAX (gtk_page_setup_get_bottom_margin (page_setup, GTK_UNIT_POINTS),
						    convert_from_mm (priv->margin_bottom, GTK_UNIT_POINTS));
	priv->real_margin_left = MAX (priv->page_margin_left,
						  convert_from_mm (priv->margin_left, GTK_UNIT_POINTS));
	priv->real_margin_right = MAX (gtk_page_setup_get_right_margin (page_setup, GTK_UNIT_POINTS),
						   convert_from_mm (priv->margin_right, GTK_UNIT_POINTS));

	GTK_SOURCE_PROFILER_LOG ("real_margin_top: %f points (%f mm), "
	                         "real_margin_bottom: %f points (%f mm), "
	                         "real_margin_left: %f points (%f mm), "
	                         "real_margin_righ: %f points (%f mm)",
	                         priv->real_margin_top,
	                         convert_to_mm (priv->real_margin_top, GTK_UNIT_POINTS),
	                         priv->real_margin_bottom,
	                         convert_to_mm (priv->real_margin_bottom, GTK_UNIT_POINTS),
	                         priv->real_margin_left,
	                         convert_to_mm (priv->real_margin_left, GTK_UNIT_POINTS),
	                         priv->real_margin_right,
	                         convert_to_mm (priv->real_margin_right, GTK_UNIT_POINTS));

	priv->paper_width = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_POINTS);
	priv->paper_height = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_POINTS);

	GTK_SOURCE_PROFILER_LOG ("paper_width: %f points (%f mm), "
	                         "paper_heigth: %f points (%f mm), "
	                         "text_width: %f points (%f mm), "
	                         "text_height: %f points (%f mm)",
	                         priv->paper_width,
	                         convert_to_mm (priv->paper_width, GTK_UNIT_POINTS),
	                         priv->paper_height,
	                         convert_to_mm (priv->paper_height, GTK_UNIT_POINTS),
				 get_text_width (compositor),
	                         convert_to_mm (get_text_width (compositor), GTK_UNIT_POINTS),
				 get_text_height (compositor),
	                         convert_to_mm (get_text_height (compositor), GTK_UNIT_POINTS));
}

static gboolean
ignore_tag (GtkSourcePrintCompositor *compositor,
            GtkTextTag               *tag)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	return tag ? g_hash_table_contains (priv->ignored_tags, tag) : FALSE;
}

static GSList *
get_iter_attrs (GtkSourcePrintCompositor *compositor,
                GtkTextIter              *iter,
                GtkTextIter              *limit)
{
	GSList *attrs = NULL;
	GSList *tags;
	PangoAttribute *bg = NULL, *fg = NULL, *style = NULL, *ul = NULL;
	PangoAttribute *weight = NULL, *st = NULL;

	tags = gtk_text_iter_get_tags (iter);
	gtk_text_iter_forward_to_tag_toggle (iter, NULL);

	if (gtk_text_iter_compare (iter, limit) > 0)
		*iter = *limit;

	while (tags)
	{
		GtkTextTag *tag;
		gboolean bg_set, fg_set, style_set, ul_set, weight_set, st_set;

		tag = tags->data;
		tags = g_slist_delete_link (tags, tags);

		if (ignore_tag (compositor, tag))
			continue;

		g_object_get (tag,
			     "background-set", &bg_set,
			     "foreground-set", &fg_set,
			     "style-set", &style_set,
			     "underline-set", &ul_set,
			     "weight-set", &weight_set,
			     "strikethrough-set", &st_set,
			     NULL);

		if (bg_set)
		{
			GdkRGBA *color = NULL;

			if (bg != NULL)
			{
				pango_attribute_destroy (bg);
			}

			g_object_get (tag, "background-rgba", &color, NULL);
			bg = pango_attr_background_new (color->red * 65535,
							color->green * 65535,
							color->blue * 65535);
			gdk_rgba_free (color);
		}

		if (fg_set)
		{
			GdkRGBA *color = NULL;

			if (fg != NULL)
			{
				pango_attribute_destroy (fg);
			}

			g_object_get (tag, "foreground-rgba", &color, NULL);
			fg = pango_attr_foreground_new (color->red * 65535,
							color->green * 65535,
							color->blue * 65535);
			gdk_rgba_free (color);
		}

		if (style_set)
		{
			PangoStyle style_value;
			if (style) pango_attribute_destroy (style);
			g_object_get (tag, "style", &style_value, NULL);
			style = pango_attr_style_new (style_value);
		}

		if (ul_set)
		{
			PangoUnderline underline;
			if (ul) pango_attribute_destroy (ul);
			g_object_get (tag, "underline", &underline, NULL);
			ul = pango_attr_underline_new (underline);
		}

		if (weight_set)
		{
			PangoWeight weight_value;
			if (weight) pango_attribute_destroy (weight);
			g_object_get (tag, "weight", &weight_value, NULL);
			weight = pango_attr_weight_new (weight_value);
		}

		if (st_set)
		{
			gboolean strikethrough;
			if (st) pango_attribute_destroy (st);
			g_object_get (tag, "strikethrough", &strikethrough, NULL);
			st = pango_attr_strikethrough_new (strikethrough);
		}
	}

	if (bg)
		attrs = g_slist_prepend (attrs, bg);
	if (fg)
		attrs = g_slist_prepend (attrs, fg);
	if (style)
		attrs = g_slist_prepend (attrs, style);
	if (ul)
		attrs = g_slist_prepend (attrs, ul);
	if (weight)
		attrs = g_slist_prepend (attrs, weight);
	if (st)
		attrs = g_slist_prepend (attrs, st);

	return attrs;
}

static gboolean
is_empty_line (const gchar *text)
{
	if (*text != '\0')
	{
		const gchar *p;

		for (p = text; p != NULL; p = g_utf8_next_char (p))
		{
			if (!g_unichar_isspace (*p))
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

static void
layout_paragraph (GtkSourcePrintCompositor *compositor,
                  GtkTextIter              *start,
                  GtkTextIter              *end)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	gchar *text;

	text = gtk_text_iter_get_slice (start, end);

	/* If it is an empty line (or it just contains tabs) pango has problems:
	 * see for instance comment #22 and #23 on bug #143874 and bug #457990.
	 * We just hack around it by inserting a space... not elegant but
	 * works :-) */
	if (gtk_text_iter_ends_line (start) ||
	    is_empty_line (text))
	{
		pango_layout_set_text (priv->layout, " ", 1);
		g_free (text);
		return;
	}

	pango_layout_set_text (priv->layout, text, -1);
	g_free (text);

	if (priv->highlight_syntax)
	{
		PangoAttrList *attr_list = NULL;
		GtkTextIter segm_start, segm_end;
		int start_index;

		/* Make sure it is highlighted even if it was not shown yet */
		gtk_source_buffer_ensure_highlight (priv->buffer,
						    start,
						    end);

		segm_start = *start;
		start_index = gtk_text_iter_get_line_index (start);

		while (gtk_text_iter_compare (&segm_start, end) < 0)
		{
			GSList *attrs;
			int si, ei;

			segm_end = segm_start;
			attrs = get_iter_attrs (compositor, &segm_end, end);
			if (attrs)
			{
				si = gtk_text_iter_get_line_index (&segm_start) - start_index;
				ei = gtk_text_iter_get_line_index (&segm_end) - start_index;
			}

			while (attrs)
			{
				PangoAttribute *a = attrs->data;

				a->start_index = si;
				a->end_index = ei;

				if (!attr_list)
					attr_list = pango_attr_list_new ();

				pango_attr_list_insert (attr_list, a);

				attrs = g_slist_delete_link (attrs, attrs);
			}

			segm_start = segm_end;
		}

		pango_layout_set_attributes (priv->layout,
					     attr_list);

		if (attr_list)
			pango_attr_list_unref (attr_list);
	}
}

static gboolean
line_is_numbered (GtkSourcePrintCompositor *compositor,
                  gint                      line_number)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	return (priv->print_line_numbers > 0) &&
	       ((line_number + 1) % priv->print_line_numbers == 0);
}

static void
set_pango_layouts_width (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (priv->layout != NULL);
	pango_layout_set_width (priv->layout,
				get_text_width (compositor) * PANGO_SCALE);

	if (priv->print_line_numbers)
	{
		g_return_if_fail (priv->line_numbers_layout != NULL);
		pango_layout_set_width (priv->line_numbers_layout,
					priv->line_numbers_width * PANGO_SCALE);
	}
}

/* If you want
   to use the ::paginate signal to perform pagination in async way, it is suggested to
   ensure the buffer is not modified until pagination terminates. */

/**
 * gtk_source_print_compositor_paginate:
 * @compositor: a #GtkSourcePrintCompositor.
 * @context: the #GtkPrintContext whose parameters (e.g. paper size, print margins, etc.)
 * are used by the the @compositor to paginate the document.
 *
 * Paginate the document associated with the @compositor.
 *
 * In order to support non-blocking pagination, document is paginated in small chunks.
 * Each time [method@PrintCompositor.paginate] is invoked, a chunk of the document
 * is paginated. To paginate the entire document, [method@PrintCompositor.paginate]
 * must be invoked multiple times.
 * It returns %TRUE if the document has been completely paginated, otherwise it returns %FALSE.
 *
 * This method has been designed to be invoked in the handler of the [signal@Gtk.PrintOperation::paginate] signal,
 * as shown in the following example:
 *
 * ```c
 * // Signal handler for the GtkPrintOperation::paginate signal
 *
 * static gboolean
 * paginate (GtkPrintOperation *operation,
 *           GtkPrintContext   *context,
 *           gpointer           user_data)
 * {
 *     GtkSourcePrintCompositor *compositor;
 *
 *     compositor = GTK_SOURCE_PRINT_COMPOSITOR (user_data);
 *
 *     if (gtk_source_print_compositor_paginate (compositor, context))
 *     {
 *         gint n_pages;
 *
 *         n_pages = gtk_source_print_compositor_get_n_pages (compositor);
 *         gtk_print_operation_set_n_pages (operation, n_pages);
 *
 *         return TRUE;
 *     }
 *
 *     return FALSE;
 * }
 * ```
 * ```python
 * def on_paginate(
 *     operation: Gtk.PrintOperation,
 *     context: Gtk.PrintContext,
 *     compositor: GtkSource.PrintCompositor,
 * ) -> bool:
 *     if compositor.paginate(context=context):
 *         n_pages = compositor.get_n_pages()
 *         operation.set_n_pages(n_pages=n_pages)
 *         return True
 *     return False
 * ```
 *
 * If you don't need to do pagination in chunks, you can simply do it all in the
 * [signal@Gtk.PrintOperation::begin-print] handler, and set the number of pages from there, like
 * in the following example:
 *
 * ```c
 * // Signal handler for the GtkPrintOperation::begin-print signal
 *
 * static void
 * begin_print (GtkPrintOperation *operation,
 *              GtkPrintContext   *context,
 *              gpointer           user_data)
 * {
 *     GtkSourcePrintCompositor *compositor;
 *     gint n_pages;
 *
 *     compositor = GTK_SOURCE_PRINT_COMPOSITOR (user_data);
 *
 *     while (!gtk_source_print_compositor_paginate (compositor, context));
 *
 *     n_pages = gtk_source_print_compositor_get_n_pages (compositor);
 *     gtk_print_operation_set_n_pages (operation, n_pages);
 * }
 * ```
 * ```python
 * def on_begin_print(
 *     operation: Gtk.PrintOperation,
 *     context: Gtk.PrintContext,
 *     compositor: GtkSource.PrintCompositor,
 * ) -> None:
 *     # Paginate until complete
 *     while not compositor.paginate(context=context):
 *         pass
 *
 *     n_pages = compositor.get_n_pages()
 *     operation.set_n_pages(n_pages=n_pages)
 * ```
 *
 * Return value: %TRUE if the document has been completely paginated, %FALSE otherwise.
 */
gboolean
gtk_source_print_compositor_paginate (GtkSourcePrintCompositor *compositor,
                                      GtkPrintContext          *context)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	GtkTextIter start, end;
	gint page_start_offset;
	double text_height;
	double cur_height;

	gboolean done;
	gint pages_count;

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), TRUE);
	g_return_val_if_fail (GTK_IS_PRINT_CONTEXT (context), TRUE);

	if (priv->state == DONE)
		return TRUE;

	if (priv->state == INIT)
	{
		if (GTK_SOURCE_PROFILER_ACTIVE)
		{
#ifdef GTK_SOURCE_PROFILER_ENABLED
			priv->pagination_timer = GTK_SOURCE_PROFILER_CURRENT_TIME;
#endif
		}

		g_return_val_if_fail (priv->pages == NULL, TRUE);

		priv->pages = g_array_new (FALSE, FALSE, sizeof (gint));

		setup_pango_layouts (compositor, context);

		calculate_line_numbers_layout_size (compositor, context);
		calculate_footer_height (compositor, context);
		calculate_header_height (compositor, context);
		calculate_page_size_and_margins (compositor, context);

		/* Set layouts width otherwise "aligh right" does not work as expected */
		/* Cannot be done when setting up layouts since we need the width */
		set_pango_layouts_width (compositor);

		priv->state = PAGINATING;
	}

	g_return_val_if_fail (priv->state == PAGINATING, FALSE);
	g_return_val_if_fail (priv->layout != NULL, FALSE);

	if (priv->pagination_mark == NULL)
	{
		gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (priv->buffer), &start);

		priv->pagination_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (priv->buffer),
										 NULL,
										 &start,
										 TRUE);

		/* add the first page start */
		page_start_offset = gtk_text_iter_get_offset (&start);
		g_array_append_val (priv->pages, page_start_offset);
	}
	else
	{
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer),
						  &start,
						  priv->pagination_mark);
	}

	GTK_SOURCE_PROFILER_LOG ("Start paginating at %d", gtk_text_iter_get_offset (&start));

	gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (priv->buffer), &end);

	cur_height = 0;
	text_height = get_text_height (compositor);

	done = gtk_text_iter_compare (&start, &end) >= 0;
	pages_count = 0;

	while (!done && (pages_count < PAGINATION_CHUNK_SIZE))
	{
		gint line_number;
		GtkTextIter line_end;
		gdouble line_height;

		line_number = gtk_text_iter_get_line (&start);

		line_end = start;
		if (!gtk_text_iter_ends_line (&line_end))
			gtk_text_iter_forward_to_line_end (&line_end);

		layout_paragraph (compositor, &start, &line_end);

		get_layout_size (priv->layout, NULL, &line_height);

		if (line_is_numbered (compositor, line_number))
		{
			g_assert (priv->line_numbers_height > 0);

			line_height = MAX (line_height,
					   priv->line_numbers_height);
		}

#define EPS (.1)
		if (cur_height + line_height > text_height + EPS)
		{
			/* if we have multiline paragraphs, see how much of
			 * it we can fit in the current page */
			if (priv->wrap_mode != GTK_WRAP_NONE &&
			    pango_layout_get_line_count (priv->layout) > 1)
			{
				PangoLayoutIter *layout_iter;
				PangoRectangle logical_rect;
				gboolean is_first_line = TRUE;
				double part_height = 0;
				gint idx;

				layout_iter = pango_layout_get_iter (priv->layout);

				do
				{
					double layout_line_height;

					pango_layout_iter_get_line_extents (layout_iter, NULL, &logical_rect);
					layout_line_height = (double) logical_rect.height / PANGO_SCALE;

					if (is_first_line &&
					    line_is_numbered (compositor, line_number))
					{
						layout_line_height = MAX (priv->line_numbers_height,
									  layout_line_height);
					}

					if (cur_height + part_height + layout_line_height > text_height + EPS)
						break;

					part_height += layout_line_height;
					is_first_line = FALSE;
				}
				while (pango_layout_iter_next_line (layout_iter));

				/* move our start iter to the page break:
				 * note that text_iter_set_index measures from
				 * the start of the line, while our layout
				 * may start in the middle of a line, so we have
				 * to add.
				 */
				idx = gtk_text_iter_get_line_index (&start);
				idx += pango_layout_iter_get_index (layout_iter);
				gtk_text_iter_set_line_index (&start, idx);

				pango_layout_iter_free (layout_iter);

				page_start_offset = gtk_text_iter_get_offset (&start);

				gtk_text_buffer_move_mark (GTK_TEXT_BUFFER (priv->buffer),
							   priv->pagination_mark,
							   &start);

				/* if the remainder fits on the next page, go
				 * on to the next line, otherwise restart pagination
				 * from the page break we found */
				if (line_height - part_height > text_height + EPS)
				{
					cur_height = 0;
				}
				else
				{
					/* reset cur_height for the next page */
					cur_height = line_height - part_height;
					gtk_text_iter_forward_line (&start);
				}
			}
			else
			{
				page_start_offset = gtk_text_iter_get_offset (&start);

				gtk_text_buffer_move_mark (GTK_TEXT_BUFFER (priv->buffer),
							   priv->pagination_mark,
							   &start);

				/* reset cur_height for the next page */
				cur_height = line_height;
				gtk_text_iter_forward_line (&start);
			}

			/* store the start of the new page */
			g_array_append_val (priv->pages,
					    page_start_offset);

			++pages_count;
		}
		else
		{
			cur_height += line_height;
			gtk_text_iter_forward_line (&start);
		}

		done = gtk_text_iter_compare (&start, &end) >= 0;
	}
#undef EPS

	if (done)
	{

#ifdef GTK_SOURCE_PROFILER_ENABLED
		if (GTK_SOURCE_PROFILER_ACTIVE)
		{
			gint64 duration = GTK_SOURCE_PROFILER_CURRENT_TIME - priv->pagination_timer;
			char *message = g_strdup_printf ("Paginated in %lf seconds",
							 (duration / 1000L) / (double)G_USEC_PER_SEC);
			GTK_SOURCE_PROFILER_MARK (duration, "Print Pagination", message);
			g_free (message);

			for (guint i = 0; i < priv->pages->len; i += 1)
			{
				gint offset;
				GtkTextIter iter;

				offset = g_array_index (priv->pages, int, i);
				gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (priv->buffer), &iter, offset);

				GTK_SOURCE_PROFILER_LOG ("page %d starts at line %d (offset %d)",
				                         i, gtk_text_iter_get_line (&iter), offset);
			}
		}
#endif

		priv->state = DONE;

		priv->n_pages = priv->pages->len;

		/* Remove the pagination mark */
		gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (priv->buffer),
					     priv->pagination_mark);
		priv->pagination_mark = NULL;
	}

	return (done != FALSE);
}

/**
 * gtk_source_print_compositor_get_pagination_progress:
 * @compositor: a #GtkSourcePrintCompositor.
 *
 * Returns the current fraction of the document pagination that has been completed.
 *
 * Return value: a fraction from 0.0 to 1.0 inclusive.
 */
gdouble
gtk_source_print_compositor_get_pagination_progress (GtkSourcePrintCompositor *compositor)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	GtkTextIter current;
	gint char_count;

	g_return_val_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor), 0.0);

	if (priv->state == INIT)
		return 0.0;

	if (priv->state == DONE)
		return 1.0;

	char_count = gtk_text_buffer_get_char_count (GTK_TEXT_BUFFER (priv->buffer));
	if (char_count == 0)
		return 1.0;

	g_return_val_if_fail (priv->pagination_mark != NULL, 0.0);

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer),
					  &current,
					  priv->pagination_mark);

	return (gdouble) gtk_text_iter_get_offset (&current) / (gdouble) char_count;
}

static void
print_header_string (GtkSourcePrintCompositor *compositor,
                     cairo_t                  *cr,
                     PangoAlignment            alignment,
                     const gchar              *format)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	gchar *text;

	text = evaluate_format_string (compositor, format);
	if (text != NULL)
	{
		PangoLayoutLine* line;
		gdouble	baseline;
		PangoLayoutIter *iter;

		gdouble layout_width;
		gdouble layout_height;
		gdouble header_width;
		gdouble x;

		header_width = priv->paper_width -
			       priv->real_margin_left -
			       priv->real_margin_right;

		pango_layout_set_text (priv->header_layout, text, -1);

		/* Print only the first line */
		iter = pango_layout_get_iter (priv->header_layout);
		baseline = (gdouble) pango_layout_iter_get_baseline (iter) / (gdouble) PANGO_SCALE;

		get_layout_size (priv->header_layout, &layout_width, &layout_height);

		switch (alignment)
		{
			case PANGO_ALIGN_RIGHT:
				x = priv->real_margin_left + header_width - layout_width;
				break;

			case PANGO_ALIGN_CENTER:
				x = priv->real_margin_left + header_width / 2 - layout_width / 2;
				break;

			case PANGO_ALIGN_LEFT:
			default:
				x = priv->real_margin_left;
				break;
		}

#if 0
		/* Debug Margins */
		{
			cairo_save (cr);

			cairo_set_line_width (cr, 1.);
			cairo_set_source_rgb (cr, 0., 0., 1.);
			cairo_rectangle (cr,
					 x,
					 priv->real_margin_top,
					 layout_width,
					 layout_height);
			cairo_stroke (cr);

			cairo_restore (cr);
		}
#endif

		line = pango_layout_iter_get_line_readonly (iter);

		cairo_move_to (cr,
			       x,
			       priv->real_margin_top + baseline);

		pango_cairo_show_layout_line (cr, line);

		pango_layout_iter_free (iter);
		g_free (text);
	}
}

static void
print_header (GtkSourcePrintCompositor *compositor,
              cairo_t                  *cr)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	pango_cairo_update_layout (cr, priv->header_layout);

	/* left format */
	if (priv->header_format_left != NULL)
		print_header_string (compositor,
				     cr,
				     PANGO_ALIGN_LEFT,
				     priv->header_format_left);

	/* right format */
	if (priv->header_format_right != NULL)
		print_header_string (compositor,
				     cr,
				     PANGO_ALIGN_RIGHT,
				     priv->header_format_right);

	/* center format */
	if (priv->header_format_center != NULL)
		print_header_string (compositor,
				     cr,
				     PANGO_ALIGN_CENTER,
				     priv->header_format_center);

	/* separator */
	if (priv->header_separator)
	{
		gdouble y = priv->real_margin_top +
			    (1 - SEPARATOR_SPACING_FACTOR) * priv->header_height;

		cairo_save (cr);

		cairo_move_to (cr, priv->real_margin_left, y);
		cairo_set_line_width (cr, SEPARATOR_LINE_WIDTH);
		cairo_line_to (cr, priv->paper_width - priv->real_margin_right, y);
		cairo_stroke (cr);

		cairo_restore (cr);
	}
}

static void
print_footer_string (GtkSourcePrintCompositor *compositor,
                     cairo_t                  *cr,
                     PangoAlignment            alignment,
                     const gchar              *format)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	gchar *text;

	text = evaluate_format_string (compositor, format);
	if (text != NULL)
	{
		PangoLayoutLine* line;

		gdouble layout_width;
		gdouble layout_height;
		gdouble footer_width;
		gdouble x;

		footer_width = priv->paper_width -
			       priv->real_margin_left -
			       priv->real_margin_right;

		pango_layout_set_text (priv->footer_layout, text, -1);

		get_layout_size (priv->footer_layout, &layout_width, &layout_height);

		switch (alignment)
		{
			case PANGO_ALIGN_RIGHT:
				x = priv->real_margin_left + footer_width - layout_width;
				break;

			case PANGO_ALIGN_CENTER:
				x = priv->real_margin_left + footer_width / 2 - layout_width / 2;
				break;

			case PANGO_ALIGN_LEFT:
			default:
				x = priv->real_margin_left;
				break;
		}
		/* Print only the first line */
		line = pango_layout_get_line (priv->footer_layout, 0);

#if 0
		/* Debug Footer Line */
		{
			gdouble w;
			gdouble h;

			get_layout_size (priv->footer_layout, &w, &h);

			cairo_save (cr);
			cairo_set_line_width (cr, 1.);
			cairo_set_source_rgb (cr, 0., 0., 1.);
			cairo_rectangle (cr,
					 x,
					 priv->paper_height - priv->real_margin_bottom - h,
					 layout_width,
					 layout_height);
			cairo_stroke (cr);
			cairo_restore (cr);
		}
#endif

		cairo_move_to (cr,
		               x,
		               priv->paper_height - priv->real_margin_bottom - priv->footer_font_descent);

		pango_cairo_show_layout_line (cr, line);

		g_free (text);
	}
}

static void
print_footer (GtkSourcePrintCompositor *compositor,
              cairo_t                  *cr)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	pango_cairo_update_layout (cr, priv->footer_layout);

	/* left format */
	if (priv->footer_format_left != NULL)
		print_footer_string (compositor,
				     cr,
				     PANGO_ALIGN_LEFT,
				     priv->footer_format_left);

	/* right format */
	if (priv->footer_format_right != NULL)
		print_footer_string (compositor,
				     cr,
				     PANGO_ALIGN_RIGHT,
				     priv->footer_format_right);

	/* center format */
	if (priv->footer_format_center != NULL)
		print_footer_string (compositor,
				     cr,
				     PANGO_ALIGN_CENTER,
				     priv->footer_format_center);

	/* separator */
	if (priv->footer_separator)
	{
		gdouble y = priv->paper_height -
			    priv->real_margin_bottom -
			    (1 - SEPARATOR_SPACING_FACTOR) * priv->footer_height;

		cairo_save (cr);

		cairo_move_to (cr, priv->real_margin_left, y);
		cairo_set_line_width (cr, SEPARATOR_LINE_WIDTH);
		cairo_line_to (cr, priv->paper_width - priv->real_margin_right, y);
		cairo_stroke (cr);

		cairo_restore (cr);
	}
}

/**
 * gtk_source_print_compositor_draw_page:
 * @compositor: a #GtkSourcePrintCompositor.
 * @context: the #GtkPrintContext encapsulating the context information that is required when
 *           drawing the page for printing.
 * @page_nr: the number of the page to print.
 *
 * Draw page @page_nr for printing on the the Cairo context encapsuled in @context.
 *
 * This method has been designed to be called in the handler of the [signal@Gtk.PrintOperation::draw_page] signal
 * as shown in the following example:
 *
 * ```c
 * // Signal handler for the GtkPrintOperation::draw_page signal
 *
 * static void
 * draw_page (GtkPrintOperation *operation,
 *            GtkPrintContext   *context,
 *            gint               page_nr,
 *            gpointer           user_data)
 * {
 *     GtkSourcePrintCompositor *compositor;
 *
 *     compositor = GTK_SOURCE_PRINT_COMPOSITOR (user_data);
 *
 *     gtk_source_print_compositor_draw_page (compositor,
 *                                            context,
 *                                            page_nr);
 * }
 * ```
 * ```python
 * def on_draw_page(
 *     operation: Gtk.PrintOperation,
 *     context: Gtk.PrintContext,
 *     page_nr: int,
 *     compositor: GtkSource.PrintCompositor,
 * ) -> None:
 *     """Signal handler for draw-page that renders a single page."""
 *     compositor.draw_page(context=context, page_nr=page_nr)
 * ```
 */
void
gtk_source_print_compositor_draw_page (GtkSourcePrintCompositor *compositor,
                                       GtkPrintContext          *context,
                                       gint                      page_nr)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);
	cairo_t *cr;
	GtkTextIter start, end;
	gint offset;
	double x, y, ln_x;

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (GTK_IS_PRINT_CONTEXT (context));
	g_return_if_fail (page_nr >= 0);

	priv->current_page = page_nr;

	cr = gtk_print_context_get_cairo_context (context);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_translate (cr,
			 -1 * priv->page_margin_left,
			 -1 * priv->page_margin_top);

	if (is_header_to_print (compositor))
	{
		print_header (compositor, cr);
	}

	if (is_footer_to_print (compositor))
	{
		print_footer (compositor, cr);
	}

	x = get_text_x (compositor);
	y = get_text_y (compositor);
	ln_x = get_line_numbers_x (compositor);

#if 0
	/* Debug Page Drawing */
	{
		cairo_save (cr);

		cairo_set_line_width (cr, 1.);
		cairo_set_source_rgb (cr, 0., 0., 1.);
		cairo_rectangle (cr,
				 priv->real_margin_left,
				 priv->real_margin_top,
				 priv->paper_width -
				 	priv->real_margin_left - priv->real_margin_right,
				 priv->paper_height -
				 	priv->real_margin_top - priv->real_margin_bottom);
		cairo_stroke (cr);

		cairo_set_source_rgb (cr, 1., 0., 0.);
		cairo_rectangle (cr,
				 ln_x, y,
				 priv->line_numbers_width,
				 get_text_height (compositor));
		cairo_stroke (cr);

		cairo_set_source_rgb (cr, 0., 1., 0.);
		cairo_rectangle (cr,
				 x, y,
				 get_text_width (compositor),
				 get_text_height (compositor));
		cairo_stroke (cr);

		cairo_set_source_rgb (cr, 1., 0., 0.);
		cairo_rectangle (cr,
				 0, 0,
				 priv->paper_width,
				 priv->paper_height);
		cairo_stroke (cr);

		cairo_restore (cr);
	}
#endif

	g_return_if_fail (priv->layout != NULL);
	pango_cairo_update_layout (cr, priv->layout);

	if (priv->print_line_numbers)
	{
		g_return_if_fail (priv->line_numbers_layout != NULL);
		pango_cairo_update_layout (cr, priv->line_numbers_layout);
	}

	g_return_if_fail (priv->buffer != NULL);
	g_return_if_fail (priv->pages != NULL);
	g_return_if_fail ((guint) page_nr < priv->pages->len);

	offset = g_array_index (priv->pages, int, page_nr);
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (priv->buffer),
					    &start, offset);

	if ((guint) page_nr + 1 < priv->pages->len)
	{
		offset = g_array_index (priv->pages, int, page_nr + 1);
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (priv->buffer),
						    &end, offset);
	}
	else
	{
		gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (priv->buffer),
					      &end);
	}

	while (gtk_text_iter_compare (&start, &end) < 0)
	{
		GtkTextIter line_end;
		gint line_number;
		double line_height;
		double baseline_offset;

		line_end = start;
		if (!gtk_text_iter_ends_line (&line_end))
			gtk_text_iter_forward_to_line_end (&line_end);
		if (gtk_text_iter_compare (&line_end, &end) > 0)
			line_end = end;

		if (gtk_text_iter_starts_line (&start))
		{
			line_number = gtk_text_iter_get_line (&start);
		}
		else
		{
			/* This happens only if the first line of the page
			 * is the continuation of the last line of the previous page.
			 * In this case the line numbers must not be print
			 */
			line_number = -1;
		}

		layout_paragraph (compositor, &start, &line_end);

		get_layout_size (priv->layout, NULL, &line_height);

		baseline_offset = 0;

		/* print the line number if needed */
		if ((line_number >= 0) && line_is_numbered (compositor, line_number))
		{
			PangoLayoutIter *iter;
			double baseline;
			double ln_baseline;
			double ln_baseline_offset;
			gchar *str;

			str = g_strdup_printf ("%d", line_number + 1);
			pango_layout_set_text (priv->line_numbers_layout, str, -1);
			g_free (str);

			/* Adjust the baseline */
			iter = pango_layout_get_iter (priv->layout);
			baseline = (double) pango_layout_iter_get_baseline (iter) / (double) PANGO_SCALE;
			pango_layout_iter_free (iter);

			iter = pango_layout_get_iter (priv->line_numbers_layout);
			ln_baseline = (double) pango_layout_iter_get_baseline (iter) / (double) PANGO_SCALE;
			pango_layout_iter_free (iter);

			ln_baseline_offset = baseline - ln_baseline;

			if (ln_baseline_offset < 0)
			{
				baseline_offset = -ln_baseline_offset;
				ln_baseline_offset =  0;
			}

			cairo_move_to (cr, ln_x, y + ln_baseline_offset);

			g_return_if_fail (priv->line_numbers_layout != NULL);
			pango_cairo_show_layout (cr, priv->line_numbers_layout);
		}

		cairo_move_to (cr, x, y + baseline_offset);
		pango_cairo_show_layout (cr, priv->layout);

		line_height = MAX (line_height,
				   priv->line_numbers_height);

		y += line_height;
		gtk_text_iter_forward_line (&start);
	}
}

/**
 * gtk_source_print_compositor_ignore_tag:
 * @compositor: a #GtkSourcePrintCompositor
 * @tag: a #GtkTextTag
 *
 * Specifies a tag whose style should be ignored when compositing the
 * document to the printable page.
 *
 * Since: 5.2
 */
void
gtk_source_print_compositor_ignore_tag (GtkSourcePrintCompositor *compositor,
                                        GtkTextTag               *tag)
{
	GtkSourcePrintCompositorPrivate *priv = gtk_source_print_compositor_get_instance_private (compositor);

	g_return_if_fail (GTK_SOURCE_IS_PRINT_COMPOSITOR (compositor));
	g_return_if_fail (!tag || GTK_IS_TEXT_TAG (tag));

	if (tag != NULL)
	{
		g_hash_table_add (priv->ignored_tags, tag);
	}
}
