/* gtkcellareaboxcontext.c
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

#include "config.h"
#include "gtkintl.h"
#include "gtkcellareabox.h"
#include "gtkcellareaboxcontext.h"
#include "gtkorientable.h"

/* GObjectClass */
static void      gtk_cell_area_box_context_finalize              (GObject               *object);

/* GtkCellAreaContextClass */
static void      gtk_cell_area_box_context_reset                 (GtkCellAreaContext    *context);
static void      gtk_cell_area_box_context_allocate              (GtkCellAreaContext    *context,
								  gint                   width,
								  gint                   height);

/* Internal functions */
static void      gtk_cell_area_box_context_sum                   (GtkCellAreaBoxContext *context,
								  GtkOrientation         orientation);
static void      free_cache_array                                (GArray                *array);
static GArray   *group_array_new                                 (GtkCellAreaBoxContext *context);
static GArray   *get_array                                       (GtkCellAreaBoxContext *context,
								  GtkOrientation         orientation,
								  gint                   for_size);
static gboolean  group_expands                                   (GtkCellAreaBoxContext *context,
								  gint                   group_idx);
static gint      count_expand_groups                             (GtkCellAreaBoxContext *context);


/* CachedSize management */
typedef struct {
  gint     min_size;
  gint     nat_size;
} CachedSize;

struct _GtkCellAreaBoxContextPrivate
{
  /* Table of per renderer CachedSizes */
  GArray *base_widths;
  GArray *base_heights;

  /* Table of per height/width hash tables of per renderer CachedSizes */
  GHashTable *widths;
  GHashTable *heights;

  /* Whether each group expands */
  gboolean  *expand;

  /* Allocation info for this context if any */
  gint                      alloc_width;
  gint                      alloc_height;
  gint                      n_orientation_allocs;
  GtkCellAreaBoxAllocation *orientation_allocs;
};

G_DEFINE_TYPE (GtkCellAreaBoxContext, gtk_cell_area_box_context, GTK_TYPE_CELL_AREA_CONTEXT);

static void
free_cache_array (GArray *array)
{
  g_array_free (array, TRUE);
}

static GArray *
group_array_new (GtkCellAreaBoxContext *context)
{
  GtkCellAreaBoxContextPrivate *priv = context->priv;
  GArray *group_array;

  group_array = g_array_new (FALSE, TRUE, sizeof (CachedSize));
  g_array_set_size (group_array, priv->base_widths->len);

  return group_array;
}

static GArray *
get_array (GtkCellAreaBoxContext *context,
	   GtkOrientation         orientation,
	   gint                   for_size)
{
  GtkCellAreaBoxContextPrivate *priv = context->priv;
  GArray                       *array;

  if (for_size < 0)
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	array = priv->base_widths;
      else
	array = priv->base_heights;
    }
  else
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  array = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (for_size));

	  if (!array)
	    array = priv->base_widths;
	}
      else
	{
	  array = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (for_size));

	  if (!array)
	    array = priv->base_heights;
	}
    }

  return array;
}

static gboolean 
group_expands (GtkCellAreaBoxContext *context,
	       gint                   group_idx)
{
  GtkCellAreaBoxContextPrivate *priv = context->priv;

  g_assert (group_idx >= 0 && group_idx < priv->base_widths->len);

  return priv->expand[group_idx];
}

static gint
count_expand_groups (GtkCellAreaBoxContext *context)
{
  GtkCellAreaBoxContextPrivate *priv = context->priv;
  gint i, expand = 0;

  for (i = 0; i < priv->base_widths->len; i++)
    {
      if (priv->expand[i])
	expand++;
    }
  
  return expand;
}

