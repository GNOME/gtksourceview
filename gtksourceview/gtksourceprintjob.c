/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/*
 * gtksourceprintjob.c
 * This file is part of GtkSourceView
 *
 * Derived from gedit-print.c
 *
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002  Paolo Maggi  
 * Copyright (C) 2003  Gustavo Giráldez
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
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA. 
 */
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>

#include "gtksourceview-i18n.h"
#include "gtksourceview-marshal.h"
#include "gtksourcetag.h"
#include "gtksourceprintjob.h"

#ifdef ENABLE_PROFILE
#define PROFILE(x) x
#else
#define PROFILE(x)
#endif

#ifdef ENABLE_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x)
#endif


#define DEFAULT_FONT_NAME   "Monospace Regular 10"
#define DEFAULT_COLOR       0x000000ff

#define CM(v) ((v) * 72.0 / 2.54)
#define A4_WIDTH (210.0 * 72 / 25.4)
#define A4_HEIGHT (297.0 * 72 / 25.4)

#define LINE_SPACING_RATIO      1.2

#define NUMBERS_TEXT_SEPARATION CM(0.5)

#define HEADER_FOOTER_SIZE      2.5
#define SEPARATOR_SPACING       1.5
#define SEPARATOR_LINE_WIDTH    1.0


typedef struct _TextSegment TextSegment;
typedef struct _DisplayLine DisplayLine;
typedef struct _TextStyle   TextStyle;

/* a piece of text (within a paragraph) of the same style */
struct _TextSegment
{
	TextSegment             *next;
	TextStyle               *style;
	gchar                   *text;
};

/* a printable line */
struct _DisplayLine
{
	guint                    page;
	guint                    line_number;
	TextSegment             *segment;
	const gchar             *start;
	guint                    char_count;
};

/* the style of a TextSegment */
struct _TextStyle
{
	GnomeFont               *font;
	gdouble                  red;
	gdouble                  green;
	gdouble                  blue;
};


struct _GtkSourcePrintJobPrivate
{
	/* General job configuration */
	GnomePrintConfig 	*config;
	GtkSourceBuffer         *buffer;
	guint			 tabs_width;
	GtkWrapMode		 wrap_mode;
	gboolean                 highlight;
	GnomeFont		*font;
	GnomeFont		*numbers_font;
	guint 			 print_numbers;
	gdouble                  margin_top;
	gdouble                  margin_bottom;
	gdouble                  margin_left;
	gdouble                  margin_right;

	/* Default header and footer configuration */
	gboolean                 print_header;
	gboolean                 print_footer;
	GnomeFont		*header_footer_font;
	gchar                   *header_format_left;
	gchar                   *header_format_center;
	gchar                   *header_format_right;
	gboolean                 header_separator;
	gchar                   *footer_format_left;
	gchar                   *footer_format_center;
	gchar                   *footer_format_right;
	gboolean                 footer_separator;

	/* Job data */
	guint                    first_line_number;
	guint                    last_line_number;
	GSList                  *lines;
	GSList                  *display_lines;

	/* Job state */
	gboolean                 printing;
	guint                    idle_printing_tag;
	GnomePrintContext	*print_ctxt;
	GnomePrintJob           *print_job;
	gint                     page;
	gint                     page_count;
	guint                    line_number;
	gdouble                  available_height;
	GSList                  *current_display_line;
	guint                    printed_lines;

	/* Current printing style */
	GnomeFont               *current_font;
	guint32                  current_color;  /* what gnome_glyphlist_color needs */
	
	/* Cached information - all this information is obtained from
	 * other fields in the configuration */
	GHashTable              *tag_styles;

	gdouble			 page_width;
	gdouble			 page_height;
	/* outer margins */
	gdouble			 doc_margin_top;
	gdouble			 doc_margin_left;
	gdouble			 doc_margin_right;
	gdouble			 doc_margin_bottom;

	gdouble                  header_height;
	gdouble                  footer_height;
	gdouble                  numbers_width;

	/* printable (for the document itself) size */
	gdouble                  text_width;
	gdouble                  text_height;
	/* various sizes for the default text font */
	gdouble                  width_of_tab;
	gdouble                  space_advance;
	gdouble                  font_height;
	gdouble                  line_spacing;
};


enum
{
	PROP_0,
	PROP_CONFIG,
	PROP_BUFFER,
	PROP_TABS_WIDTH,
	PROP_WRAP_MODE,
	PROP_HIGHLIGHT,
	PROP_FONT,
	PROP_NUMBERS_FONT,
	PROP_PRINT_NUMBERS,
	PROP_PRINT_HEADER,
	PROP_PRINT_FOOTER,
	PROP_HEADER_FOOTER_FONT,
};

enum
{
	BEGIN_PAGE = 0,
	FINISHED,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;
static guint 	     print_job_signals [LAST_SIGNAL] = { 0 };

static void     gtk_source_print_job_class_init    (GtkSourcePrintJobClass *klass);
static void     gtk_source_print_job_instance_init (GtkSourcePrintJob      *job);
static void     gtk_source_print_job_finalize      (GObject                *object);
static void     gtk_source_print_job_get_property  (GObject                *object,
						    guint                   property_id,
						    GValue                 *value,
						    GParamSpec             *pspec);
static void     gtk_source_print_job_set_property  (GObject                *object,
						    guint                   property_id,
						    const GValue           *value,
						    GParamSpec             *pspec);
static void     gtk_source_print_job_begin_page    (GtkSourcePrintJob      *job);

static void     default_print_header               (GtkSourcePrintJob      *job,
						    gdouble                 x,
						    gdouble                 y);
static void     default_print_footer               (GtkSourcePrintJob      *job,
						    gdouble                 x,
						    gdouble                 y);


GType
gtk_source_print_job_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0)
	{
		static const GTypeInfo our_info = {
			sizeof (GtkSourcePrintJobClass),
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) gtk_source_print_job_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (GtkSourcePrintJob),
			0,	/* n_preallocs */
			(GInstanceInitFunc) gtk_source_print_job_instance_init
		};

		our_type = g_type_register_static (G_TYPE_OBJECT,
						   "GtkSourcePrintJob",
						   &our_info, 
						   0);
	}
	
	return our_type;
}
	
