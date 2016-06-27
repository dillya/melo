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

static void volume_added(GVolumeMonitor *monitor, GVolume *vol,
                         gpointer user_data);
static void volume_removed(GVolumeMonitor *monitor, GVolume *vol,
                           gpointer user_data);
static void mount_added(GVolumeMonitor *monitor, GMount *mnt,
                        gpointer user_data);
static void mount_removed(GVolumeMonitor *monitor, GMount *mnt,
                          gpointer user_data);
static const MeloBrowserInfo *melo_browser_file_get_info (MeloBrowser *browser);
static GList *melo_browser_file_get_list (MeloBrowser *browser,
                                          const gchar *path);

struct _MeloBrowserFilePrivate {
  GVolumeMonitor *monitor;
  GMutex mutex;
  GList *volumes;
  GList *mounts;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloBrowserFile, melo_browser_file, MELO_TYPE_BROWSER)

static void
melo_browser_file_finalize (GObject *gobject)
{
  MeloBrowserFile *browser_file = MELO_BROWSER_FILE (gobject);
  MeloBrowserFilePrivate *priv =
                          melo_browser_file_get_instance_private (browser_file);

  /* Release volume monitor */
  g_object_unref (priv->monitor);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Free volume and mount list */
  g_list_free_full (priv->mounts, g_object_unref);
  g_list_free_full (priv->volumes, g_object_unref);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_browser_file_parent_class)->finalize (gobject);
}

static void
melo_browser_file_class_init (MeloBrowserFileClass *klass)
{
  MeloBrowserClass *bclass = MELO_BROWSER_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  bclass->get_info = melo_browser_file_get_info;
  bclass->get_list = melo_browser_file_get_list;

  /* Add custom finalize() function */
  oclass->finalize = melo_browser_file_finalize;
}

static void
melo_browser_file_init (MeloBrowserFile *self)
{
  MeloBrowserFilePrivate *priv = melo_browser_file_get_instance_private (self);

  self->priv = priv;

  /* Get volume monitor */
  priv->monitor = g_volume_monitor_get ();

  /* Init mutex */
  g_mutex_init (&priv->mutex);

  /* Get list of volumes and mounts */
  priv->volumes = g_volume_monitor_get_volumes (priv->monitor);
  priv->mounts = g_volume_monitor_get_mounts (priv->monitor);

  /* Subscribe to volume and mount events of volume monitor */
  g_signal_connect (priv->monitor, "volume-added",
                    (GCallback) volume_added, priv);
  g_signal_connect (priv->monitor, "volume-removed",
                    (GCallback) volume_removed, priv);
  g_signal_connect (priv->monitor, "mount_added",
                    (GCallback) mount_added, priv);
  g_signal_connect (priv->monitor, "mount_removed",
                    (GCallback) mount_removed, priv);
}

static const MeloBrowserInfo *
melo_browser_file_get_info (MeloBrowser *browser)
{
  return &melo_browser_file_info;
}

static void
volume_added(GVolumeMonitor *monitor, GVolume *vol, gpointer user_data)
{
  MeloBrowserFilePrivate *priv = (MeloBrowserFilePrivate *) user_data;

  /* Lock volume/mount list */
  g_mutex_lock (&priv->mutex);

  priv->volumes = g_list_append (priv->volumes, vol);

  /* Unlock volume/mount list */
  g_mutex_unlock (&priv->mutex);
}

static void
volume_removed(GVolumeMonitor *monitor, GVolume *vol, gpointer user_data)
{
  MeloBrowserFilePrivate *priv = (MeloBrowserFilePrivate *) user_data;

  /* Lock volume/mount list */
  g_mutex_lock (&priv->mutex);

  priv->volumes = g_list_remove (priv->volumes, vol);

  /* Unlock volume/mount list */
  g_mutex_unlock (&priv->mutex);
}

static void
mount_added(GVolumeMonitor *monitor, GMount *mnt, gpointer user_data)
{
  MeloBrowserFilePrivate *priv = (MeloBrowserFilePrivate *) user_data;

  /* Lock volume/mount list */
  g_mutex_lock (&priv->mutex);

  priv->mounts = g_list_append (priv->mounts, mnt);

  /* Unlock volume/mount list */
  g_mutex_unlock (&priv->mutex);
}