static void
gtk_cell_area_box_context_init (GtkCellAreaBoxContext *box_context)
{
  GtkCellAreaBoxContextPrivate *priv;

  box_context->priv = G_TYPE_INSTANCE_GET_PRIVATE (box_context,
						   GTK_TYPE_CELL_AREA_BOX_CONTEXT,
						   GtkCellAreaBoxContextPrivate);
  priv = box_context->priv;

  priv->base_widths  = g_array_new (FALSE, TRUE, sizeof (CachedSize));
  priv->base_heights = g_array_new (FALSE, TRUE, sizeof (CachedSize));

  priv->widths       = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					      NULL, (GDestroyNotify)free_cache_array);
  priv->heights      = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					      NULL, (GDestroyNotify)free_cache_array);

  priv->alloc_width  = 0;
  priv->alloc_height = 0;
  priv->orientation_allocs   = NULL;
  priv->n_orientation_allocs = 0;
}

static void 
gtk_cell_area_box_context_class_init (GtkCellAreaBoxContextClass *class)
{
  GObjectClass            *object_class = G_OBJECT_CLASS (class);
  GtkCellAreaContextClass *context_class   = GTK_CELL_AREA_CONTEXT_CLASS (class);

  /* GObjectClass */
  object_class->finalize = gtk_cell_area_box_context_finalize;

  context_class->reset     = gtk_cell_area_box_context_reset;
  context_class->allocate  = gtk_cell_area_box_context_allocate;

  g_type_class_add_private (object_class, sizeof (GtkCellAreaBoxContextPrivate));
}

/*************************************************************
 *                      GObjectClass                         *
 *************************************************************/
static void
gtk_cell_area_box_context_finalize (GObject *object)
{
  GtkCellAreaBoxContext        *box_context = GTK_CELL_AREA_BOX_CONTEXT (object);
  GtkCellAreaBoxContextPrivate *priv        = box_context->priv;

  g_array_free (priv->base_widths, TRUE);
  g_array_free (priv->base_heights, TRUE);
  g_hash_table_destroy (priv->widths);
  g_hash_table_destroy (priv->heights);

  g_free (priv->orientation_allocs);
  g_free (priv->expand);

  G_OBJECT_CLASS (gtk_cell_area_box_context_parent_class)->finalize (object);
}

/*************************************************************
 *                    GtkCellAreaContextClass                   *
 *************************************************************/
static void
gtk_cell_area_box_context_reset (GtkCellAreaContext *context)
{
  GtkCellAreaBoxContext        *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  GtkCellAreaBoxContextPrivate *priv        = box_context->priv;
  CachedSize                   *size;
  gint                          i;

  for (i = 0; i < priv->base_widths->len; i++)
    {
      size = &g_array_index (priv->base_widths, CachedSize, i);

      size->min_size = 0;
      size->nat_size = 0;

      size = &g_array_index (priv->base_heights, CachedSize, i);

      size->min_size = 0;
      size->nat_size = 0;
    }

  /* Reset context sizes as well */
  g_hash_table_remove_all (priv->widths);
  g_hash_table_remove_all (priv->heights);

  /* Clear the allocation */
  g_free (priv->orientation_allocs);
  priv->orientation_allocs   = NULL;
  priv->n_orientation_allocs = 0;

  GTK_CELL_AREA_CONTEXT_CLASS
    (gtk_cell_area_box_context_parent_class)->reset (context);
}

static GtkRequestedSize *
gtk_cell_area_box_context_get_requests (GtkCellAreaBoxContext *box_context,
					GtkOrientation         orientation,
					gint                   for_size,
					gint                  *n_requests)
{
  GtkCellAreaBoxContextPrivate *priv;
  GtkRequestedSize             *requests;
  GArray                       *array;
  CachedSize                   *size;
  gint                          visible_groups = 0;
  gint                          i, j;

  g_return_val_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context), NULL);

  priv  = box_context->priv;
  array = get_array (box_context, orientation, for_size);

  for (i = 0; i < array->len; i++)
    {
      size = &g_array_index (array, CachedSize, i);

      if (size->nat_size > 0)
	visible_groups++;
    }

  requests = g_new (GtkRequestedSize, visible_groups);

  for (j = 0, i = 0; i < array->len; i++)
    {
      size = &g_array_index (array, CachedSize, i);

      if (size->nat_size > 0)
	{
	  requests[j].data         = GINT_TO_POINTER (i);
	  requests[j].minimum_size = size->min_size;
	  requests[j].natural_size = size->nat_size;
	  j++;
	}
    }

  if (n_requests)
    *n_requests = visible_groups;

  return requests;
}