static void
gtk_source_print_job_class_init (GtkSourcePrintJobClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);
		
	object_class->finalize	   = gtk_source_print_job_finalize;
	object_class->get_property = gtk_source_print_job_get_property;
	object_class->set_property = gtk_source_print_job_set_property;

	klass->begin_page = gtk_source_print_job_begin_page;
	klass->finished = NULL;
	
	g_object_class_install_property (object_class,
					 PROP_CONFIG,
					 g_param_spec_object ("config",
							      _("Configuration"),
							      _("Configuration options for "
								"the print job"),
							      GNOME_TYPE_PRINT_CONFIG,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_BUFFER,
					 g_param_spec_object ("buffer",
							      _("Source Buffer"),
							      _("GtkSourceBuffer object to print"),
							      GTK_TYPE_SOURCE_BUFFER,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_TABS_WIDTH,
					 g_param_spec_uint ("tabs_width",
							    _("Tabs Width"),
							    _("Width in equivalent space "
							      "characters of tabs"),
							    0, 100, 8,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_WRAP_MODE,
					 g_param_spec_enum ("wrap_mode",
							    _("Wrap Mode"),
							    _("Word wrapping mode"),
							    GTK_TYPE_WRAP_MODE,
							    GTK_WRAP_NONE,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_HIGHLIGHT,
					 g_param_spec_boolean ("highlight",
							       _("Highlight"),
							       _("Whether to print the "
								 "document with highlighted "
								 "syntax"),
							       TRUE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_FONT,
					 g_param_spec_string ("font",
							      _("Font"),
							      _("Font name to use for the "
								"document text"),
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_NUMBERS_FONT,
					 g_param_spec_string ("numbers_font",
							      _("Numbers Font"),
							      _("Font name to use for the "
								"line numbers"),
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_PRINT_NUMBERS,
					 g_param_spec_uint ("print_numbers",
							    _("Print Line Numbers"),
							    _("Interval of printed line numbers "
							      "(0 means no numbers)"),
							    0, 100, 1,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_PRINT_HEADER,
					 g_param_spec_boolean ("print_header",
							       _("Print Header"),
							       _("Whether to print a header "
								 "in each page"),
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_PRINT_FOOTER,
					 g_param_spec_boolean ("print_footer",
							       _("Print Footer"),
							       _("Whether to print a footer "
								 "in each page"),
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_HEADER_FOOTER_FONT,
					 g_param_spec_string ("header_footer_font",
							      _("Header and Footer Font"),
							      _("Font name to use for the header "
								"and footer"),
							      NULL,
							      G_PARAM_READWRITE));
	
	print_job_signals [BEGIN_PAGE] =
	    g_signal_new ("begin_page",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GtkSourcePrintJobClass, begin_page),
			  NULL, NULL,
			  gtksourceview_marshal_VOID__VOID,
			  G_TYPE_NONE, 
			  0);
	print_job_signals [FINISHED] =
	    g_signal_new ("finished",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_FIRST,
			  G_STRUCT_OFFSET (GtkSourcePrintJobClass, finished),
			  NULL, NULL,
			  gtksourceview_marshal_VOID__VOID,
			  G_TYPE_NONE, 
			  0);
}

static void
gtk_source_print_job_instance_init (GtkSourcePrintJob *job)
{
	GtkSourcePrintJobPrivate *priv;

	priv = g_new0 (GtkSourcePrintJobPrivate, 1);
	job->priv = priv;

	/* default job configuration */
	priv->config = NULL;
	priv->buffer = NULL;

	priv->tabs_width = 8;
	priv->wrap_mode = GTK_WRAP_NONE;
	priv->highlight = TRUE;
	priv->font = NULL;
	priv->numbers_font = NULL;
	priv->print_numbers = 1;
	priv->margin_top = 0.0;
	priv->margin_bottom = 0.0;
	priv->margin_left = 0.0;
	priv->margin_right = 0.0;

	priv->print_header = FALSE;
	priv->print_footer = FALSE;
	priv->header_footer_font = NULL;
	priv->header_format_left = NULL;
	priv->header_format_center = NULL;
	priv->header_format_right = NULL;
	priv->header_separator = FALSE;
	priv->footer_format_left = NULL;
	priv->footer_format_center = NULL;
	priv->footer_format_right = NULL;
	priv->footer_separator = FALSE;

	/* initial state */
	priv->printing = FALSE;
	priv->print_ctxt = NULL;
	priv->print_job = NULL;
	priv->page = 0;
	priv->page_count = 0;

	priv->first_line_number = 0;
	priv->lines = NULL;
	priv->display_lines = NULL;
	priv->tag_styles = NULL;

	/* some default, sane values */
	priv->page_width = A4_WIDTH;
	priv->page_height = A4_HEIGHT;
	priv->doc_margin_top = CM (1);
	priv->doc_margin_left = CM (1);
	priv->doc_margin_right = CM (1);
	priv->doc_margin_bottom = CM (1);
}

static void
free_lines (GSList *lines)
{
	while (lines != NULL)
	{
		TextSegment *seg = lines->data;
		while (seg != NULL)
		{
			TextSegment *next = seg->next;
			g_free (seg->text);
			g_free (seg);
			seg = next;
		}
		lines = g_slist_delete_link (lines, lines);
	}
}

static void
free_display_lines (GSList *display_lines)
{
	while (display_lines != NULL)
	{
		g_free (display_lines->data);
		display_lines = g_slist_delete_link (display_lines, display_lines);
	}
}

static void
gtk_source_print_job_finalize (GObject *object)
{
	GtkSourcePrintJob *job;
	GtkSourcePrintJobPrivate *priv;
	
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (object));
	
	job = GTK_SOURCE_PRINT_JOB (object);
	priv = job->priv;
	
	if (priv != NULL)
	{
		if (priv->config != NULL)
			gnome_print_config_unref (priv->config);
		if (priv->buffer != NULL)
			g_object_unref (priv->buffer);
		if (priv->font != NULL)
			gnome_font_unref (priv->font);
		if (priv->numbers_font != NULL)
			gnome_font_unref (priv->numbers_font);
		if (priv->header_footer_font != NULL)
			gnome_font_unref (priv->header_footer_font);
		g_free (priv->header_format_left);
		g_free (priv->header_format_right);
		g_free (priv->header_format_center);
		g_free (priv->footer_format_left);
		g_free (priv->footer_format_right);
		g_free (priv->footer_format_center);
		
		if (priv->print_ctxt != NULL)
			g_object_unref (priv->print_ctxt);
		if (priv->print_job != NULL)
			g_object_unref (priv->print_job);

		if (priv->lines != NULL)
			free_lines (priv->lines);
		if (priv->display_lines != NULL)
			free_display_lines (priv->display_lines);
		if (priv->tag_styles != NULL)
			g_hash_table_destroy (priv->tag_styles);
		
		g_free (priv);
		job->priv = NULL;
	}
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void 
gtk_source_print_job_get_property (GObject    *object,
				   guint       prop_id,
				   GValue     *value,
				   GParamSpec *pspec)
{
	GtkSourcePrintJob *job = GTK_SOURCE_PRINT_JOB (object);

	switch (prop_id)
	{
		case PROP_CONFIG:
			g_value_set_object (value, job->priv->config);
			break;
			
		case PROP_BUFFER:
			g_value_set_object (value, job->priv->buffer);
			break;

		case PROP_TABS_WIDTH:
			g_value_set_uint (value, job->priv->tabs_width);
			break;
			
		case PROP_WRAP_MODE:
			g_value_set_enum (value, job->priv->wrap_mode);
			break;

		case PROP_HIGHLIGHT:
			g_value_set_boolean (value, job->priv->highlight);
			break;
			
		case PROP_FONT:
			g_value_set_string (value, gtk_source_print_job_get_font (job));
			break;
			
		case PROP_NUMBERS_FONT:
 			g_value_set_string (value, gtk_source_print_job_get_numbers_font (job));
			break;
			
		case PROP_PRINT_NUMBERS:
			g_value_set_uint (value, job->priv->print_numbers);
			break;
			
		case PROP_PRINT_HEADER:
			g_value_set_boolean (value, job->priv->print_header);
			break;
			
		case PROP_PRINT_FOOTER:
			g_value_set_boolean (value, job->priv->print_footer);
			break;
			
		case PROP_HEADER_FOOTER_FONT:
			g_value_set_string (value,
 					    gtk_source_print_job_get_header_footer_font (job));
			break;
			
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void 
gtk_source_print_job_set_property (GObject      *object,
				   guint         prop_id,
				   const GValue *value,
				   GParamSpec   *pspec)
{
	GtkSourcePrintJob *job = GTK_SOURCE_PRINT_JOB (object);

	switch (prop_id)
	{
		case PROP_CONFIG:
			gtk_source_print_job_set_config (job, g_value_get_object (value));
			break;
			
		case PROP_BUFFER:
			gtk_source_print_job_set_buffer (job, g_value_get_object (value));
			break;
			
		case PROP_TABS_WIDTH:
			gtk_source_print_job_set_tabs_width (job, g_value_get_uint (value));
			break;
			
		case PROP_WRAP_MODE:
			gtk_source_print_job_set_wrap_mode (job, g_value_get_enum (value));
			break;

		case PROP_HIGHLIGHT:
			gtk_source_print_job_set_highlight (job, g_value_get_boolean (value));
			break;

		case PROP_FONT:
			gtk_source_print_job_set_font (job, g_value_get_string (value));
			break;

		case PROP_NUMBERS_FONT:
			gtk_source_print_job_set_numbers_font (job, g_value_get_string (value));
			break;

		case PROP_PRINT_NUMBERS:
			gtk_source_print_job_set_print_numbers (job, g_value_get_uint (value));
			break;
			
		case PROP_PRINT_HEADER:
			gtk_source_print_job_set_print_header (job, g_value_get_boolean (value));
			break;

		case PROP_PRINT_FOOTER:
			gtk_source_print_job_set_print_footer (job, g_value_get_boolean (value));
			break;

		case PROP_HEADER_FOOTER_FONT:
			gtk_source_print_job_set_header_footer_font (job,
								     g_value_get_string (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void 
gtk_source_print_job_begin_page (GtkSourcePrintJob *job)
{
	g_return_if_fail (job->priv->printing);
	
	if (job->priv->print_header && job->priv->header_height > 0)
	{
		gdouble x, y;

		x = job->priv->doc_margin_left + job->priv->margin_left;
		y = job->priv->page_height - job->priv->doc_margin_top - job->priv->margin_top;
		default_print_header (job, x, y);
	}

	if (job->priv->print_footer && job->priv->footer_height > 0)
	{
		gdouble x, y;

		x = job->priv->doc_margin_left + job->priv->margin_left;
		y = job->priv->doc_margin_bottom +
			job->priv->margin_bottom +
			job->priv->footer_height;
		default_print_footer (job, x, y);
	}
}

/* ---- Configuration functions */

static void
ensure_print_config (GtkSourcePrintJob *job)
{
	if (job->priv->config == NULL)
		job->priv->config = gnome_print_config_default ();
	if (job->priv->font == NULL)
		job->priv->font = gnome_font_find_closest_from_full_name (DEFAULT_FONT_NAME);
}

static gboolean
update_page_size_and_margins (GtkSourcePrintJob *job)
{
	ArtPoint space_advance;
	gint space;
	gchar *tmp;
	
	gnome_print_job_get_page_size_from_config (job->priv->config, 
						   &job->priv->page_width,
						   &job->priv->page_height);

	gnome_print_config_get_length (job->priv->config, GNOME_PRINT_KEY_PAGE_MARGIN_TOP,
				       &job->priv->doc_margin_top, NULL);
	gnome_print_config_get_length (job->priv->config, GNOME_PRINT_KEY_PAGE_MARGIN_BOTTOM,
				       &job->priv->doc_margin_bottom, NULL);
	gnome_print_config_get_length (job->priv->config, GNOME_PRINT_KEY_PAGE_MARGIN_LEFT,
				       &job->priv->doc_margin_left, NULL);
	gnome_print_config_get_length (job->priv->config, GNOME_PRINT_KEY_PAGE_MARGIN_RIGHT,
				       &job->priv->doc_margin_right, NULL);

	/* set default fonts for numbers and header/footer */
	if (job->priv->numbers_font == NULL)
	{
		job->priv->numbers_font = job->priv->font;
		g_object_ref (job->priv->font);
	}
	
	if (job->priv->header_footer_font == NULL)
	{
		job->priv->header_footer_font = job->priv->font;
		g_object_ref (job->priv->font);
	}
	
	/* calculate numbers width */
	if (job->priv->print_numbers > 0)
	{
		tmp = g_strdup_printf ("%d", job->priv->last_line_number);
		job->priv->numbers_width =
			gnome_font_get_width_utf8 (job->priv->numbers_font, tmp) +
			NUMBERS_TEXT_SEPARATION;
		g_free (tmp);
	}
	else
		job->priv->numbers_width = 0.0;

	/* calculate header/footer height */
	if (job->priv->print_header &&
	    (job->priv->header_format_left != NULL ||
	     job->priv->header_format_center != NULL ||
	     job->priv->header_format_right != NULL))
		job->priv->header_height = HEADER_FOOTER_SIZE *
			gnome_font_get_size (job->priv->header_footer_font);
	else
		job->priv->header_height = 0.0;

	if (job->priv->print_footer &&
	    (job->priv->footer_format_left != NULL ||
	     job->priv->footer_format_center != NULL ||
	     job->priv->footer_format_right != NULL))
		job->priv->footer_height = HEADER_FOOTER_SIZE *
			gnome_font_get_size (job->priv->header_footer_font);
	else
		job->priv->footer_height = 0.0;

	/* verify that the user provided margins are not too excesive
	 * and that we still have room for the text */
	job->priv->text_width = (job->priv->page_width -
				 job->priv->doc_margin_left - job->priv->doc_margin_right -
				 job->priv->margin_left - job->priv->margin_right -
				 job->priv->numbers_width);
	
	job->priv->text_height = (job->priv->page_height -
				  job->priv->doc_margin_top - job->priv->doc_margin_bottom -
				  job->priv->margin_top - job->priv->margin_bottom -
				  job->priv->header_height - job->priv->footer_height);

	/* FIXME: put some saner values than 5cm - Gustavo */
	g_return_val_if_fail (job->priv->text_width > CM(5.0), FALSE);
	g_return_val_if_fail (job->priv->text_height > CM(5.0), FALSE);

	/* Find space advance */
	space = gnome_font_lookup_default (job->priv->font, ' ');
	gnome_font_get_glyph_stdadvance (job->priv->font, space, &space_advance);
	job->priv->space_advance = space_advance.x;

	/* calculate tab stops */
	job->priv->width_of_tab = space_advance.x * job->priv->tabs_width;

	/* font heights and line spacings */
	job->priv->font_height =
		gnome_font_get_ascender (job->priv->font) +
		gnome_font_get_descender (job->priv->font);
	job->priv->line_spacing = LINE_SPACING_RATIO * gnome_font_get_size (job->priv->font);
	
	return TRUE;
}

/* ----- Helper functions */

static gchar * 
construct_full_font_name (GnomeFont *font)
{
	gchar *retval;

	retval = g_strdup_printf ("%s %.1f",
				  gnome_font_get_name (font),
				  gnome_font_get_size (font));

	return retval;
}

static GnomeFont *
find_gnome_font_from_widget (GtkWidget *widget)
{
	PangoContext *ctxt;
	PangoFontDescription *desc;
	PangoStyle style;
	PangoWeight weight;
	gboolean italic;
	const char *family_name;
	gdouble size;
	
	ctxt = gtk_widget_get_pango_context (widget);
	desc = pango_context_get_font_description (ctxt);
	
	g_return_val_if_fail (desc != NULL, NULL);
	
	weight = pango_font_description_get_weight (desc);
	style = pango_font_description_get_style (desc);
	italic = ((style == PANGO_STYLE_OBLIQUE) || (style == PANGO_STYLE_ITALIC));
	family_name = pango_font_description_get_family (desc);
	size = (gdouble) pango_font_description_get_size (desc) / PANGO_SCALE;
	
	return gnome_font_find_closest_from_weight_slant (family_name, weight, italic, size);
}

/* ---- TextStyle functions */

static TextStyle * 
text_style_new (GtkSourcePrintJob *job, GtkSourceTag *tag)
{
	TextStyle *style;
	gint text_style, text_weight;
	gboolean fg_set;
	GdkColor *fg;
	
	g_return_val_if_fail (tag != NULL && GTK_IS_SOURCE_TAG (tag), NULL);

	style = g_new0 (TextStyle, 1);

	/* FIXME: handle background, underline and strikethrough */
	g_object_get (G_OBJECT (tag),
		      "foreground_gdk", &fg,
		      "foreground_set", &fg_set,
		      "style", &text_style,
		      "weight", &text_weight,
		      NULL);

	if (fg_set)
	{
		style->red = (gdouble) fg->red / 65535;
		style->green = (gdouble) fg->green / 65535;
		style->blue = (gdouble) fg->blue / 65535;
	}

	style->font = gnome_font_find_closest_from_weight_slant (
		gnome_font_get_family_name (job->priv->font),
		text_weight,
		(text_style & PANGO_STYLE_ITALIC) != 0,
		gnome_font_get_size (job->priv->font));
	
	return style;
}

static void
text_style_free (TextStyle *style)
{
	g_object_unref (style->font);
	g_free (style);
}

static TextStyle * 
get_style (GtkSourcePrintJob *job, const GtkTextIter *iter)
{
	GSList *tags, *t;
	GtkSourceTag *tag = NULL;
	TextStyle *style = NULL;
	
	if (job->priv->tag_styles == NULL)
	{
		job->priv->tag_styles = g_hash_table_new_full (
			g_direct_hash, g_direct_equal,
			NULL, (GDestroyNotify) text_style_free);
	}
	
	/* get the tags at iter */
	tags = gtk_text_iter_get_tags (iter);

	/* now find the GtkSourceTag (if any) which applies at iter */
	/* FIXME: this makes the assumption that the style at a given
	 * iter is only determined by one GtkSourceTag (the one with
	 * highest priority).  This is true for now, but could change
	 * in the future - Gustavo */
	t = tags;
	while (t != NULL)
	{
		if (GTK_IS_SOURCE_TAG (t->data))
			tag = t->data;
		t = g_slist_next (t);
	}
	g_slist_free (tags);

	/* now we lookup the tag style in the cache */
	if (tag != NULL)
	{
		style = g_hash_table_lookup (job->priv->tag_styles, tag);
		if (style == NULL)
		{
			/* create a style for the tag and cache it */
			style = text_style_new (job, tag);
			g_hash_table_insert (job->priv->tag_styles, tag, style);
		}
	}

	return style;
}

static void
set_style (GtkSourcePrintJob *job, TextStyle *style)
{
	if (style != NULL)
	{
		job->priv->current_font = style->font;
		job->priv->current_color =
			(((guint32) (style->red * 255) << 24) |
			 ((guint32) (style->green * 255) << 16) |
			 ((guint32) (style->blue * 255) << 8) |
			 0xff);
	}
	else
	{
		job->priv->current_font = job->priv->font;
		job->priv->current_color = DEFAULT_COLOR;
	}
}

/* ----- Text fetching functions */

static gboolean 
get_text_simple (GtkSourcePrintJob *job,
		 GtkTextIter       *start,
		 GtkTextIter       *end)
{
	GtkTextIter iter;

	while (gtk_text_iter_compare (start, end) < 0)
	{
		TextSegment *seg;
		
		/* get a line of text */
		iter = *start;
		gtk_text_iter_forward_line (&iter);
		if (gtk_text_iter_compare (&iter, end) > 0)
			iter = *end;

		seg = g_new0 (TextSegment, 1);
		seg->next = NULL;  /* only one segment per line, since there's no style change */
		seg->style = NULL; /* use default style */
		/* FIXME: handle invisible text properly.  This also
		 * assumes the text has no embedded images and
		 * stuff */
		seg->text = gtk_text_iter_get_slice (start, &iter);

		/* add the line of text to the job */
		job->priv->lines = g_slist_prepend (job->priv->lines, seg);

		/* advance to next line */
		*start = iter;
	}
	job->priv->lines = g_slist_reverse (job->priv->lines);

	return TRUE;
}

static gboolean 
get_text_with_style (GtkSourcePrintJob *job,
		     GtkTextIter       *start,
		     GtkTextIter       *end)
{
	GtkTextIter limit, iter;
	
	/* make sure the region to print is highlighted */
	_gtk_source_buffer_highlight_region (job->priv->buffer, start, end, TRUE);

	/* FIXME: handle invisible text properly.  This also assumes
	 * the text has no embedded images and stuff */
	while (gtk_text_iter_compare (start, end) < 0)
	{
		TextStyle *style;
		TextSegment *seg, *head;
		
		/* get the style at the start of the line */
		style = get_style (job, start);

		/* get a line of text - limit points to the end of the line */
		limit = *start;
		gtk_text_iter_forward_line (&limit);
		if (gtk_text_iter_compare (&limit, end) > 0)
			limit = *end;

		/* create the first segment for the line */
		head = seg = g_new0 (TextSegment, 1);
		seg->style = style;

		iter = *start;
		/* now we advance iter until next tag toggle and check
		 * if the style has changed; if it has, create a new
		 * segment; break at the line end. */
		while (gtk_text_iter_compare (&iter, &limit) < 0)
		{
			gtk_text_iter_forward_to_tag_toggle (&iter, NULL);
			if (gtk_text_iter_compare (&iter, &limit) > 0)
			{
				/* we have reached the limit, so break
				 * from the inner loop to close the
				 * line */
				break;
			}

			/* otherwise, check style changes */
			style = get_style (job, &iter);
			if (style != seg->style)
			{
				TextSegment *new_seg;
				
				/* style has changed, thus we need to
				 * create a new segment */
				/* close the current segment */
				seg->text = gtk_text_iter_get_slice (start, &iter);
				*start = iter;
				
				new_seg = g_new0 (TextSegment, 1);
				seg->next = new_seg;
				seg = new_seg;
				seg->style = style;
			}
		}
		
		/* close the line */
		seg->next = NULL;
		seg->text = gtk_text_iter_get_slice (start, &limit);

		/* add the line of text to the job */
		job->priv->lines = g_slist_prepend (job->priv->lines, head);

		/* advance to next line */
		*start = limit;
	}
	job->priv->lines = g_slist_reverse (job->priv->lines);

	return TRUE;
}

static gboolean 
get_text_to_print (GtkSourcePrintJob *job,
		   const GtkTextIter *start,
		   const GtkTextIter *end)
{
	GtkTextIter _start, _end;
	
	g_return_val_if_fail (start != NULL && end != NULL, FALSE);
	g_return_val_if_fail (job->priv->buffer != NULL, FALSE);

	_start = *start;
	_end = *end;

	/* erase any previous data */
	if (job->priv->lines != NULL)
	{
		free_lines (job->priv->lines);
		job->priv->lines = NULL;
	}
	if (job->priv->tag_styles != NULL)
	{
		g_hash_table_destroy (job->priv->tag_styles);
		job->priv->tag_styles = NULL;
	}
	/* free display lines here since they point to segments which
	 * have just been freed */
	if (job->priv->display_lines != NULL)
	{
		free_display_lines (job->priv->display_lines);
		job->priv->display_lines = NULL;
	}

	/* provide ordered iters */
	gtk_text_iter_order (&_start, &_end);

	/* save the first and last line numbers for future reference */
	job->priv->first_line_number = gtk_text_iter_get_line (&_start) + 1;
	job->priv->last_line_number = gtk_text_iter_get_line (&_end) + 1;

	if (!job->priv->highlight)
		return get_text_simple (job, &_start, &_end);
	else
		return get_text_with_style (job, &_start, &_end);
}

/* ----- Pagination functions */

static void
break_line (GtkSourcePrintJob *job,
	    TextSegment       *segment,
	    const gchar       *ptr,
	    gboolean           first_line_of_par)
{
	DisplayLine *dline;
	gdouble line_width;
	gint char_count;
	const gchar *word_ptr;
	TextSegment *word_seg;
	gint word_char_count;
	gunichar ch;

	line_width = 0.0;
	char_count = 0;

	/* these are the safe point at the last word break */
	word_ptr = ptr;
	word_seg = segment;
	word_char_count = 0;

	ch = g_utf8_get_char (ptr);

	if (!first_line_of_par)
	{
		/* eat up initial spaces */
		while (ch == ' ' || ch == '\t')
		{
			ptr = g_utf8_next_char (ptr);
			ch = g_utf8_get_char (ptr);
			if (ch == 0)
			{
				/* this should never happen, since we
				 * would first get the carriage
				 * return, but... */
				segment = segment->next;
				if (segment)
				{
					ptr = segment->text;
					ch = g_utf8_get_char (ptr);
				}
				else
					return;
			}
		}
	}

	/* create the display line */
	dline = g_new0 (DisplayLine, 1);
	dline->line_number = job->priv->line_number;
	dline->segment = segment;
	dline->start = ptr;

	/* check page boundaries */
	if (job->priv->available_height < job->priv->font_height)
	{
		/* "create" a new page */
		job->priv->page_count++;
		job->priv->available_height = job->priv->text_height;
	}
	job->priv->available_height -= job->priv->line_spacing;
	dline->page = job->priv->page_count;

	set_style (job, segment->style);
	
	while (ch != '\n')
	{
	       	gint glyph;
		
		++char_count;
		
		if (ch == '\t')
		{
			gdouble tab_stop = job->priv->width_of_tab;

			/* advance to next tab stop */
			while (line_width >= tab_stop)
				tab_stop += job->priv->width_of_tab;
			line_width = tab_stop;
		}
		else
		{
			ArtPoint adv;

			glyph = gnome_font_lookup_default (job->priv->current_font, ch);
			gnome_font_get_glyph_stdadvance (job->priv->current_font, glyph, &adv);
			if (adv.x > 0)
				line_width += adv.x;
			else
				/* To be as conservative as possible */
				line_width += (2 * job->priv->space_advance);
		}

		/* save word boundary */
		if (ch == ' ' || ch == '\t')
		{
			word_ptr = ptr;
			word_seg = segment;
			word_char_count = char_count;
		}
		
		if (line_width > job->priv->text_width)
		{
			if (job->priv->wrap_mode == GTK_WRAP_NONE)
				break;
			else if (job->priv->wrap_mode == GTK_WRAP_WORD)
			{
				/* handle words longer than the line */
				if (word_char_count != 0)
				{
					char_count = word_char_count;
					ptr = word_ptr;
					segment = word_seg;
				}
			}

			/* close the line */
			dline->char_count = char_count - 1;
			job->priv->display_lines = g_slist_prepend (
				job->priv->display_lines, dline);

			/* recursively break the rest of the line */
			break_line (job, segment, ptr, FALSE);
			
			return;
		}

		ptr = g_utf8_next_char (ptr);
		ch = g_utf8_get_char (ptr);
		if (ch == 0)
		{
			/* move to the next segment */
			segment = segment->next;
			if (segment == NULL)
				break;

			/* switch style */
			set_style (job, segment->style);
			ptr = segment->text;
			ch = g_utf8_get_char (ptr);
		}
	}

	/* add the last display line */
	dline->char_count = char_count;
	job->priv->display_lines = g_slist_prepend (job->priv->display_lines, dline);
}

static gboolean 
paginate_text (GtkSourcePrintJob *job)
{
	GSList *line;
	
	/* set these to zero so the first break_line creates a new page */
	job->priv->page_count = 0;
	job->priv->available_height = 0;
	job->priv->line_number = job->priv->first_line_number;
	line = job->priv->lines;
	while (line != NULL)
	{
		TextSegment *seg = line->data;
		
		break_line (job, seg, seg->text, TRUE);
		job->priv->line_number++;
		line = g_slist_next (line);
	}
	job->priv->display_lines = g_slist_reverse (job->priv->display_lines);

	/* FIXME: do we have any error condition which can force us to
	 * return FALSE? - Gustavo */
	return TRUE;
}

/* ---- Printing functions */

static void
begin_page (GtkSourcePrintJob *job)
{
	gnome_print_beginpage (job->priv->print_ctxt, NULL);

	g_signal_emit (job, print_job_signals [BEGIN_PAGE], 0);
}

static void
end_page (GtkSourcePrintJob *job)
{
	gnome_print_showpage (job->priv->print_ctxt);
}

static void 
print_line_number (GtkSourcePrintJob *job,
		   guint              line_number,
		   gdouble            x,
		   gdouble            y)
{
	gchar *num_str;
	gdouble len;
		
	num_str = g_strdup_printf ("%d", line_number);
	gnome_print_setfont (job->priv->print_ctxt, job->priv->numbers_font);
	len = gnome_font_get_width_utf8 (job->priv->numbers_font, num_str);
	x = x + job->priv->numbers_width - len - NUMBERS_TEXT_SEPARATION;
	gnome_print_moveto (job->priv->print_ctxt, x, y - 
			    gnome_font_get_ascender (job->priv->numbers_font));
	gnome_print_show (job->priv->print_ctxt, num_str);
	g_free (num_str);
}	

static void 
print_display_line (GtkSourcePrintJob *job,
		    DisplayLine       *dline,
		    gdouble            x,
		    gdouble            y)
{
	TextSegment *seg;
	const gchar *ptr;
	GnomeGlyphList *gl = NULL;
	gint char_count = 0;
	gboolean need_style;
	gdouble dx;
	
	seg = dline->segment;
	ptr = dline->start;
	need_style = TRUE;
	dx = 0.0;
	
	while (char_count < dline->char_count && seg != NULL)
	{
		gunichar ch;
	       	gint glyph;
		
		if (need_style)
		{
			/* create a new glyphlist with the style of
			 * the current segment */
			if (gl != NULL)
				gnome_glyphlist_unref (gl);
			
			set_style (job, seg->style);
			gl = gnome_glyphlist_from_text_dumb (job->priv->current_font,
							     job->priv->current_color,
							     0.0, 0.0, "");
			gnome_glyphlist_advance (gl, TRUE);
			gnome_glyphlist_moveto (gl, x + dx,
						y - gnome_font_get_ascender (
							job->priv->current_font));
			need_style = FALSE;
		}
		
		ch = g_utf8_get_char (ptr);
		++char_count;
		
		if (ch == '\t')
		{
			gdouble tab_stop = job->priv->width_of_tab;

			while (dx >= tab_stop)
				tab_stop += job->priv->width_of_tab;
			dx = tab_stop;

			/* we need a new "absolute" position */
			need_style = TRUE;

			ptr = g_utf8_next_char (ptr);
		}
		else if (ch == 0)
		{
			seg = seg->next;
			ptr = seg->text;
			
			/* don't count the segment terminator */
			--char_count;
			
			need_style = TRUE;
		}
		else
		{
			ArtPoint adv;

			glyph = gnome_font_lookup_default (job->priv->current_font, ch);
			gnome_font_get_glyph_stdadvance (job->priv->current_font, glyph, &adv);
			if (adv.x > 0)
				dx += adv.x;
			else
				/* To be as conservative as possible */
				dx += (2 * job->priv->space_advance);
			gnome_glyphlist_glyph (gl, glyph);

			ptr = g_utf8_next_char (ptr);
		}

		if (need_style)
		{
			/* flush (display) the current glyphlist */
			gnome_print_moveto (job->priv->print_ctxt, 0.0, 0.0);
			gnome_print_glyphlist (job->priv->print_ctxt, gl);
		}
	}

	if (gl != NULL)
	{
		/* flush the current (last) glyphlist */
		gnome_print_moveto (job->priv->print_ctxt, 0.0, 0.0);
		gnome_print_glyphlist (job->priv->print_ctxt, gl);
		gnome_glyphlist_unref (gl);
	}
}

static void
print_page (GtkSourcePrintJob *job)
{
	GSList *l;
	gdouble x, y;
	
	y = 0.0;
	x = 0.0;

	begin_page (job);
	y = job->priv->page_height -
		job->priv->doc_margin_top - job->priv->margin_top -
		job->priv->header_height;
	x = job->priv->doc_margin_left + job->priv->margin_left +
		job->priv->numbers_width;
	l = job->priv->current_display_line;

	while (l != NULL)
	{
		DisplayLine *dline = l->data;
		
		if (dline->page != job->priv->page)
			/* page is finished */
			break;
		
		if (dline->line_number != job->priv->line_number)
		{
			job->priv->line_number = dline->line_number;
			if (job->priv->print_numbers > 0 &&
			    (job->priv->printed_lines % job->priv->print_numbers) == 0)
			{
				print_line_number (job,
						   job->priv->line_number,
						   job->priv->doc_margin_left +
						   job->priv->margin_left,
						   y);
			}
			job->priv->printed_lines++;
		}

		print_display_line (job, dline, x, y);
		y -= job->priv->line_spacing;

		l = l->next;
	}
	end_page (job);
	job->priv->current_display_line = l;
}

static void
setup_for_print (GtkSourcePrintJob *job)
{
	job->priv->current_display_line = job->priv->display_lines;
	job->priv->line_number = 0;
	job->priv->printed_lines = 0;

	if (job->priv->print_job != NULL)
		g_object_unref (job->priv->print_job);
	if (job->priv->print_ctxt != NULL)
		g_object_unref (job->priv->print_ctxt);
	
	job->priv->print_job = gnome_print_job_new (job->priv->config);
	job->priv->print_ctxt = gnome_print_job_get_context (job->priv->print_job);
}

static void
print_job (GtkSourcePrintJob *job)
{
	while (job->priv->current_display_line != NULL)
	{
		DisplayLine *dline = job->priv->current_display_line->data;

		job->priv->page = dline->page;
		print_page (job);
	}

	gnome_print_job_close (job->priv->print_job);
}

static gboolean
idle_printing_handler (GtkSourcePrintJob *job)
{
	DisplayLine *dline;
	
	g_assert (job->priv->current_display_line != NULL);

	dline = job->priv->current_display_line->data;
	job->priv->page = dline->page;
	print_page (job);

	if (job->priv->current_display_line == NULL)
	{
		gnome_print_job_close (job->priv->print_job);
		job->priv->printing = FALSE;
		job->priv->idle_printing_tag = 0;

		g_signal_emit (job, print_job_signals [FINISHED], 0);
		/* after this the print job object is possibly
		 * destroyed (common use case) */
		
		return FALSE;
	}
	return TRUE;
}


/* Public API ------------------- */

GtkSourcePrintJob *
gtk_source_print_job_new (GnomePrintConfig  *config)
{
	GtkSourcePrintJob *job;

	g_return_val_if_fail (config == NULL || GNOME_IS_PRINT_CONFIG (config), NULL);

	job = GTK_SOURCE_PRINT_JOB (g_object_new (GTK_TYPE_SOURCE_PRINT_JOB, NULL));
	if (config != NULL)
		gtk_source_print_job_set_config (job, config);

	return job;
}

GtkSourcePrintJob *
gtk_source_print_job_new_with_buffer (GnomePrintConfig  *config,
				      GtkSourceBuffer   *buffer)
{
	GtkSourcePrintJob *job;

	g_return_val_if_fail (config == NULL || GNOME_IS_PRINT_CONFIG (config), NULL);
	g_return_val_if_fail (buffer == NULL || GTK_IS_SOURCE_BUFFER (buffer), NULL);

	job = gtk_source_print_job_new (config);
	if (buffer != NULL)
		gtk_source_print_job_set_buffer (job, buffer);

	return job;
}

/* --- print job basic configuration */

void
gtk_source_print_job_set_config (GtkSourcePrintJob *job,
				 GnomePrintConfig  *config)
{
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (GNOME_IS_PRINT_CONFIG (config));
	g_return_if_fail (!job->priv->printing);
	
	if (config == job->priv->config)
		return;
	
	if (job->priv->config != NULL)
		gnome_print_config_unref (job->priv->config);

	job->priv->config = config;
	gnome_print_config_ref (config);

	g_object_notify (G_OBJECT (job), "config");
}

GnomePrintConfig * 
gtk_source_print_job_get_config (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), NULL);

	ensure_print_config (job);
	
	return job->priv->config;
}

void 
gtk_source_print_job_set_buffer (GtkSourcePrintJob *job,
				 GtkSourceBuffer   *buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (!job->priv->printing);

	if (buffer == job->priv->buffer)
		return;
	
	if (job->priv->buffer != NULL)
		g_object_unref (job->priv->buffer);

	job->priv->buffer = buffer;
	g_object_ref (buffer);

	g_object_notify (G_OBJECT (job), "buffer");
}

GtkSourceBuffer *
gtk_source_print_job_get_buffer (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), NULL);

	return job->priv->buffer;
}

/* --- print job layout and style configuration */

void 
gtk_source_print_job_set_tabs_width (GtkSourcePrintJob *job,
				     guint              tabs_width)
{
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (!job->priv->printing);

	if (tabs_width == job->priv->tabs_width)
		return;
	
	job->priv->tabs_width = tabs_width;

	g_object_notify (G_OBJECT (job), "tabs_width");
}

guint 
gtk_source_print_job_get_tabs_width (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), 0);

	return job->priv->tabs_width;
}

void 
gtk_source_print_job_set_wrap_mode (GtkSourcePrintJob *job,
				    GtkWrapMode        wrap)
{
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (!job->priv->printing);

	if (wrap == job->priv->wrap_mode)
		return;
	
	job->priv->wrap_mode = wrap;

	g_object_notify (G_OBJECT (job), "wrap_mode");
}

GtkWrapMode 
gtk_source_print_job_get_wrap_mode (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), GTK_WRAP_NONE);

	return job->priv->wrap_mode;
}

void 
gtk_source_print_job_set_highlight (GtkSourcePrintJob *job,
				    gboolean           highlight)
{
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (!job->priv->printing);

	highlight = (highlight != FALSE);
	
	if (highlight == job->priv->highlight)
		return;
	
	job->priv->highlight = highlight;

	g_object_notify (G_OBJECT (job), "highlight");
}

gboolean 
gtk_source_print_job_get_highlight (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), FALSE);

	return job->priv->highlight;
}

void 
gtk_source_print_job_set_font (GtkSourcePrintJob *job,
			       const gchar       *font_name)
{
	GnomeFont *font;
	
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (font_name != NULL);
	g_return_if_fail (!job->priv->printing);

	font = gnome_font_find_closest_from_full_name (font_name);
	g_return_if_fail (font != NULL);
	
	if (font != job->priv->font)
	{
		if (job->priv->font != NULL)
			g_object_unref (job->priv->font);
		job->priv->font = font;
		g_object_ref (font);
		g_object_notify (G_OBJECT (job), "font");
	}
	
	gnome_font_unref (font);
}

gchar *
gtk_source_print_job_get_font (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), NULL);

	ensure_print_config (job);
	
	return construct_full_font_name (job->priv->font);
}

void 
gtk_source_print_job_setup_from_view (GtkSourcePrintJob *job,
				      GtkSourceView     *view)
{
	GtkSourceBuffer *buffer = NULL;
	GnomeFont *font;
	
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));
	g_return_if_fail (!job->priv->printing);

	if (GTK_IS_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view))))
		buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	
	if (job->priv->buffer == NULL && buffer != NULL)
		gtk_source_print_job_set_buffer (job, buffer);

	gtk_source_print_job_set_tabs_width (job, gtk_source_view_get_tabs_width (view));
	if (buffer != NULL)
		gtk_source_print_job_set_highlight (job, gtk_source_buffer_get_highlight (buffer));
	gtk_source_print_job_set_wrap_mode (
		job, gtk_text_view_get_wrap_mode (GTK_TEXT_VIEW (view)));

	font = find_gnome_font_from_widget (GTK_WIDGET (view));
	if (job->priv->font != NULL)
		g_object_unref (job->priv->font);
	job->priv->font = font;
	g_object_notify (G_OBJECT (job), "font");
}

