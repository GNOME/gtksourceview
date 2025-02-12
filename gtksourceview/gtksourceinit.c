/*
 * This file is part of GtkSourceView
 *
 * Copyright 2016, 2017 - Sébastien Wilmet <swilmet@gnome.org>
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

#include "gtksourceview-gresources.h"

#include <glib/gi18n-lib.h>

#include "gtksourcebuffer.h"
#include "gtksourcebufferinputstream-private.h"
#include "gtksourcebufferoutputstream-private.h"
#include "gtksourcecompletion.h"
#include "gtksourcecompletioncontext.h"
#include "gtksourcecompletionproposal.h"
#include "gtksourcecompletionprovider.h"
#include "gtksourcefileloader.h"
#include "gtksourcefilesaver.h"
#include "gtksourcegutterrenderer.h"
#include "gtksourcegutterrendererpixbuf.h"
#include "gtksourcegutterrenderertext.h"
#include "gtksourceinit.h"
#include "gtksourcelanguagemanager-private.h"
#include "gtksourcemap.h"
#include "gtksourcesnippetmanager-private.h"
#include "gtksourcestyleschemechooser.h"
#include "gtksourcestyleschemechooserbutton.h"
#include "gtksourcestyleschemechooserwidget.h"
#include "gtksourcestyleschememanager-private.h"
#include "gtksourcestyleschememanager-private.h"
#include "gtksourcestyleschemepreview.h"
#include "gtksourceutils-private.h"
#include "gtksourceview.h"
#include "gtksourcevimimcontext.h"

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HMODULE gtk_source_dll;

BOOL WINAPI DllMain (HINSTANCE hinstDLL,
		     DWORD     fdwReason,
		     LPVOID    lpvReserved);

BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
	 DWORD     fdwReason,
	 LPVOID    lpvReserved)
{
	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
			gtk_source_dll = hinstDLL;
			break;

		default:
			/* do nothing */
			break;
	}

	return TRUE;
}
#endif /* G_OS_WIN32 */

#ifdef OS_OSX
#include <Cocoa/Cocoa.h>

static gchar *
dirs_os_x_get_bundle_resource_dir (void)
{
	NSAutoreleasePool *pool;
	gchar *str = NULL;
	NSString *path;

	pool = [[NSAutoreleasePool alloc] init];

	if ([[NSBundle mainBundle] bundleIdentifier] == nil)
	{
		[pool release];
		return NULL;
	}

	path = [[NSBundle mainBundle] resourcePath];

	if (!path)
	{
		[pool release];
		return NULL;
	}

	str = g_strdup ([path UTF8String]);
	[pool release];
	return str;
}

static gchar *
dirs_os_x_get_locale_dir (void)
{
	gchar *res_dir;
	gchar *ret;

	res_dir = dirs_os_x_get_bundle_resource_dir ();

	if (res_dir == NULL)
	{
		ret = g_build_filename (DATADIR, "locale", NULL);
	}
	else
	{
		ret = g_build_filename (res_dir, "share", "locale", NULL);
		g_free (res_dir);
	}

	return ret;
}
#endif /* OS_OSX */

static gchar *
get_locale_dir (void)
{
	gchar *locale_dir;

#if defined (G_OS_WIN32)
	gchar *win32_dir;

	win32_dir = g_win32_get_package_installation_directory_of_module (gtk_source_dll);

	locale_dir = g_build_filename (win32_dir, "share", "locale", NULL);

	g_free (win32_dir);
#elif defined (OS_OSX)
	locale_dir = dirs_os_x_get_locale_dir ();
#else
	locale_dir = g_build_filename (DATADIR, "locale", NULL);
#endif

	return locale_dir;
}

static gpointer
load_builder_blocks (gpointer data)
{
	return _gtk_source_utils_get_builder_blocks ();
}

/**
 * gtk_source_init:
 *
 * Initializes the GtkSourceView library (e.g. for the internationalization).
 *
 * This function can be called several times, but is meant to be called at the
 * beginning of main(), before any other GtkSourceView function call.
 *
 * The counterpart to this function is [func@finalize] which can be convenient
 * when using memory debugging tools.
 */
