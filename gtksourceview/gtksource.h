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

#include "gtksourcetypes.h"
#include "gtksourceannotation.h"
#include "gtksourceannotations.h"
#include "gtksourceannotationprovider.h"
#include "gtksourcebuffer.h"
#include "gtksourcecompletion.h"
#include "gtksourcecompletioncell.h"
#include "gtksourcecompletioncontext.h"
#include "gtksourcecompletionproposal.h"
#include "gtksourcecompletionprovider.h"
#include "gtksourceencoding.h"
#include "gtksourcefile.h"
#include "gtksourcefileloader.h"
#include "gtksourcefilesaver.h"
#include "gtksourcegutter.h"
#include "gtksourcegutterrenderer.h"
#include "gtksourcegutterrenderertext.h"
#include "gtksourcegutterrendererpixbuf.h"
#include "gtksourcehover.h"
#include "gtksourcehovercontext.h"
#include "gtksourcehoverdisplay.h"
#include "gtksourcehoverprovider.h"
#include "gtksourceindenter.h"
#include "gtksourceinit.h"
#include "gtksourcelanguage.h"
#include "gtksourcelanguagemanager.h"
#include "gtksourcegutterlines.h"
#include "gtksourcemap.h"
#include "gtksourcemark.h"
#include "gtksourcemarkattributes.h"
#include "gtksourceprintcompositor.h"
#include "gtksourceregion.h"
#include "gtksourcescheduler.h"
#include "gtksourcesearchcontext.h"
#include "gtksourcesearchsettings.h"
#include "gtksourcesnippet.h"
#include "gtksourcesnippetchunk.h"
#include "gtksourcesnippetcontext.h"
#include "gtksourcesnippetmanager.h"
#include "gtksourcespacedrawer.h"
#include "gtksourcestyle.h"
#include "gtksourcestylescheme.h"
#include "gtksourcestyleschemechooser.h"
#include "gtksourcestyleschemechooserbutton.h"
#include "gtksourcestyleschemechooserwidget.h"
#include "gtksourcestyleschememanager.h"
#include "gtksourcestyleschemepreview.h"
#include "gtksourcetag.h"
#include "gtksourceutils.h"
#include "gtksourceversion.h"
#include "gtksourceview.h"
#include "gtksourcevimimcontext.h"
#include "gtksource-enumtypes.h"

#include "completion-providers/words/gtksourcecompletionwords.h"
#include "completion-providers/snippets/gtksourcecompletionsnippets.h"

#undef GTK_SOURCE_H_INSIDE