void 
gtk_source_print_job_set_numbers_font (GtkSourcePrintJob *job,
				       const gchar       *font_name)
{
	GnomeFont *font = NULL;
	
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (!job->priv->printing);

	if (font_name != NULL)
		font = gnome_font_find_closest_from_full_name (font_name);
	
	if (font == job->priv->numbers_font)
	{
		g_object_unref (font);
		return;
	}

	if (job->priv->numbers_font != NULL)
		g_object_unref (job->priv->numbers_font);
	job->priv->numbers_font = font;
	g_object_notify (G_OBJECT (job), "numbers_font");
}

gchar *
gtk_source_print_job_get_numbers_font (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), NULL);

	if (job->priv->numbers_font != NULL)
		return construct_full_font_name (job->priv->numbers_font);
	else
		return NULL;
}

void 
gtk_source_print_job_set_print_numbers (GtkSourcePrintJob *job,
					guint              interval)
{
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (!job->priv->printing);

	if (interval == job->priv->print_numbers)
		return;
	
	job->priv->print_numbers = interval;

	g_object_notify (G_OBJECT (job), "print_numbers");
}

guint 
gtk_source_print_job_get_print_numbers (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), 0);

	return job->priv->print_numbers;
}

void 
gtk_source_print_job_set_text_margins (GtkSourcePrintJob *job,
				       gdouble            top,
				       gdouble            bottom,
				       gdouble            left,
				       gdouble            right)
{
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (!job->priv->printing);

	if (top >= 0)
		job->priv->margin_top = top;
	if (bottom >= 0)
		job->priv->margin_bottom = bottom;
	if (left >= 0)
		job->priv->margin_left = left;
	if (right >= 0)
		job->priv->margin_right = right;
}