static void
mount_removed(GVolumeMonitor *monitor, GMount *mnt, gpointer user_data)
{
  MeloBrowserFilePrivate *priv = (MeloBrowserFilePrivate *) user_data;

  /* Lock volume/mount list */
  g_mutex_lock (&priv->mutex);

  priv->mounts = g_list_remove (priv->mounts, mnt);

  /* Unlock volume/mount list */
  g_mutex_unlock (&priv->mutex);
}

static const gchar *
melo_brower_file_fix_path (const gchar *path)
{
  while (*path != '\0' && *path == '/')
    path++;
  return path;
}

static GList *
melo_browser_file_list (GFile *dir)
{
  GFileEnumerator *dir_enum;
  GFileInfo *info;
  GList *list = NULL;

  /* Get details */
  if (g_file_query_file_type (dir, 0, NULL) != G_FILE_TYPE_DIRECTORY)
    return NULL;

  /* Get list of directory */
  dir_enum = g_file_enumerate_children (dir,
                                    G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                    G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                                    G_FILE_ATTRIBUTE_STANDARD_NAME,
                                    0, NULL, NULL);
  if (!dir_enum)
    return NULL;

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
    item->full_name = g_strdup (g_file_info_get_display_name (info));
    list = g_list_insert_sorted (list, item,
                                 (GCompareFunc) melo_browser_item_cmp);
    g_object_unref (info);
  }
  g_object_unref (dir_enum);

  return list;
}

static GList *
melo_browser_file_get_local_list (const gchar *uri)
{
  GFile *dir;
  GList *list;

  /* Open directory */
  dir = g_file_new_for_uri (uri);
  if (!dir)
    return NULL;

  /* Get list from GFile */
  list = melo_browser_file_list (dir);
  g_object_unref (dir);

  return list;
}

static GList *
melo_browser_file_get_volume_list (MeloBrowserFile *bfile, const gchar *path)
{
  MeloBrowserFilePrivate *priv = bfile->priv;
  GMount *mount = NULL;
  GFile *root, *dir;
  GList *list;
  gchar *name;

  /* Extract volume name from path */
  name = g_strstr_len (path, -1, "/");
  if (name) {
    gsize len = name - path;
    name = g_strndup (path, len);
    path += len + 1;
  } else {
    name = g_strdup (path);
    path = "";
  }

  /* Get mount from volume monitor, through its UUID */
  if (!mount) {
    mount = g_volume_monitor_get_mount_for_uuid (priv->monitor, name);
  }

  /* Get mount from its volume, through its UUID */
  if (!mount) {
    GVolume *vol = g_volume_monitor_get_volume_for_uuid (priv->monitor, name);
    if (vol) {
      g_volume_mount (vol, 0, NULL, NULL, NULL, NULL);
      mount = g_volume_get_mount (vol);
      g_object_unref (vol);
    }
  }

  /* Get mount from mount list */
  if (!mount) {
    GList *l = NULL;

    /* Lock volume/mount list */
    g_mutex_lock (&priv->mutex);

    /* Find mount in list */
    for (l = priv->mounts; l != NULL; l = l->next) {
      GMount *mnt = (GMount *) l->data;
      gchar *mnt_name = g_mount_get_name (mnt);
      if (!g_strcmp0 (mnt_name, name)) {
        mount = g_object_ref (mnt);
        g_free (mnt_name);
        break;
      }
      g_free (mnt_name);
    }

    /* Unlock volume/mount list */
    g_mutex_unlock (&priv->mutex);
  }

  /* Get mount from volume list */
  if (!mount) {
    GVolume *vol = NULL;
    GList *l = NULL;
    gchar *vol_name;

    /* Lock volume/mount list */
    g_mutex_lock (&priv->mutex);

    /* Find in volume list */
    for (l = priv->volumes; l != NULL; l = l->next) {
      vol = (GVolume *) l->data;
      vol_name = g_volume_get_name (vol);
      if (!g_strcmp0 (vol_name, name)) {
          g_object_ref (vol);
          g_free (vol_name);
          break;
      }
      g_free (vol_name);
    }

    /* Unlock volume/mount list */
    g_mutex_unlock (&priv->mutex);

    /* volume found */
    if (l && vol) {
      g_volume_mount (vol, 0, NULL, NULL, NULL, NULL);
      mount = g_volume_get_mount (vol);
      g_object_unref (vol);
    }
  }

  /* Volume doesn't exist */
  g_free (name);
  if (!mount)
    return NULL;

  /* Get root */
  root = g_mount_get_root (mount);
  if (!root) {
    g_object_unref (mount);
    return NULL;
  }
  g_object_unref (mount);

  /* Get final directory from our path */
  dir = g_file_resolve_relative_path (root, path);
  if (!dir) {
    g_object_unref (root);
    return NULL;
  }
  g_object_unref (root);

  /* List files from our GFile  */
  list = melo_browser_file_list (dir);
  g_object_unref (dir);

  return list;
}

