/*  gtksourcefold-private.h
 *
 *  Copyright (C) 2005 - Jeroen Zwartepoorte <jeroen.zwartepoorte@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_SOURCE_FOLD_PRIVATE_H__
#define __GTK_SOURCE_FOLD_PRIVATE_H__

#include "gtksourcefold.h"

G_BEGIN_DECLS

#define INVISIBLE_LINE "GtkSourceBuffer:InvisibleLine"

struct _GtkSourceFold
{
	/* Markers for the start & end of the fold. */
	GtkTextMark	*start_line;
	GtkTextMark	*end_line;
	
	/* Add reference to parent fold; needed for reparenting. */
	GtkSourceFold	*parent;
	
	/* List of child folds; sorted by appearance. */
	GList		*children;
	
	/* Style of the expander arrow; if animated is set, this will gradually
	 * increase to show the fold is collapsing/expanding. */
	GtkExpanderStyle expander_style;

	/* TRUE if the fold has collapsed. */
	gint		 folded : 1;
	
	/* TRUE if the user moves the mouse over the expander arrow; draw the
	 * expander filled to indicate the mouse over. */
	gint		 prelighted : 1;
	
	/* TRUE if the user expanded/collapsed a fold using the GUI; animate
	 * the collapse/expansion of the fold. */
	gint		 animated : 1;
};

G_END_DECLS

#endif  /* __GTK_SOURCE_FOLD_PRIVATE_H__ */