void 
gtk_source_print_job_get_text_margins (GtkSourcePrintJob *job,
				       gdouble           *top,
				       gdouble           *bottom,
				       gdouble           *left,
				       gdouble           *right)
{
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));

	if (top != NULL)
		*top = job->priv->margin_top;
	if (bottom != NULL)
		*bottom = job->priv->margin_bottom;
	if (left != NULL)
		*left = job->priv->margin_left;
	if (right != NULL)
		*right = job->priv->margin_right;
}

/* --- printing operations */

static gboolean
gtk_source_print_job_prepare (GtkSourcePrintJob *job,
			      const GtkTextIter *start,
			      const GtkTextIter *end)
{
	PROFILE (GTimer *timer);
	
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), FALSE);
	g_return_val_if_fail (!job->priv->printing, FALSE);
	g_return_val_if_fail (job->priv->buffer != NULL, FALSE);
	g_return_val_if_fail (start != NULL && end != NULL, FALSE);

	/* make sure we have a sane configuration to start printing */
	ensure_print_config (job);

	PROFILE (timer = g_timer_new ());

	/* get the text to print */
	if (!get_text_to_print (job, start, end))
		return FALSE;

	PROFILE (g_message ("get_text_to_print: %.2f", g_timer_elapsed (timer, NULL)));

	/* check margins */
	if (!update_page_size_and_margins (job))
		return FALSE;

	/* split the document in pages */
	if (!paginate_text (job))
		return FALSE;

	PROFILE ({
		g_message ("paginate_text: %.2f", g_timer_elapsed (timer, NULL));
		g_timer_destroy (timer);
	});

	return TRUE;
}

