/* gtkcellareacontext.h
 *
 * Copyright (C) 2010 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_CELL_AREA_CONTEXT_H__
#define __GTK_CELL_AREA_CONTEXT_H__

#include <gtk/gtkcellarea.h>

G_BEGIN_DECLS

#define GTK_TYPE_CELL_AREA_CONTEXT    	      (gtk_cell_area_context_get_type ())
#define GTK_CELL_AREA_CONTEXT(obj)	      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_AREA_CONTEXT, GtkCellAreaContext))
#define GTK_CELL_AREA_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_AREA_CONTEXT, GtkCellAreaContextClass))
#define GTK_IS_CELL_AREA_CONTEXT(obj)	      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_AREA_CONTEXT))
#define GTK_IS_CELL_AREA_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_AREA_CONTEXT))
#define GTK_CELL_AREA_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CELL_AREA_CONTEXT, GtkCellAreaContextClass))

typedef struct _GtkCellAreaContextPrivate       GtkCellAreaContextPrivate;
typedef struct _GtkCellAreaContextClass         GtkCellAreaContextClass;

struct _GtkCellAreaContext
{
  GObject parent_instance;

  GtkCellAreaContextPrivate *priv;
};

struct _GtkCellAreaContextClass
{
  GObjectClass parent_class;

  void    (* allocate)               (GtkCellAreaContext *context,
				      gint                width,
				      gint                height);
  void    (* reset)                  (GtkCellAreaContext *context);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType        gtk_cell_area_context_get_type              (void) G_GNUC_CONST;

/* Main apis */
GtkCellArea *gtk_cell_area_context_get_area              (GtkCellAreaContext *context);
void         gtk_cell_area_context_allocate              (GtkCellAreaContext *context,
							  gint                width,
							  gint                height);
void         gtk_cell_area_context_reset                 (GtkCellAreaContext *context);

/* Apis for GtkCellArea clients to consult cached values for a series of GtkTreeModel rows */
void         gtk_cell_area_context_get_preferred_width   (GtkCellAreaContext *context,
							  gint               *minimum_width,
							  gint               *natural_width);
void         gtk_cell_area_context_get_preferred_height  (GtkCellAreaContext *context,
							  gint               *minimum_height,
							  gint               *natural_height);
void         gtk_cell_area_context_get_allocation        (GtkCellAreaContext *context,
							  gint               *width,
							  gint               *height);

/* Apis for GtkCellArea implementations to update cached values for multiple GtkTreeModel rows */
void         gtk_cell_area_context_push_preferred_width  (GtkCellAreaContext *context,
							  gint                minimum_width,
							  gint                natural_width);
void         gtk_cell_area_context_push_preferred_height (GtkCellAreaContext *context,
							  gint                minimum_height,
							  gint                natural_height);

G_END_DECLS

#endif /* __GTK_CELL_AREA_CONTEXT_H__ */