void
gtk_source_init (void)
{
	static gboolean done = FALSE;

	if (!done)
	{
		GdkDisplay *display;
		gchar *locale_dir;

		locale_dir = get_locale_dir ();
		bindtextdomain (GETTEXT_PACKAGE, locale_dir);
		g_free (locale_dir);

		bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

		/* Start loading our BuilderBlocks font very early on a thread
		 * so that it doesn't slow down application startup.
		 */
		g_thread_unref (g_thread_new ("[gtksourceview-font]", load_builder_blocks, NULL));

		/* Due to potential deadlocks when registering types, we need to
		 * ensure the dependent private class GtkSourceBufferOutputStream
		 * and GtkSourceBufferInputStream have been registered up front.
		 *
		 * See https://bugzilla.gnome.org/show_bug.cgi?id=780216
		 */

		g_type_ensure (GTK_SOURCE_TYPE_BUFFER);
		g_type_ensure (GTK_SOURCE_TYPE_BUFFER_INPUT_STREAM);
		g_type_ensure (GTK_SOURCE_TYPE_BUFFER_OUTPUT_STREAM);
		g_type_ensure (GTK_SOURCE_TYPE_COMPLETION);
		g_type_ensure (GTK_SOURCE_TYPE_COMPLETION_CONTEXT);
		g_type_ensure (GTK_SOURCE_TYPE_COMPLETION_PROVIDER);
		g_type_ensure (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL);
		g_type_ensure (GTK_SOURCE_TYPE_FILE_LOADER);
		g_type_ensure (GTK_SOURCE_TYPE_FILE_SAVER);
		g_type_ensure (GTK_SOURCE_TYPE_GUTTER_RENDERER);
		g_type_ensure (GTK_SOURCE_TYPE_GUTTER_RENDERER_TEXT);
		g_type_ensure (GTK_SOURCE_TYPE_GUTTER_RENDERER_PIXBUF);
		g_type_ensure (GTK_SOURCE_TYPE_MAP);
		g_type_ensure (GTK_SOURCE_TYPE_STYLE_SCHEME_CHOOSER);
		g_type_ensure (GTK_SOURCE_TYPE_STYLE_SCHEME_CHOOSER_BUTTON);
		g_type_ensure (GTK_SOURCE_TYPE_STYLE_SCHEME_CHOOSER_WIDGET);
		g_type_ensure (GTK_SOURCE_TYPE_STYLE_SCHEME_PREVIEW);
		g_type_ensure (GTK_SOURCE_TYPE_VIEW);
		g_type_ensure (GTK_SOURCE_TYPE_VIM_IM_CONTEXT);

		display = gdk_display_get_default ();

		if (display != NULL)
		{
			GtkCssProvider *css_provider;
			GtkIconTheme *icon_theme;

			/* Setup default CSS styling for widgetry */
			css_provider = gtk_css_provider_new ();
			gtk_css_provider_load_from_resource (css_provider,
			                                     "/org/gnome/gtksourceview/css/GtkSourceView.css");
			gtk_style_context_add_provider_for_display (display,
			                                            GTK_STYLE_PROVIDER (css_provider),
			                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION-1);
			g_clear_object (&css_provider);

			/* We need an additional provider that ensures that the application cannot set
			 * a background for "textview text" which would end up drawing the background
			 * twice for textview, drawing over our right-margin. See gtksourceview.c for
			 * details on why we draw the right-margin from gtk_source_view_snapshot().
			 */
			css_provider = gtk_css_provider_new ();
			gtk_css_provider_load_from_string (css_provider,
			                                   "textview.GtkSourceView text {background: transparent;}\n"
			                                   "textview.GtkSourceMap text {background: transparent;}\n");
			gtk_style_context_add_provider_for_display (display,
			                                            GTK_STYLE_PROVIDER (css_provider),
			                                            G_MAXINT);
			g_clear_object (&css_provider);

			/* Add path to internal scalable icons */
			icon_theme = gtk_icon_theme_get_for_display (display);
			gtk_icon_theme_add_search_path (icon_theme, HICOLORDIR);
		}

		done = TRUE;
	}
}

/**
 * gtk_source_finalize:
 *
 * Free the resources allocated by GtkSourceView. For example it unrefs the
 * singleton objects.
 *
 * It is not mandatory to call this function, it's just to be friendlier to
 * memory debugging tools. This function is meant to be called at the end of
 * main(). It can be called several times.
 */

/* Another way is to use a DSO destructor, see gconstructor.h in GLib.
 *
 * The advantage of calling gtk_source_finalize() at the end of main() is that
 * gobject-list [1] correctly reports that all GtkSource* objects have been
 * finalized when quitting the application. On the other hand a DSO destructor
 * runs after the gobject-list's last output, so it's much less convenient, see:
 * commit e761de9c2bee90c232875bbc41e6e73e1f63e145
 *
 * [1] A tool for debugging the lifetime of GObjects:
 * https://github.com/danni/gobject-list
 */
void
gtk_source_finalize (void)
{
	static gboolean done = FALSE;

	/* Unref the singletons only once, even if this function is called
	 * multiple times, to see if a reference is not released correctly.
	 * Normally the singleton have a ref count of 1. If for some reason the
	 * ref count is increased somewhere, it needs to be decreased
	 * accordingly, at the right place.
	 */
	if (!done)
	{
		GtkSourceLanguageManager *language_manager;
		GtkSourceStyleSchemeManager *style_scheme_manager;
		GtkSourceSnippetManager *snippet_manager;

		g_resources_register (gtksourceview_get_resource ());

		language_manager = _gtk_source_language_manager_peek_default ();
		g_clear_object (&language_manager);

		style_scheme_manager = _gtk_source_style_scheme_manager_peek_default ();
		g_clear_object (&style_scheme_manager);

		snippet_manager = _gtk_source_snippet_manager_peek_default ();
		g_clear_object (&snippet_manager);

		done = TRUE;
	}
}