GnomePrintJob *
gtk_source_print_job_print (GtkSourcePrintJob *job)
{
	GtkTextIter start, end;

	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), NULL);
	g_return_val_if_fail (!job->priv->printing, NULL);
	g_return_val_if_fail (job->priv->buffer != NULL, NULL);

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (job->priv->buffer), &start, &end);

	return gtk_source_print_job_print_range (job, &start, &end);
}

GnomePrintJob *
gtk_source_print_job_print_range (GtkSourcePrintJob *job,
				  const GtkTextIter *start,
				  const GtkTextIter *end)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), NULL);
	g_return_val_if_fail (!job->priv->printing, NULL);
	g_return_val_if_fail (job->priv->buffer != NULL, NULL);
	g_return_val_if_fail (start != NULL && end != NULL, NULL);

	if (!gtk_source_print_job_prepare (job, start, end))
		return NULL;

	/* real work starts here */
	setup_for_print (job);

	job->priv->printing = TRUE;
	print_job (job);
	job->priv->printing = FALSE;

	g_object_ref (job->priv->print_job);
	return job->priv->print_job;
}

/* --- asynchronous printing */

gboolean 
gtk_source_print_job_print_range_async (GtkSourcePrintJob *job,
					const GtkTextIter *start,
					const GtkTextIter *end)
{
	GSource *idle_source;

	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), FALSE);
	g_return_val_if_fail (!job->priv->printing, FALSE);
	g_return_val_if_fail (job->priv->buffer != NULL, FALSE);
	g_return_val_if_fail (start != NULL && end != NULL, FALSE);

	if (!gtk_source_print_job_prepare (job, start, end))
		return FALSE;

	/* real work starts here */
	setup_for_print (job);
	if (job->priv->current_display_line == NULL)
		return FALSE;
	
	/* setup the idle handler to print each page at a time */
	idle_source = g_idle_source_new ();
	g_source_set_closure (idle_source,
			      g_cclosure_new_object ((GCallback) idle_printing_handler,
						     G_OBJECT (job)));
	job->priv->idle_printing_tag = g_source_attach (idle_source, NULL);
	g_source_unref (idle_source);

	job->priv->printing = TRUE;

	return TRUE;
}

