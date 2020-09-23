/*
 * This file is part of GtkSourceView
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib.h>

#ifndef GETTEXT_PACKAGE
# error "config.h was not included before gtksourcetrace.h."
#endif

#ifdef HAVE_SYSPROF
# include <sysprof-capture.h>
#endif

G_BEGIN_DECLS

#ifdef HAVE_SYSPROF
# define GTK_SOURCE_PROFILER_BEGIN_MARK \
  G_STMT_START { \
    gint64 __begin_time = SYSPROF_CAPTURE_CURRENT_TIME;
# define GTK_SOURCE_PROFILER_END_MARK(name, message) \
    G_STMT_START { \
      gint64 __duration = SYSPROF_CAPTURE_CURRENT_TIME - __begin_time; \
      sysprof_collector_mark (__begin_time, __duration, "GtkSourceView", name, message); \
    } G_STMT_END; \
  } G_STMT_END
# define GTK_SOURCE_PROFILER_MARK(duration, name, message) \
  G_STMT_START { \
    sysprof_collector_mark (SYSPROF_CAPTURE_CURRENT_TIME - duration, \
                            duration, "GtkSourceView", name, message) \
  } G_STMT_END
#else
# define GTK_SOURCE_PROFILER_MARK(duration, name, message) \
  G_STMT_START { } G_STMT_END
# define GTK_SOURCE_PROFILER_BEGIN_MARK G_STMT_START {
# define GTK_SOURCE_PROFILER_END_MARK(name, message) } G_STMT_END
#endif

G_END_DECLS
