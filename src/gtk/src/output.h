/* -*- c-file-style: "java" -*-
 * $Id: output.h,v 1.3 2006-02-08 05:56:04 zeeb90au Exp $
 * This file is part of SmallBASIC
 *
 * Copyright(C) 2001-2006 Chris Warren-Smith. Gawler, South Australia
 * cwarrens@twpo.com.au
 *
 * This program is distributed under the terms of the GPL v2.0 or later
 * Download the GNU Public License (GPL) from www.gnu.org
 */ 

#ifndef DEV_GTK_OUTPUT_H
#define DEV_GTK_OUTPUT_H

static gboolean drawing_area_init(GtkWidget *window);
void invalidate_rect(int x, int y, int w, int h);

#endif

/* End of "$Id: output.h,v 1.3 2006-02-08 05:56:04 zeeb90au Exp $". */