void 
gtk_source_print_job_cancel (GtkSourcePrintJob *job)
{
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (job->priv->printing);

	if (job->priv->idle_printing_tag > 0)
	{
		g_source_remove (job->priv->idle_printing_tag);
		job->priv->current_display_line = NULL;
		job->priv->idle_printing_tag = 0;
		job->priv->printing = FALSE;
		g_object_unref (job->priv->print_job);
		g_object_unref (job->priv->print_ctxt);
		job->priv->print_job = NULL;
		job->priv->print_ctxt = NULL;
	}
}

GnomePrintJob *
gtk_source_print_job_get_print_job (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), NULL);

	g_object_ref (job->priv->print_job);
	return job->priv->print_job;
}

/* --- information for asynchronous ops and headers and footers callback */

guint 
gtk_source_print_job_get_page (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), 0);
	g_return_val_if_fail (job->priv->printing, 0);

	return job->priv->page;
}

guint 
gtk_source_print_job_get_page_count (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), 0);

	return job->priv->page_count;
}

GnomePrintContext *
gtk_source_print_job_get_print_context (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), NULL);
	g_return_val_if_fail (job->priv->printing, NULL);

	return job->priv->print_ctxt;
}

/* ---- Header and footer (default implementation) */

/* Most of this code taken from GLib's g_date_strftime() in gdate.c
 * GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald */

