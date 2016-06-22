/*
 * melo_browser_file.c: File Browser using GIO
 *
 * Copyright (C) 2016 Alexandre Dilly <dillya@sparod.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <gio/gio.h>

#include "melo_browser_file.h"

/* File browser info */
static MeloBrowserInfo melo_browser_file_info = {
  .name = "Browse files",
  .description = "Navigate though local and remote filesystems",
};

static const MeloBrowserInfo *melo_browser_file_get_info (MeloBrowser *browser);
static GList *melo_browser_file_get_list (MeloBrowser *browser,
                                          const gchar *path);

G_DEFINE_TYPE (MeloBrowserFile, melo_browser_file, MELO_TYPE_BROWSER)

static void
melo_browser_file_class_init (MeloBrowserFileClass *klass)
{
  MeloBrowserClass *bclass = MELO_BROWSER_CLASS (klass);

  bclass->get_info = melo_browser_file_get_info;
  bclass->get_list = melo_browser_file_get_list;
}

static void
melo_browser_file_init (MeloBrowserFile *self)
{
}

static const MeloBrowserInfo *
melo_browser_file_get_info (MeloBrowser *browser)
{
  return &melo_browser_file_info;
}

static GList *
melo_browser_file_get_list (MeloBrowser *browser, const gchar *path)
{
  GFileEnumerator *dir_enum;
  GFileInfo *info;
  GFile *dir;
  GList *list = NULL;

  /* Open directory */
  dir = g_file_new_for_uri (path);
  if (!dir)
    return NULL;

  /* Get details */
  if (g_file_query_file_type (dir, 0, NULL) != G_FILE_TYPE_DIRECTORY) {
    g_object_unref (dir);
    return NULL;
  }

  /* Get list of directory */
  dir_enum = g_file_enumerate_children (dir, NULL, 0, NULL, NULL);
  if (!dir_enum) {
    g_object_unref (dir);
    return NULL;
  }

  /* Create list */
  while ((info = g_file_enumerator_next_file (dir_enum, NULL, NULL))) {
    MeloBrowserItem *item;
    const gchar *itype;
    GFileType type;

    /* Get item type */
    type = g_file_info_get_file_type (info);
    if (type == G_FILE_TYPE_REGULAR)
      itype = "file";
    else if (type == G_FILE_TYPE_DIRECTORY)
      itype = "directory";
    else {
      g_object_unref (info);
      continue;
    }

    /* Create a new browser item and insert in list */
    item = melo_browser_item_new (g_file_info_get_name (info), itype);
    list = g_list_insert_sorted (list, item,
                                 (GCompareFunc) melo_browser_item_cmp);
    g_object_unref (info);
  }
  g_object_unref (dir_enum);
  g_object_unref (dir);

  return list;
}