static GList *
melo_browser_file_list_volumes (MeloBrowserFile *bfile, GList *list)
{
  MeloBrowserFilePrivate *priv = bfile->priv;
  MeloBrowserItem *item;
  GVolume *vol;
  GMount *mnt;
  GList *l;
  gchar *full_name, *name;
  gchar *uuid;

  /* Lock volume/mount list */
  g_mutex_lock (&priv->mutex);

  /* Get volume list */
  for (l = priv->volumes; l != NULL; l = l->next) {
    /* Get volume */
    vol = (GVolume *) l->data;

    /* Get name and UUID */
    full_name = g_volume_get_name (vol);
    uuid = g_volume_get_uuid (vol);
    if (uuid) {
      name = g_strdup_printf ("_%s", uuid);
      g_free (uuid);
    } else
      name = g_strdup_printf ("_%s", full_name);

    /* Create item */
    item = melo_browser_item_new (NULL, "category");
    item->name = name;
    item->full_name = full_name;
    list = g_list_append(list, item);
  }

  /* Complete with mount list */
  for (l = priv->mounts; l != NULL; l = l->next) {
    /* Get mount */
    mnt = (GMount *) l->data;

    /* Skip if mount has a volume */
    vol = g_mount_get_volume (mnt);
    if (vol) {
      g_object_unref (vol);
      continue;
    }

    /* Get name */
    full_name = g_mount_get_name (mnt);
    uuid = g_mount_get_uuid (mnt);
    if (uuid) {
      name = g_strdup_printf ("_%s", uuid);
      g_free (uuid);
    } else
      name = g_strdup_printf ("_%s", full_name);

    /* Create item */
    item = melo_browser_item_new (NULL, "category");
    item->name = name;
    item->full_name = full_name;
    list = g_list_append(list, item);
  }

  /* Unlock volume/mount list */
  g_mutex_unlock (&priv->mutex);

  return list;
}

static GList *
melo_browser_file_get_list (MeloBrowser *browser, const gchar *path)
{
  MeloBrowserFile *bfile = MELO_BROWSER_FILE (browser);
  GList *list = NULL;
  gchar *uri;

  /* Check path */
  if (!path || *path != '/')
    return NULL;
  path++;

  /* Parse path and select list action */
  if (*path == '\0') {
    /* Root path: "/" */
    MeloBrowserItem *item;

    /* Add Local entry for local file system */
    item = melo_browser_item_new ("local", "category");
    item->full_name = g_strdup ("Local");
    list = g_list_append(list, item);

    /* Add local volumes to list */
    list = melo_browser_file_list_volumes (bfile, list);
  } else if (*path == '_') {
    /* Volume path: "/_VOLUME_NAME/" */
    list = melo_browser_file_get_volume_list (bfile, path + 1);
  } else if (g_str_has_prefix (path, "local")) {
    /* Local path: "/local/" */
    path = melo_brower_file_fix_path (path + 5);
    uri = g_strdup_printf ("file:/%s", path);
    list = melo_browser_file_get_local_list (uri);
    g_free (uri);
  }
  else
    return NULL;

  return list;
}