static gchar *
strdup_strftime (const gchar *format, const struct tm *tm)
{
	gsize locale_format_len = 0;
	gchar *locale_format;
	gsize tmplen;
	gchar *tmpbuf;
	gsize tmpbufsize;
	gchar *convbuf;
	gsize convlen = 0;
	GError *error = NULL;

	g_return_val_if_fail (format != NULL, NULL);
	g_return_val_if_fail (tm != NULL, NULL);

	locale_format = g_locale_from_utf8 (format, -1, NULL, &locale_format_len, &error);

	if (error)
	{
		g_warning (G_STRLOC "Error converting format to locale encoding: %s",
			   error->message);
		g_error_free (error);
		
		return NULL;
	}

	tmpbufsize = MAX (128, locale_format_len * 2);
	while (TRUE)
	{
		tmpbuf = g_malloc (tmpbufsize);
		
		/* Set the first byte to something other than '\0', to be able to
		 * recognize whether strftime actually failed or just returned "".
		 */
		tmpbuf[0] = '\1';
		tmplen = strftime (tmpbuf, tmpbufsize, locale_format, tm);
		
		if (tmplen == 0 && tmpbuf[0] != '\0')
		{
			g_free (tmpbuf);
			tmpbufsize *= 2;
			
			if (tmpbufsize > 65536)
			{
				g_warning (G_STRLOC "Maximum buffer size for strdup_strftime "
					   "exceeded: giving up");
				g_free (locale_format);
				return NULL;
			}
		}
		else
			break;
	}
	g_free (locale_format);

	convbuf = g_locale_to_utf8 (tmpbuf, tmplen, NULL, &convlen, &error);
	g_free (tmpbuf);

	if (error)
	{
		g_warning (G_STRLOC "Error converting results of strftime to UTF-8: %s",
			   error->message);
		g_error_free (error);
		
		return NULL;
	}

	return convbuf;
}

static gchar *
evaluate_format_string (GtkSourcePrintJob *job, const gchar *format)
{
	GString *eval;
	gchar *eval_str, *retval;
	const struct tm *tm;
	time_t now;
	gunichar ch;
	
	/* get time */
	time (&now);
	tm = localtime (&now);

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
				g_string_append_printf (eval, "%d", job->priv->page);
			else if (ch == 'Q')
				g_string_append_printf (eval, "%d", job->priv->page_count);
			else
			{
				g_string_append_c (eval, '%');
				g_string_append_unichar (eval, ch);
			}
		}
		else
			g_string_append_unichar (eval, ch);

		format = g_utf8_next_char (format);
		ch = g_utf8_get_char (format);
	}

	eval_str = g_string_free (eval, FALSE);
	retval = strdup_strftime (eval_str, tm);
	g_free (eval_str);

	return retval;
}