static GtkCellAreaBoxAllocation *
allocate_for_orientation (GtkCellAreaBoxContext *context,
			  GtkOrientation         orientation,
			  gint                   spacing,
			  gint                   size,
			  gint                   for_size,
			  gint                  *n_allocs)
{
  GtkCellAreaBoxAllocation     *allocs;
  GtkRequestedSize             *sizes;
  GArray                       *array;
  gint                          n_expand_groups = 0;
  gint                          i, n_groups, position;
  gint                          extra_size, extra_extra;
  gint                          avail_size = size;

  sizes           = gtk_cell_area_box_context_get_requests (context, orientation, for_size, &n_groups);
  array           = get_array (context, orientation, for_size);
  n_expand_groups = count_expand_groups (context);

  /* First start by naturally allocating space among groups */
  avail_size -= (n_groups - 1) * spacing;
  for (i = 0; i < n_groups; i++)
    avail_size -= sizes[i].minimum_size;

  if (avail_size > 0)
    avail_size = gtk_distribute_natural_allocation (avail_size, n_groups, sizes);
  else
    avail_size = 0;

  /* Calculate/distribute expand for groups */
  if (n_expand_groups > 0)
    {
      extra_size  = avail_size / n_expand_groups;
      extra_extra = avail_size % n_expand_groups;
    }
  else
    extra_size = extra_extra = 0;

  allocs = g_new (GtkCellAreaBoxAllocation, n_groups);

  for (position = 0, i = 0; i < n_groups; i++)
    {
      allocs[i].group_idx = GPOINTER_TO_INT (sizes[i].data);
      allocs[i].position  = position;
      allocs[i].size      = sizes[i].minimum_size;

      if (group_expands (context, allocs[i].group_idx))
	{
	  allocs[i].size += extra_size;
	  if (extra_extra)
	    {
	      allocs[i].size++;
	      extra_extra--;
	    }
	}

      position += allocs[i].size;
      position += spacing;
    }

  if (n_allocs)
    *n_allocs = n_groups;

  g_free (sizes);

  return allocs;
}

static void
gtk_cell_area_box_context_allocate (GtkCellAreaContext *context,
				    gint                width,
				    gint                height)
{
  GtkCellAreaBoxContext        *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  GtkCellAreaBoxContextPrivate *priv        = box_context->priv;
  GtkCellArea                  *area;
  GtkOrientation                orientation;
  gint                          spacing;

  area        = gtk_cell_area_context_get_area (context);
  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (area));
  spacing     = gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (area));

  g_free (priv->orientation_allocs);
  priv->orientation_allocs   = NULL;
  priv->n_orientation_allocs = 0;

  if (orientation == GTK_ORIENTATION_HORIZONTAL && width > 0)
    priv->orientation_allocs = allocate_for_orientation (box_context, orientation, 
							 spacing, width, height,
							 &priv->n_orientation_allocs);
  else if (orientation == GTK_ORIENTATION_VERTICAL && height > 0)
    priv->orientation_allocs = allocate_for_orientation (box_context, orientation, 
							 spacing, height, width,
							 &priv->n_orientation_allocs);
  
  GTK_CELL_AREA_CONTEXT_CLASS (gtk_cell_area_box_context_parent_class)->allocate (context, width, height);
}

