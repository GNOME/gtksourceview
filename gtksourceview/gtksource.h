/*
 * This file is part of gtksourceview
 *
 * gtksourceview is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * gtksourceview is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#define GTK_SOURCE_H_INSIDE

#include <gtksourceview/completion-providers/words/gtksourcecompletionwords.h>
#include <gtksourceview/gtksourcetypes.h>
#include <gtksourceview/gtksourcebuffer.h>
#include <gtksourceview/gtksourcecompletioncontext.h>
#include <gtksourceview/gtksourcecompletion.h>
#include <gtksourceview/gtksourcecompletioninfo.h>
#include <gtksourceview/gtksourcecompletionitem.h>
#include <gtksourceview/gtksourcecompletionproposal.h>
#include <gtksourceview/gtksourcecompletionprovider.h>
#include <gtksourceview/gtksourceencoding.h>
#include <gtksourceview/gtksourcefile.h>
#include <gtksourceview/gtksourcefileloader.h>
#include <gtksourceview/gtksourcefilesaver.h>
#include <gtksourceview/gtksourcegutter.h>
#include <gtksourceview/gtksourcegutterrenderer.h>
#include <gtksourceview/gtksourcegutterrenderertext.h>
#include <gtksourceview/gtksourcegutterrendererpixbuf.h>
#include <gtksourceview/gtksourceinit.h>
#include <gtksourceview/gtksourcelanguage.h>
#include <gtksourceview/gtksourcelanguagemanager.h>
#include <gtksourceview/gtksourcemap.h>
#include <gtksourceview/gtksourcemark.h>
#include <gtksourceview/gtksourcemarkattributes.h>
#include <gtksourceview/gtksourceprintcompositor.h>
#include <gtksourceview/gtksourceregion.h>
#include <gtksourceview/gtksourcesearchcontext.h>
#include <gtksourceview/gtksourcesearchsettings.h>
#include <gtksourceview/gtksourcespacedrawer.h>
#include <gtksourceview/gtksourcestyle.h>
#include <gtksourceview/gtksourcestylescheme.h>
#include <gtksourceview/gtksourcestyleschemechooser.h>
#include <gtksourceview/gtksourcestyleschemechooserbutton.h>
#include <gtksourceview/gtksourcestyleschemechooserwidget.h>
#include <gtksourceview/gtksourcestyleschememanager.h>
#include <gtksourceview/gtksourcetag.h>
#include <gtksourceview/gtksourceundomanager.h>
#include <gtksourceview/gtksourceutils.h>
#include <gtksourceview/gtksourceversion.h>
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksource-enumtypes.h>
#include <gtksourceview/gtksourceautocleanups.h>

#undef GTK_SOURCE_H_INSIDE