static void 
default_print_header (GtkSourcePrintJob *job,
		      gdouble            x,
		      gdouble            y)
{
	gdouble len;
	gchar *text;
	gdouble yy, xx;
	gdouble width;

	gnome_print_setfont (job->priv->print_ctxt, job->priv->header_footer_font);
	width = job->priv->text_width + job->priv->numbers_width;
	
	yy = y - gnome_font_get_ascender (job->priv->header_footer_font);

	/* left format */
	if (job->priv->header_format_left != NULL)
	{
		text = evaluate_format_string (job, job->priv->header_format_left);
		if (text != NULL)
		{
			xx = x;
			gnome_print_moveto (job->priv->print_ctxt, xx, yy);
			gnome_print_show (job->priv->print_ctxt, text);
			g_free (text);
		}
	}
	
	/* right format */
	if (job->priv->header_format_right != NULL)
	{
		text = evaluate_format_string (job, job->priv->header_format_right);
		if (text != NULL)
		{
			len = gnome_font_get_width_utf8 (job->priv->header_footer_font, text);
			xx = x + width - len;
			gnome_print_moveto (job->priv->print_ctxt, xx, yy);
			gnome_print_show (job->priv->print_ctxt, text);
			g_free (text);
		}
	}

	/* center format */
	if (job->priv->header_format_center != NULL)
	{
		text = evaluate_format_string (job, job->priv->header_format_center);
		if (text != NULL)
		{
			len = gnome_font_get_width_utf8 (job->priv->header_footer_font, text);
			xx = x + (width - len) / 2;
			gnome_print_moveto (job->priv->print_ctxt, xx, yy);
			gnome_print_show (job->priv->print_ctxt, text);
			g_free (text);
		}
	}

	/* separator */
	if (job->priv->header_separator)
	{
		yy = y - (SEPARATOR_SPACING * gnome_font_get_size (job->priv->header_footer_font));
		gnome_print_setlinewidth (job->priv->print_ctxt, SEPARATOR_LINE_WIDTH);
		gnome_print_moveto (job->priv->print_ctxt, x, yy);
		gnome_print_lineto (job->priv->print_ctxt, x + width, yy);
		gnome_print_stroke (job->priv->print_ctxt);
	}
}

static void 
default_print_footer (GtkSourcePrintJob *job,
		      gdouble            x,
		      gdouble            y)
{
	gdouble len;
	gchar *text;
	gdouble yy, xx;
	gdouble width;

	gnome_print_setfont (job->priv->print_ctxt, job->priv->header_footer_font);
	width = job->priv->text_width + job->priv->numbers_width;

	yy = y - job->priv->footer_height +
		gnome_font_get_descender (job->priv->header_footer_font);

	/* left format */
	if (job->priv->footer_format_left != NULL)
	{
		text = evaluate_format_string (job, job->priv->footer_format_left);
		if (text != NULL)
		{
			xx = x;
			gnome_print_moveto (job->priv->print_ctxt, xx, yy);
			gnome_print_show (job->priv->print_ctxt, text);
			g_free (text);
		}
	}
	
	/* right format */
	if (job->priv->footer_format_right != NULL)
	{
		text = evaluate_format_string (job, job->priv->footer_format_right);
		if (text != NULL)
		{
			len = gnome_font_get_width_utf8 (job->priv->header_footer_font, text);
			xx = x + width - len;
			gnome_print_moveto (job->priv->print_ctxt, xx, yy);
			gnome_print_show (job->priv->print_ctxt, text);
			g_free (text);
		}
	}

	/* center format */
	if (job->priv->footer_format_center != NULL)
	{
		text = evaluate_format_string (job, job->priv->footer_format_center);
		if (text != NULL)
		{
			len = gnome_font_get_width_utf8 (job->priv->header_footer_font, text);
			xx = x + (width - len) / 2;
			gnome_print_moveto (job->priv->print_ctxt, xx, yy);
			gnome_print_show (job->priv->print_ctxt, text);
			g_free (text);
		}
	}

	/* separator */
	if (job->priv->footer_separator)
	{
		yy = y - job->priv->footer_height +
			(SEPARATOR_SPACING * gnome_font_get_size (job->priv->header_footer_font));
		gnome_print_setlinewidth (job->priv->print_ctxt, SEPARATOR_LINE_WIDTH);
		gnome_print_moveto (job->priv->print_ctxt, x, yy);
		gnome_print_lineto (job->priv->print_ctxt, x + width, yy);
		gnome_print_stroke (job->priv->print_ctxt);
	}
}

void 
gtk_source_print_job_set_print_header (GtkSourcePrintJob *job,
				       gboolean           setting)
{
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (!job->priv->printing);

	setting = (setting != FALSE);
	
	if (setting == job->priv->print_header)
		return;
	
	job->priv->print_header = setting;

	g_object_notify (G_OBJECT (job), "print_header");
}

gboolean 
gtk_source_print_job_get_print_header (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), FALSE);

	return job->priv->print_header;
}

void 
gtk_source_print_job_set_print_footer (GtkSourcePrintJob *job,
				       gboolean           setting)
{
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (!job->priv->printing);

	setting = (setting != FALSE);
	
	if (setting == job->priv->print_footer)
		return;
	
	job->priv->print_footer = setting;

	g_object_notify (G_OBJECT (job), "print_footer");
}

gboolean 
gtk_source_print_job_get_print_footer (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), FALSE);

	return job->priv->print_footer;
}

void 
gtk_source_print_job_set_header_footer_font (GtkSourcePrintJob *job,
					     const gchar       *font_name)
{
	GnomeFont *font = NULL;
	
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (!job->priv->printing);

	if (font_name != NULL)
		font = gnome_font_find_closest_from_full_name (font_name);
	
	if (font == job->priv->header_footer_font)
	{
		g_object_unref (font);
		return;
	}

	if (job->priv->header_footer_font != NULL)
		g_object_unref (job->priv->header_footer_font);
	job->priv->header_footer_font = font;
	g_object_notify (G_OBJECT (job), "header_footer_font");
}

gchar *
gtk_source_print_job_get_header_footer_font (GtkSourcePrintJob *job)
{
	g_return_val_if_fail (GTK_IS_SOURCE_PRINT_JOB (job), NULL);

	if (job->priv->header_footer_font != NULL)
		return construct_full_font_name (job->priv->header_footer_font);
	else
		return NULL;
}

void 
gtk_source_print_job_set_header_format (GtkSourcePrintJob *job,
					const gchar       *left,
					const gchar       *center,
					const gchar       *right,
					gboolean           separator)
{
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (!job->priv->printing);

	/* FIXME: validate given strings? */
	g_free (job->priv->header_format_left);
	g_free (job->priv->header_format_center);
	g_free (job->priv->header_format_right);
	job->priv->header_format_left = g_strdup (left);
	job->priv->header_format_center = g_strdup (center);
	job->priv->header_format_right = g_strdup (right);
	job->priv->header_separator = separator;
}

void 
gtk_source_print_job_set_footer_format (GtkSourcePrintJob *job,
					const gchar       *left,
					const gchar       *center,
					const gchar       *right,
					gboolean           separator)
{
	g_return_if_fail (GTK_IS_SOURCE_PRINT_JOB (job));
	g_return_if_fail (!job->priv->printing);

	/* FIXME: validate given strings? */
	g_free (job->priv->footer_format_left);
	g_free (job->priv->footer_format_center);
	g_free (job->priv->footer_format_right);
	job->priv->footer_format_left = g_strdup (left);
	job->priv->footer_format_center = g_strdup (center);
	job->priv->footer_format_right = g_strdup (right);
	job->priv->footer_separator = separator;
}