static void
gtk_cell_area_box_context_sum (GtkCellAreaBoxContext *context,
			       GtkOrientation         orientation)
{
  GtkCellAreaBoxContextPrivate *priv = context->priv;
  GtkCellArea                  *area;
  GtkOrientation                box_orientation;
  GArray                       *array;
  gint                          spacing, i;
  gint                          min_size = 0, nat_size = 0;

  area            = gtk_cell_area_context_get_area (GTK_CELL_AREA_CONTEXT (context));
  spacing         = gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (area));
  box_orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (area));
  array           = get_array (context, orientation, -1);

  for (i = 0; i < array->len; i++)
    {
      CachedSize *size = &g_array_index (priv->base_widths, CachedSize, i);

      if (box_orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  /* Dont add spacing for 0 size groups, they can be 0 size because
	   * they contain only invisible cells for this round of requests 
	   */
	  if (min_size > 0 && size->nat_size > 0)
	    {
	      min_size += spacing;
	      nat_size += spacing;
	    }
	  
	  min_size += size->min_size;
	  nat_size += size->nat_size;
	}
      else
	{
	  min_size = MAX (min_size, size->min_size);
	  nat_size = MAX (nat_size, size->nat_size);
	}
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_cell_area_context_push_preferred_width (GTK_CELL_AREA_CONTEXT (context), min_size, nat_size);
  else
    gtk_cell_area_context_push_preferred_height (GTK_CELL_AREA_CONTEXT (context), min_size, nat_size);
}

/*************************************************************
 *                            API                            *
 *************************************************************/
void
gtk_cell_area_box_init_groups (GtkCellAreaBoxContext *box_context,
			       guint                  n_groups,
			       gboolean              *expand_groups)
{
  GtkCellAreaBoxContextPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));
  g_return_if_fail (n_groups == 0 || expand_groups != NULL);

  /* When the group dimensions change, all info must be reset 
   * Note this already clears the min/nat values on the CachedSizes
   */
  gtk_cell_area_context_reset (GTK_CELL_AREA_CONTEXT (box_context));

  priv = box_context->priv;
  g_array_set_size (priv->base_widths,  n_groups);
  g_array_set_size (priv->base_heights, n_groups);

  g_free (priv->expand);
  priv->expand = g_memdup (expand_groups, n_groups * sizeof (gboolean));
}

void
gtk_cell_area_box_context_push_group_width (GtkCellAreaBoxContext *box_context,
					    gint                   group_idx,
					    gint                   minimum_width,
					    gint                   natural_width)
{
  GtkCellAreaBoxContextPrivate *priv;
  CachedSize                   *size;
  gboolean                      grew = FALSE;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  size = &g_array_index (priv->base_widths, CachedSize, group_idx);
  if (minimum_width > size->min_size)
    {
      size->min_size = minimum_width;
      grew = TRUE;
    }
  if (natural_width > size->nat_size)
    {
      size->nat_size = natural_width;
      grew = TRUE;
    }

  if (grew)
    gtk_cell_area_box_context_sum (box_context, GTK_ORIENTATION_HORIZONTAL);
}

void
gtk_cell_area_box_context_push_group_height_for_width  (GtkCellAreaBoxContext *box_context,
							gint                   group_idx,
							gint                   for_width,
							gint                   minimum_height,
							gint                   natural_height)
{
  GtkCellAreaBoxContextPrivate *priv;
  GArray                       *group_array;
  CachedSize                   *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  group_array = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (for_width));
  if (!group_array)
    {
      group_array = group_array_new (box_context);
      g_hash_table_insert (priv->heights, GINT_TO_POINTER (for_width), group_array);
    }

  size = &g_array_index (group_array, CachedSize, group_idx);
  size->min_size = MAX (size->min_size, minimum_height);
  size->nat_size = MAX (size->nat_size, natural_height);
}

void
gtk_cell_area_box_context_push_group_height (GtkCellAreaBoxContext *box_context,
					     gint                   group_idx,
					     gint                   minimum_height,
					     gint                   natural_height)
{
  GtkCellAreaBoxContextPrivate *priv;
  CachedSize                   *size;
  gboolean                      grew = FALSE;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_heights->len);

  size = &g_array_index (priv->base_heights, CachedSize, group_idx);
  if (minimum_height > size->min_size)
    {
      size->min_size = minimum_height;
      grew = TRUE;
    }
  if (natural_height > size->nat_size)
    {
      size->nat_size = natural_height;
      grew = TRUE;
    }

  if (grew)
    gtk_cell_area_box_context_sum (box_context, GTK_ORIENTATION_VERTICAL);
}

