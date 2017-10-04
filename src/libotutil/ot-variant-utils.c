/*
 * Copyright (C) 2011 Colin Walters <walters@verbum.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Colin Walters <walters@verbum.org>
 */

#include "config.h"

#include <gio/gio.h>
#include <gio/gfiledescriptorbased.h>

#include <string.h>

#include "otutil.h"

GVariant *
ot_gvariant_new_empty_string_dict (void)
{
  g_auto(GVariantBuilder) builder = OT_VARIANT_BUILDER_INITIALIZER;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  return g_variant_builder_end (&builder);
}

GVariant *
ot_gvariant_new_bytearray (const guchar   *data,
                           gsize           len)
{
  gpointer data_copy;
  GVariant *ret;

  data_copy = g_memdup (data, len);
  ret = g_variant_new_from_data (G_VARIANT_TYPE ("ay"), data_copy,
                                 len, FALSE, g_free, data_copy);
  return ret;
}

GVariant *
ot_gvariant_new_ay_bytes (GBytes *bytes)
{
  gsize size;
  gconstpointer data;
  data = g_bytes_get_data (bytes, &size);
  g_bytes_ref (bytes);
  return g_variant_new_from_data (G_VARIANT_TYPE ("ay"), data, size,
                                  TRUE, (GDestroyNotify)g_bytes_unref, bytes);
}

GHashTable *
ot_util_variant_asv_to_hash_table (GVariant *variant)
{
  GHashTable *ret;
  GVariantIter *viter;
  char *key;
  GVariant *value;
  
  ret = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_variant_unref);
  viter = g_variant_iter_new (variant);
  while (g_variant_iter_next (viter, "{s@v}", &key, &value))
    g_hash_table_replace (ret, key, g_variant_ref_sink (value));
  
  g_variant_iter_free (viter);
  
  return ret;
}

GVariant *
ot_util_variant_take_ref (GVariant *variant)
{
  return g_variant_take_ref (variant);
}

/* Create a GVariant in @out_variant that is backed by
 * the data from @fd, starting at @start.  If the data is
 * large enough, mmap() may be used.  @trusted is used
 * by the GVariant core; see g_variant_new_from_data().
 */
gboolean
ot_variant_read_fd (int                    fd,
                    goffset                start,
                    const GVariantType    *type,
                    gboolean               trusted,
                    GVariant             **out_variant,
                    GError               **error)
{
  g_autoptr(GBytes) bytes = ot_fd_readall_or_mmap (fd, start, error);
  if (!bytes)
    return FALSE;

  *out_variant = g_variant_ref_sink (g_variant_new_from_bytes (type, bytes, trusted));
  return TRUE;
}

GInputStream *
ot_variant_read (GVariant             *variant)
{
  GMemoryInputStream *ret = NULL;

  ret = (GMemoryInputStream*)g_memory_input_stream_new_from_data (g_variant_get_data (variant),
                                                                  g_variant_get_size (variant),
                                                                  NULL);
  g_object_set_data_full ((GObject*)ret, "ot-variant-data",
                          g_variant_ref (variant), (GDestroyNotify) g_variant_unref);
  return (GInputStream*)ret;
}

GVariantBuilder *
ot_util_variant_builder_from_variant (GVariant            *variant,
                                      const GVariantType  *type)
{
  GVariantBuilder *builder = NULL;
  
  builder = g_variant_builder_new (type);
  
  if (variant != NULL)
    {
      gint i, n;

      n = g_variant_n_children (variant);
      for (i = 0; i < n; i++)
        {
          GVariant *child = g_variant_get_child_value (variant, i);
          g_variant_builder_add_value (builder, child);
          g_variant_unref (child);
        }
    }
    
  return builder;
}

/**
 * ot_variant_bsearch_str:
 * @array: A GVariant array whose first element must be a string
 * @str: Search for this string
 * @out_pos: Output position
 *
 *
 * Binary search in a GVariant array, which must be of the form 'a(s...)',
 * where '...' may be anything.  The array elements must be sorted.
 *
 * Returns: %TRUE if found, %FALSE otherwise
 */
gboolean
ot_variant_bsearch_str (GVariant   *array,
                        const char *str,
                        int        *out_pos)
{
  gsize imax, imin;
  gsize imid = -1;
  gsize n;

  n = g_variant_n_children (array);
  if (n == 0)
    return FALSE;

  imax = n - 1;
  imin = 0;
  while (imax >= imin)
    {
      g_autoptr(GVariant) child = NULL;
      const char *cur;
      int cmp;

      imid = (imin + imax) / 2;

      child = g_variant_get_child_value (array, imid);
      g_variant_get_child (child, 0, "&s", &cur, NULL);

      cmp = strcmp (cur, str);
      if (cmp < 0)
        imin = imid + 1;
      else if (cmp > 0)
        {
          if (imid == 0)
            break;
          imax = imid - 1;
        }
      else
        {
          *out_pos = imid;
          return TRUE;
        }
    }

  *out_pos = imid;
  return FALSE;
}