void
gtk_cell_area_box_context_push_group_width_for_height (GtkCellAreaBoxContext *box_context,
						       gint                   group_idx,
						       gint                   for_height,
						       gint                   minimum_width,
						       gint                   natural_width)
{
  GtkCellAreaBoxContextPrivate *priv;
  GArray                       *group_array;
  CachedSize                   *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv        = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  group_array = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (for_height));
  if (!group_array)
    {
      group_array = group_array_new (box_context);
      g_hash_table_insert (priv->widths, GINT_TO_POINTER (for_height), group_array);
    }

  size = &g_array_index (group_array, CachedSize, group_idx);
  size->min_size = MAX (size->min_size, minimum_width);
  size->nat_size = MAX (size->nat_size, natural_width);
}

void
gtk_cell_area_box_context_get_group_width (GtkCellAreaBoxContext *box_context,
					   gint                   group_idx,
					   gint                  *minimum_width,
					   gint                  *natural_width)
{
  GtkCellAreaBoxContextPrivate *priv;
  CachedSize                   *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  size = &g_array_index (priv->base_widths, CachedSize, group_idx);

  if (minimum_width)
    *minimum_width = size->min_size;
  
  if (natural_width)
    *natural_width = size->nat_size;
}

void
gtk_cell_area_box_context_get_group_height_for_width (GtkCellAreaBoxContext *box_context,
						      gint                   group_idx,
						      gint                   for_width,
						      gint                  *minimum_height,
						      gint                  *natural_height)
{
  GtkCellAreaBoxContextPrivate *priv;
  GArray                    *group_array;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv        = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  group_array = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (for_width));

  if (group_array)
    {
      CachedSize *size = &g_array_index (group_array, CachedSize, group_idx);

      if (minimum_height)
	*minimum_height = size->min_size;

      if (natural_height)
	*natural_height = size->nat_size;
    }
  else
    {
      if (minimum_height)
	*minimum_height = -1;

      if (natural_height)
	*natural_height = -1;      
    }
}

void
gtk_cell_area_box_context_get_group_height (GtkCellAreaBoxContext *box_context,
					    gint                   group_idx,
					    gint                  *minimum_height,
					    gint                  *natural_height)
{
  GtkCellAreaBoxContextPrivate *priv;
  CachedSize                   *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_heights->len);

  size = &g_array_index (priv->base_heights, CachedSize, group_idx);

  if (minimum_height)
    *minimum_height = size->min_size;
  
  if (natural_height)
    *natural_height = size->nat_size;
}

void
gtk_cell_area_box_context_get_group_width_for_height (GtkCellAreaBoxContext *box_context,
						      gint                   group_idx,
						      gint                   for_height,
						      gint                  *minimum_width,
						      gint                  *natural_width)
{
  GtkCellAreaBoxContextPrivate *priv;
  GArray                       *group_array;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  group_array = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (for_height));

  if (group_array)
    {
      CachedSize *size = &g_array_index (group_array, CachedSize, group_idx);

      if (minimum_width)
	*minimum_width = size->min_size;

      if (natural_width)
	*natural_width = size->nat_size;
    }
  else
    {
      if (minimum_width)
	*minimum_width = -1;

      if (natural_width)
	*natural_width = -1;      
    }
}

GtkRequestedSize *
gtk_cell_area_box_context_get_widths (GtkCellAreaBoxContext *box_context,
				      gint                  *n_widths)
{
  return gtk_cell_area_box_context_get_requests (box_context, GTK_ORIENTATION_HORIZONTAL, -1, n_widths);
}

GtkRequestedSize *
gtk_cell_area_box_context_get_heights (GtkCellAreaBoxContext *box_context,
				       gint                  *n_heights)
{
  return gtk_cell_area_box_context_get_requests (box_context, GTK_ORIENTATION_VERTICAL, -1, n_heights);
}

GtkCellAreaBoxAllocation *
gtk_cell_area_box_context_get_orientation_allocs (GtkCellAreaBoxContext *context,
						  gint                  *n_allocs)
{
  GtkCellAreaBoxContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (context), NULL);
  
  priv = context->priv;

  *n_allocs = priv->n_orientation_allocs;

  return priv->orientation_allocs;
}