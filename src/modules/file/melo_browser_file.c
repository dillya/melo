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

#include <string.h>
#include <gio/gio.h>
#include <gst/pbutils/pbutils.h>

#include "melo_browser_file.h"

#define MELO_BROWSER_FILE_ID "melo_browser_file_id"
#define MELO_BROWSER_FILE_ID_LENGTH 8

/* File browser info */
static MeloBrowserInfo melo_browser_file_info = {
  .name = "Browse files",
  .description = "Navigate though local and remote filesystems",
  .tags_support = TRUE,
  .tags_cache_support = FALSE,
};

static gint vms_cmp (GObject *a, GObject *b);
static void vms_added(GVolumeMonitor *monitor, GObject *obj,
                      MeloBrowserFilePrivate *priv);
static void vms_removed(GVolumeMonitor *monitor, GObject *obj,
                        MeloBrowserFilePrivate *priv);
static void on_discovered (GstDiscoverer *discoverer, GstDiscovererInfo *info,
                           GError *error, gpointer user_data);
static void melo_browser_file_set_id (GObject *obj,
                                      MeloBrowserFilePrivate *priv);
static const MeloBrowserInfo *melo_browser_file_get_info (MeloBrowser *browser);
static MeloBrowserList *melo_browser_file_get_list (MeloBrowser *browser,
                                                  const gchar *path,
                                                  gint offset, gint count,
                                                  const gchar *token,
                                                  MeloBrowserTagsMode tags_mode,
                                                  MeloTagsFields tags_fields);
static MeloTags *melo_browser_file_get_tags (MeloBrowser *browser,
                                             const gchar *path,
                                             MeloTagsFields fields);
static gboolean melo_browser_file_add (MeloBrowser *browser, const gchar *path);
static gboolean melo_browser_file_play (MeloBrowser *browser,
                                        const gchar *path);
static gboolean melo_browser_file_remove (MeloBrowser *browser,
                                          const gchar *path);

static gboolean melo_browser_file_get_cover (MeloBrowser *browser,
                                             const gchar *path, GBytes **data,
                                             gchar **type);

struct _MeloBrowserFilePrivate {
  gchar *local_path;
  GVolumeMonitor *monitor;
  GMutex mutex;
  GList *vms;
  GHashTable *ids;
  GHashTable *shortcuts;
  MeloFileDB *fdb;
  GstDiscoverer *discoverer;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloBrowserFile, melo_browser_file, MELO_TYPE_BROWSER)

static void
melo_browser_file_finalize (GObject *gobject)
{
  MeloBrowserFile *browser_file = MELO_BROWSER_FILE (gobject);
  MeloBrowserFilePrivate *priv =
                          melo_browser_file_get_instance_private (browser_file);

  /* Stop discoverer and release it */
  gst_discoverer_stop (priv->discoverer);
  gst_object_unref (priv->discoverer);

  /* Release volume monitor */
  g_object_unref (priv->monitor);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Free hash table */
  g_hash_table_remove_all (priv->shortcuts);
  g_hash_table_remove_all (priv->ids);
  g_hash_table_unref (priv->shortcuts);
  g_hash_table_unref (priv->ids);

  /* Free volume and mount list */
  g_list_free_full (priv->vms, g_object_unref);

  /* Free local path */
  g_free (priv->local_path);

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
  bclass->get_tags = melo_browser_file_get_tags;
  bclass->add = melo_browser_file_add;
  bclass->play = melo_browser_file_play;
  bclass->remove = melo_browser_file_remove;

  bclass->get_cover = melo_browser_file_get_cover;

  /* Add custom finalize() function */
  oclass->finalize = melo_browser_file_finalize;
}

static void
melo_browser_file_init (MeloBrowserFile *self)
{
  MeloBrowserFilePrivate *priv = melo_browser_file_get_instance_private (self);

  self->priv = priv;

  /* Set local path to default */
  priv->local_path = g_strdup ("");

  /* Get volume monitor */
  priv->monitor = g_volume_monitor_get ();

  /* Init mutex */
  g_mutex_init (&priv->mutex);

  /* Get list of volumes and mounts and sort by name */
  priv->vms = g_list_concat (g_volume_monitor_get_volumes (priv->monitor),
                             g_volume_monitor_get_mounts (priv->monitor));
  priv->vms = g_list_sort (priv->vms, (GCompareFunc) vms_cmp);

  /* Init Hash table for IDs */
  priv->ids = g_hash_table_new (g_str_hash, g_str_equal);

  /* Generate id for each volumes and mounts */
  g_list_foreach (priv->vms, (GFunc) melo_browser_file_set_id, priv);

  /* Subscribe to volume and mount events of volume monitor */
  g_signal_connect (priv->monitor, "volume-added",
                    (GCallback) vms_added, priv);
  g_signal_connect (priv->monitor, "volume-removed",
                    (GCallback) vms_removed, priv);
  g_signal_connect (priv->monitor, "mount_added",
                    (GCallback) vms_added, priv);
  g_signal_connect (priv->monitor, "mount_removed",
                    (GCallback) vms_removed, priv);

  /* Init Hash table for shortcuts */
  priv->shortcuts = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free, g_free);

  /* Create a new Gstreamer discoverer for async tags discovering */
  priv->discoverer = gst_discoverer_new (GST_SECOND, NULL);
  gst_discoverer_start (priv->discoverer);

  /* Subscribe to discovered event of discoverer */
  g_signal_connect (priv->discoverer, "discovered",
                    (GCallback) on_discovered, self);
}

void
melo_browser_file_set_local_path (MeloBrowserFile *bfile, const gchar *path)
{
  g_free (bfile->priv->local_path);
  bfile->priv->local_path = g_strdup (path);
}

void
melo_browser_file_set_db (MeloBrowserFile *bfile, MeloFileDB *fdb)
{
  bfile->priv->fdb = fdb;
}

static const MeloBrowserInfo *
melo_browser_file_get_info (MeloBrowser *browser)
{
  return &melo_browser_file_info;
}

static void
melo_browser_file_set_id (GObject *obj, MeloBrowserFilePrivate *priv)
{
  gchar *sha1, *id;

  /* Calculate sha1 from GVolume or GMount instance pointer */
  sha1 = g_compute_checksum_for_data (G_CHECKSUM_SHA1,
                                      (const guchar *) &obj, sizeof (obj));

  /* Keep only MELO_BROWSER_FILE_ID_LENGTH first characters to create ID */
  id = g_strndup (sha1, MELO_BROWSER_FILE_ID_LENGTH);
  g_free (sha1);

  /* Add to object and internal hash table */
  g_object_set_data_full (obj, MELO_BROWSER_FILE_ID, id, g_free);
  g_hash_table_insert (priv->ids, id, obj);
}

static gint
vms_cmp (GObject *a, GObject *b)
{
  gchar *na, *nb;
  gint ret;

  /* Get name for object a */
  if (G_IS_VOLUME (a))
    na = g_volume_get_name (G_VOLUME (a));
  else
    na = g_mount_get_name (G_MOUNT (a));

  /* Get name for object b */
  if (G_IS_VOLUME (b))
    nb = g_volume_get_name (G_VOLUME (b));
  else
    nb = g_mount_get_name (G_MOUNT (b));

  /* Compare names */
  ret = g_strcmp0 (na, nb);
  g_free (na);
  g_free (nb);

  return ret;
}

static void
vms_added(GVolumeMonitor *monitor, GObject *obj, MeloBrowserFilePrivate *priv)
{
  /* Lock volume/mount list */
  g_mutex_lock (&priv->mutex);

  /* Add to volume / mount list and set id to object */
  priv->vms = g_list_insert_sorted (priv->vms, g_object_ref (obj),
                                    (GCompareFunc) vms_cmp);
  melo_browser_file_set_id (obj, priv);

  /* Unlock volume/mount list */
  g_mutex_unlock (&priv->mutex);
}

static void
vms_removed(GVolumeMonitor *monitor, GObject *obj, MeloBrowserFilePrivate *priv)
{
  const gchar *id;

  /* Lock volume/mount list */
  g_mutex_lock (&priv->mutex);

  /* Remove from hash table */
  id = g_object_get_data (obj, MELO_BROWSER_FILE_ID);
  g_hash_table_remove (priv->ids, id);

  /* Remove from volume / mount list */
  priv->vms = g_list_remove (priv->vms, obj);
  g_object_unref (obj);

  /* Unlock volume/mount list */
  g_mutex_unlock (&priv->mutex);
}

static void
async_done (GObject *obj, GAsyncResult *res, gpointer user_data)
{
  GMutex *mutex = user_data;

  /* Signal end of mount task */
  g_mutex_unlock (mutex);
}

static void
ask_password (GMountOperation *op, const char *message,
              const char *default_user, const char *default_domain,
              GAskPasswordFlags flags)
{
  /* We have already tried to connect anonymously */
  if (g_mount_operation_get_anonymous (op)) {
    g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);
    return;
  }

  /* Try to connect anonymously */
  g_mount_operation_set_anonymous (op, TRUE);
  g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
}

static const gchar *
melo_brower_file_fix_path (const gchar *path)
{
  while (*path != '\0' && *path == '/')
    path++;
  return path;
}

static MeloTags *
melo_browser_file_discover_tags (MeloBrowserFile *bfile,
                                 GstDiscovererInfo *info, const gchar *path,
                                 gint path_id, const gchar *file)
{
  MeloBrowserFilePrivate *priv = bfile->priv;
  const GstTagList *gtags;
  MeloTags *tags = NULL;
  gchar *cover_file = NULL;

  /* Get GstTagLsit */
  gtags = gst_discoverer_info_get_tags (info);

  /* Convert to MeloTags */
  if (gtags)
    tags = melo_tags_new_from_gst_tag_list (gtags, MELO_TAGS_FIELDS_FULL);

  /* Add file to database if tags are available */
  if (priv->fdb && tags) {
    if (path)
      melo_file_db_add_tags (priv->fdb, path, file, 0, tags, &cover_file);
    else
      melo_file_db_add_tags2 (priv->fdb, path_id, file, 0, tags, &cover_file);

    /* Add cover URL */
    if (cover_file) {
      melo_tags_set_cover_url (tags, G_OBJECT (bfile), cover_file, NULL);
      g_free (cover_file);
    }
  }

  return tags;
}

static void
on_discovered (GstDiscoverer *discoverer, GstDiscovererInfo *info,
               GError *error, gpointer user_data)
{
  MeloBrowserFile *bfile = user_data;
  gchar *path, *file;
  const gchar *uri;
  MeloTags *tags;

  if (error || !info)
    return;

  /* Get dirname and basename */
  uri = gst_discoverer_info_get_uri (info);
  path = g_path_get_dirname (uri);
  file = g_path_get_basename (uri);

  /* Add media to database */
  tags = melo_browser_file_discover_tags (bfile, info, path, 0, file);
  if (tags)
    melo_tags_unref (tags);
  g_free (path);
  g_free (file);
}

static GList *
melo_browser_file_list (MeloBrowserFile * bfile, GFile *dir,
                        MeloBrowserTagsMode tags_mode,
                        MeloTagsFields tags_fields)
{
  MeloBrowserFilePrivate *priv = bfile->priv;
  GstDiscoverer *disco = NULL;
  GFileEnumerator *dir_enum;
  GFileInfo *info;
  GList *dir_list = NULL;
  GList *list = NULL;
  gchar *path, *path_uri;
  gint path_id;

  /* Get details */
  if (g_file_query_file_type (dir, 0, NULL) != G_FILE_TYPE_DIRECTORY)
    return NULL;

  /* Get list of directory */
  dir_enum = g_file_enumerate_children (dir,
                                    G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                    G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                                    G_FILE_ATTRIBUTE_STANDARD_TARGET_URI ","
                                    G_FILE_ATTRIBUTE_STANDARD_NAME,
                                    0, NULL, NULL);
  if (!dir_enum)
    return NULL;

  /* Get path from directory */
  path_uri = g_file_get_uri (dir);
  path = g_uri_unescape_string (path_uri, NULL);
  g_free (path_uri);

  /* Get path ID for faster database find / insertion */
  melo_file_db_get_path_id (priv->fdb, path, TRUE, &path_id);

  /* Create list */
  while ((info = g_file_enumerator_next_file (dir_enum, NULL, NULL))) {
    MeloBrowserItem *item;
    const gchar *itype;
    gchar *name;
    gchar *add = NULL;
    GFileType type;
    GList **l;

    /* Get item type */
    type = g_file_info_get_file_type (info);
    if (type == G_FILE_TYPE_REGULAR) {
      itype = "file";
      name = g_strdup (g_file_info_get_name (info));
      add = g_strdup ("Add to playlist");
      l = &list;
    } else if (type == G_FILE_TYPE_DIRECTORY) {
      itype = "directory";
      name = g_strdup (g_file_info_get_name (info));
      l = &dir_list;
    } else if (type == G_FILE_TYPE_SHORTCUT ||
               type == G_FILE_TYPE_MOUNTABLE) {
      const gchar *uri;
      gchar *sha1;
      itype = "directory";
      l = &dir_list;
      uri = g_file_info_get_attribute_string (info,
                                          G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);

      /* Calculate sha1 from target URI */
      sha1 = g_compute_checksum_for_data (G_CHECKSUM_SHA1,
                                          (const guchar *) uri, strlen (uri));

      /* Keep only MELO_BROWSER_FILE_ID_LENGTH first characters to create ID */
      name = g_strndup (sha1, MELO_BROWSER_FILE_ID_LENGTH);
      g_free (sha1);

      /* Add shortcut to hash table */
      if (!g_hash_table_lookup (priv->shortcuts, name))
        g_hash_table_insert (priv->shortcuts, g_strdup (name), g_strdup (uri));
    } else {
      g_object_unref (info);
      continue;
    }

    /* Create a new browser item */
    item = melo_browser_item_new (NULL, itype);
    item->name = name;
    item->full_name = g_strdup (g_file_info_get_display_name (info));
    item->add = add;

    /* Insert into list */
    if (type == G_FILE_TYPE_REGULAR) {
      if (tags_mode != MELO_BROWSER_TAGS_MODE_NONE) {
        MeloTags *tags = NULL;

        /* Get file from database */
        tags = melo_file_db_find_one_song (priv->fdb, G_OBJECT (bfile),
                         tags_mode == MELO_BROWSER_TAGS_MODE_NONE_WITH_CACHING ?
                                            MELO_TAGS_FIELDS_NONE : tags_fields,
                         MELO_FILE_DB_FIELDS_PATH_ID, path_id,
                         MELO_FILE_DB_FIELDS_FILE, name,
                         MELO_FILE_DB_FIELDS_END);

        /* No tags available in database */
        if (!tags) {
          gchar *file_uri;

          /* Generate complete file URI */
          file_uri = g_strdup_printf ("%s/%s", path, name);

          if (tags_mode == MELO_BROWSER_TAGS_MODE_FULL) {
            GstDiscovererInfo *info;

            /* Create a new discoverer if not yet done */
            if (!disco)
              disco = gst_discoverer_new (GST_SECOND, NULL);

            /* Get tags from URI */
            info = gst_discoverer_discover_uri (disco, file_uri, NULL);
            if (info) {
              tags = melo_browser_file_discover_tags (bfile, info, NULL,
                                                      path_id, name);
              g_object_unref (info);
            }
          } else if (tags_mode == MELO_BROWSER_TAGS_MODE_NONE_WITH_CACHING ||
                     tags_mode == MELO_BROWSER_TAGS_MODE_FULL_WITH_CACHING) {
            /* Add URI to discoverer pending list */
            gst_discoverer_discover_uri_async (priv->discoverer, file_uri);
          }
          g_free (file_uri);
        }

        /* Add tags to item */
        if (tags) {
          if (tags_mode == MELO_BROWSER_TAGS_MODE_NONE_WITH_CACHING)
            melo_tags_unref (tags);
          else
            item->tags = tags;
        }
      }
      list = g_list_prepend (list, item);
    } else {
      dir_list = g_list_prepend (dir_list, item);
    }
    g_object_unref (info);
  }
  g_object_unref (dir_enum);
  g_free (path);

  /* Free discoverer */
  if (disco)
    gst_object_unref (disco);

  /* Sort both lists */
  list = g_list_sort (list, (GCompareFunc) melo_browser_item_cmp);
  dir_list = g_list_sort (dir_list, (GCompareFunc) melo_browser_item_cmp);

  /* Merge both list */
  return g_list_concat (dir_list, list);
}

static GList *
melo_browser_file_get_local_list (MeloBrowserFile *bfile, const gchar *uri,
                                  MeloBrowserTagsMode tags_mode,
                                  MeloTagsFields tags_fields)
{
  GFile *dir;
  GList *list;

  /* Open directory */
  dir = g_file_new_for_uri (uri);
  if (!dir)
    return NULL;

  /* Get list from GFile */
  list = melo_browser_file_list (bfile, dir, tags_mode, tags_fields);
  g_object_unref (dir);

  return list;
}

static GMount *
melo_browser_file_get_mount (MeloBrowserFile *bfile, const gchar *path)
{
  MeloBrowserFilePrivate *priv = bfile->priv;
  GMount *mount;
  GObject *obj;
  gchar *id;

  /* Get volume / mount ID */
  id = g_strndup (path, MELO_BROWSER_FILE_ID_LENGTH);

  /* Lock volume/mount list */
  g_mutex_lock (&priv->mutex);

  /* Get volume / mount from hash table */
  obj = g_hash_table_lookup (priv->ids, id);
  g_free (id);

  /* Volume / mount id not found */
  if (!obj) {
    g_mutex_unlock (&priv->mutex);
    return NULL;
  }
  g_object_ref (obj);

  /* Unlock volume/mount list */
  g_mutex_unlock (&priv->mutex);

  /* Extract mount from object */
  if (G_IS_VOLUME (obj)) {
    GVolume *vol = G_VOLUME (obj);

    /* Get mount */
    mount = g_volume_get_mount (vol);
    if (!mount) {
      GMutex mutex;

      /* Mount volume */
      g_mutex_init (&mutex);
      g_mutex_lock (&mutex);
      g_volume_mount (vol, 0, NULL, NULL, async_done, &mutex);

      /* Wait for end of mount operation and get GMount */
      g_mutex_lock (&mutex);
      mount = g_volume_get_mount (vol);
      g_mutex_unlock (&mutex);
      g_mutex_clear (&mutex);
    }

    g_object_unref (vol);
  } else
    mount = G_MOUNT (obj);

  return mount;
}

static GList *
melo_browser_file_get_volume_list (MeloBrowserFile *bfile, const gchar *path,
                                   MeloBrowserTagsMode tags_mode,
                                   MeloTagsFields tags_fields)
{
  GMount *mount;
  GFile *root, *dir;
  GList *list;

  /* Get mount assocated to path */
  mount = melo_browser_file_get_mount (bfile, path);
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
  path += MELO_BROWSER_FILE_ID_LENGTH;
  if (*path != '\0')
    path++;
  dir = g_file_resolve_relative_path (root, path);
  if (!dir) {
    g_object_unref (root);
    return NULL;
  }
  g_object_unref (root);

  /* List files from our GFile  */
  list = melo_browser_file_list (bfile, dir, tags_mode, tags_fields);
  g_object_unref (dir);

  return list;
}

static GList *
melo_browser_file_list_volumes (MeloBrowserFile *bfile, GList *list)
{
  MeloBrowserFilePrivate *priv = bfile->priv;
  GList *l;

  /* Lock volume/mount list */
  g_mutex_lock (&priv->mutex);

  /* Get volume / mount list */
  for (l = priv->vms; l != NULL; l = l->next) {
    GObject *obj = G_OBJECT (l->data);
    MeloBrowserItem *item;
    gchar *full_name, *id;
    gchar *remove = NULL;
    GVolume *vol;
    GMount *mnt;

    /* Get mount if possible or volume */
    if (G_IS_VOLUME (obj)) {
      vol = G_VOLUME (obj);

      /* Get mount */
      mnt = g_volume_get_mount (vol);
      if (!mnt)
        g_object_ref (vol);
    } else {
      mnt = G_MOUNT (obj);

      /* Skip if mount has a volume */
      vol = g_mount_get_volume (mnt);
      if (vol) {
        g_object_unref (vol);
        continue;
      } else
        g_object_ref (mnt);
    }

    /* Get id and full name */
    if (mnt) {
      full_name = g_mount_get_name (mnt);
      id = g_strdup (g_object_get_data (G_OBJECT (mnt), MELO_BROWSER_FILE_ID));
      if (g_mount_can_unmount (mnt))
        remove = g_strdup ("eject");
      g_object_unref (mnt);
    } else {
      full_name = g_volume_get_name (vol);
      id = g_strdup (g_object_get_data (G_OBJECT (vol), MELO_BROWSER_FILE_ID));
      if (g_volume_can_eject (vol))
        remove = g_strdup ("eject");
      g_object_unref (vol);
    }

    /* Create item */
    item = melo_browser_item_new (NULL, "category");
    item->name = id;
    item->full_name = full_name;
    item->remove = remove;
    list = g_list_append(list, item);
  }

  /* Unlock volume/mount list */
  g_mutex_unlock (&priv->mutex);

  return list;
}


static gchar *
melo_browser_file_get_network_uri (MeloBrowserFile *bfile, const gchar *path)
{
  MeloBrowserFilePrivate *priv = bfile->priv;
  const gchar *shortcut = NULL;
  gint len;

  /* Convert all shortcuts to final URI */
  len = strlen (path);
  while (len >= MELO_BROWSER_FILE_ID_LENGTH &&
         path[MELO_BROWSER_FILE_ID_LENGTH] == '/') {
    const gchar *s;
    gchar *id;

    /* Get ID */
    id = g_strndup (path, MELO_BROWSER_FILE_ID_LENGTH);

    /* Find in shortcuts list */
    s = g_hash_table_lookup (priv->shortcuts, id);
    g_free (id);
    if (!s)
      break;

    /* Save shortcut and look for next */
    shortcut = s;
    path += MELO_BROWSER_FILE_ID_LENGTH + 1;
    len -= MELO_BROWSER_FILE_ID_LENGTH + 1;
  }

  /* Path contains a shortcut */
  if (shortcut) {
    GError *error = NULL;
    GFileInfo *info;
    GFile *dir;
    gchar *furi, *uri;

    /* Get file from shortcut */
    dir = g_file_new_for_uri (shortcut);
    if (!dir)
      return NULL;

    /* Get file details */
    info = g_file_query_info (dir, G_FILE_ATTRIBUTE_STANDARD_TYPE,
                              0, NULL, &error);
    if (!info && error->code != G_IO_ERROR_NOT_FOUND) {
      GMountOperation *op;
      GMutex mutex;

      /* Create mount operation for authentication */
      op = g_mount_operation_new ();
      g_signal_connect (op, "ask_password", G_CALLBACK (ask_password), NULL);

      /* Mount */
      g_mutex_init (&mutex);
      g_mutex_lock (&mutex);
      g_file_mount_enclosing_volume (dir, 0, op, NULL, async_done, &mutex);

      /* Wait end of operation */
      g_mutex_lock (&mutex);
      g_clear_error (&error);
      g_mutex_unlock (&mutex);
      g_mutex_clear (&mutex);

      g_object_unref (op);
    } else {
      g_object_unref (info);
    }

    /* Generate final URI */
    furi = g_file_get_uri (dir);
    uri = g_strdup_printf ("%s%s", furi, path);
    g_object_unref (dir);
    g_free (furi);

    return uri;
  }

  /* Generate default URI */
  return g_strdup_printf ("network://%s", path);
}

static GList *
melo_browser_file_get_network_list (MeloBrowserFile *bfile, const gchar *path,
                                    MeloBrowserTagsMode tags_mode,
                                    MeloTagsFields tags_fields)
{
  GList *list = NULL;
  GFile *dir;
  gchar *uri;

  /* Generate URI from path */
  uri = melo_browser_file_get_network_uri (bfile, path);
  if (!uri)
    return NULL;

  /* Get list from URI */
  dir = g_file_new_for_uri (uri);
  g_free (uri);
  if (!dir)
    return NULL;

  /* Get list from GFile */
  list = melo_browser_file_list (bfile, dir, tags_mode, tags_fields);
  g_object_unref (dir);

  return list;
}

static MeloBrowserList *
melo_browser_file_get_list (MeloBrowser *browser, const gchar *path,
                            gint offset, gint count, const gchar *token,
                            MeloBrowserTagsMode tags_mode,
                            MeloTagsFields tags_fields)
{
  MeloBrowserFile *bfile = MELO_BROWSER_FILE (browser);
  MeloBrowserList *list;
  GList *l;

  /* Create browser list */
  list = melo_browser_list_new (path);
  if (!list)
    return NULL;

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
    list->items = g_list_append(list->items, item);

    /* Add Network entry for scanning network */
    item = melo_browser_item_new ("network", "category");
    item->full_name = g_strdup ("Network");
    list->items = g_list_append(list->items, item);

    /* Add local volumes to list */
    list->items = melo_browser_file_list_volumes (bfile, list->items);
  } else if (g_str_has_prefix (path, "local")) {
    gchar *uri;

    /* Get file path: "/local/" */
    path = melo_brower_file_fix_path (path + 5);
    uri = g_strdup_printf ("file:%s/%s", bfile->priv->local_path, path);
    list->items = melo_browser_file_get_local_list (bfile, uri, tags_mode,
                                                    tags_fields);
    g_free (uri);
  } else if (g_str_has_prefix (path, "network")) {
    /* Get file path: "/network/" */
    list->items = melo_browser_file_get_network_list (bfile, path + 8,
                                                      tags_mode, tags_fields);
  } else if (strlen (path) >= MELO_BROWSER_FILE_ID_LENGTH &&
             path[MELO_BROWSER_FILE_ID_LENGTH] == '/') {
    /* Volume path: "/VOLUME_ID/" */
    list->items = melo_browser_file_get_volume_list (bfile, path, tags_mode,
                                                     tags_fields);
  }

  /* Keep only requested part of list */
  l = list->items;
  while (l != NULL) {
    GList *next = l->next;

    /* Remove item when not in requested part */
    if (!count || list->count < offset) {
      MeloBrowserItem *item = (MeloBrowserItem *) l->data;
      list->items = g_list_delete_link (list->items, l);
      melo_browser_item_free (item);
    }
    else
      count--;

    /* Update items count */
    list->count++;
    l = next;
  }

  return list;
}

static gchar *
melo_browser_file_get_uri (MeloBrowser *browser, const gchar *path)
{
  MeloBrowserFile *bfile = MELO_BROWSER_FILE (browser);
  gchar *uri;

  g_return_val_if_fail (browser->player, FALSE);
  g_return_val_if_fail (*path && *path == '/', FALSE);
  path++;

  /* Generate URI from path */
  if (g_str_has_prefix (path, "local/")) {
    GFile *root;
    path = melo_brower_file_fix_path (path + 5);
    uri = g_strdup_printf ("file:%s/%s", bfile->priv->local_path, path);
    root = g_file_new_for_uri (uri);
    g_free (uri);
    if (!root)
      return NULL;
    uri = g_file_get_uri (root);
    g_object_unref (root);
  } else if (g_str_has_prefix (path, "network/")) {
    uri = melo_browser_file_get_network_uri (bfile, path + 8);
  } else if (strlen (path) >= MELO_BROWSER_FILE_ID_LENGTH &&
             path[MELO_BROWSER_FILE_ID_LENGTH] == '/') {
    GMount *mount;
    GFile *root;
    gchar *root_uri;

    /* Get mount from volume / mount ID */
    mount = melo_browser_file_get_mount (MELO_BROWSER_FILE (browser), path);
    if (!mount)
      return NULL;

    /* Get root */
    root = g_mount_get_root (mount);
    g_object_unref (mount);
    if (!root)
      return NULL;

    /* Get root URI */
    path += MELO_BROWSER_FILE_ID_LENGTH;
    root_uri = g_file_get_uri (root);
    g_object_unref (root);

    /* Create complete URI */
    uri = g_strconcat (root_uri, path, NULL);
    g_free (root_uri);
  } else
    return NULL;

  return uri;
}

static MeloTags *
melo_browser_file_get_tags_from_uri (MeloBrowserFile *bfile, const gchar *uri,
                                     MeloTagsFields fields)
{
  MeloBrowserFilePrivate *priv = bfile->priv;
  GstDiscovererInfo *info;
  GstDiscoverer *disco;
  MeloTags *tags = NULL;
  gchar *dir, *file;

  /* Get dirname and basename */
  dir = g_path_get_dirname (uri);
  file = g_path_get_basename (uri);

  /* Get tags from database */
  if (priv->fdb) {
    tags = melo_file_db_find_one_song (priv->fdb, G_OBJECT (bfile), fields,
                                       MELO_FILE_DB_FIELDS_PATH, dir,
                                       MELO_FILE_DB_FIELDS_FILE, file,
                                       MELO_FILE_DB_FIELDS_END);
    if (tags)
      goto end;
  }

  /* Create a new discoverer */
  disco = gst_discoverer_new (GST_SECOND, NULL);
  if (!disco)
    goto end;

  /* Get tags from URI */
  info = gst_discoverer_discover_uri (disco, uri, NULL);
  if (info) {
    tags = melo_browser_file_discover_tags (bfile, info, dir, 0, file);
    g_object_unref (info);
  }

  /* Free discoverer */
  gst_object_unref (disco);

end:
  /* Free URI parts */
  g_free (file);
  g_free (dir);

  return tags;
}

static MeloTags *
melo_browser_file_get_tags (MeloBrowser *browser, const gchar *path,
                            MeloTagsFields fields)
{
  MeloBrowserFile *bfile = MELO_BROWSER_FILE (browser);
  MeloBrowserFilePrivate *priv = bfile->priv;
  MeloTags *tags = NULL;
  gchar *uri, *uuri;

  /* Get URI from path */
  uri = melo_browser_file_get_uri (browser, path);
  if (!uri)
    return NULL;

  /* Unescape URI */
  uuri = g_uri_unescape_string (uri, NULL);
  g_free (uri);
  if (!uuri)
    return NULL;

  /* Get tags from URI */
  tags = melo_browser_file_get_tags_from_uri (bfile, uuri, fields);
  g_free (uuri);

  return tags;
}

static gboolean
melo_browser_file_add (MeloBrowser *browser, const gchar *path)
{
  MeloBrowserFile *bfile = MELO_BROWSER_FILE (browser);
  gboolean ret;
  MeloTags *tags;
  gchar *uri, *uuri;
  gchar *name;

  /* Get final URI from path */
  uri = melo_browser_file_get_uri (browser, path);
  if (!uri)
    return FALSE;

  /* Unescape for name */
  uuri = g_uri_unescape_string (uri, NULL);
  name = g_path_get_basename (uuri);

  /* Get tags from URI */
  tags = melo_browser_file_get_tags_from_uri (bfile, uuri,
                                              MELO_TAGS_FIELDS_FULL);

  /* Add with URI */
  ret = melo_player_add (browser->player, uri, name, tags);
  melo_tags_unref (tags);
  g_free (name);
  g_free (uuri);
  g_free (uri);

  return ret;
}

static gboolean
melo_browser_file_play (MeloBrowser *browser, const gchar *path)
{
  MeloBrowserFile *bfile = MELO_BROWSER_FILE (browser);
  gboolean ret;
  MeloTags *tags;
  gchar *uri, *uuri;
  gchar *name;

  /* Get final URI from path */
  uri = melo_browser_file_get_uri (browser, path);
  if (!uri)
    return FALSE;

  /* Unescape for name */
  uuri = g_uri_unescape_string (uri, NULL);
  name = g_path_get_basename (uuri);

  /* Get tags from URI */
  tags = melo_browser_file_get_tags_from_uri (bfile, uuri,
                                              MELO_TAGS_FIELDS_FULL);

  /* Play with URI */
  ret = melo_player_play (browser->player, uri, name, tags, TRUE);
  melo_tags_unref (tags);
  g_free (name);
  g_free (uuri);
  g_free (uri);

  return ret;
}

static gboolean
melo_browser_file_remove (MeloBrowser *browser, const gchar *path)
{
  MeloBrowserFile *bfile = MELO_BROWSER_FILE (browser);
  MeloBrowserFilePrivate *priv = bfile->priv;
  GMutex mutex;
  GObject *obj;
  GVolume *vol;
  GMount *mnt;
  gchar *id;

  g_return_val_if_fail (*path && *path == '/', FALSE);
  path++;

  /* Get ID */
  id = g_strndup (path, MELO_BROWSER_FILE_ID_LENGTH);

  /* Lock volume / mount list */
  g_mutex_lock (&priv->mutex);

  /* Get GObject from hash table */
  obj = g_hash_table_lookup (priv->ids, id);
  g_free (id);

  /* Not found */
  if (!obj) {
    g_mutex_unlock (&priv->mutex);
    return FALSE;
  }
  g_object_ref (obj);

  /* Unlock volume / mount list */
  g_mutex_unlock (&priv->mutex);

  /* Eject */
  if (G_IS_MOUNT (obj)) {
    mnt = G_MOUNT (obj);

    /* Try to get volume */
    vol = g_mount_get_volume (mnt);
  } else {
    vol = G_VOLUME (obj);
    mnt = NULL;
  }

  /* Unmount or eject */
  g_mutex_init (&mutex);
  g_mutex_lock (&mutex);
  if (vol) {
    /* Eject */
    g_volume_eject_with_operation (vol, 0, NULL, NULL, async_done, &mutex);

    /* Wait end of operation */
    g_mutex_lock (&mutex);
    vms_removed(priv->monitor, G_OBJECT (vol), priv);
    if (mnt)
      vms_removed(priv->monitor, G_OBJECT (mnt), priv);
    g_mutex_unlock (&mutex);
  } else {
    /* Unmount */
    g_mount_unmount_with_operation (mnt, 0, NULL, NULL, async_done, &mutex);

    /* Wait end of operation */
    g_mutex_lock (&mutex);
    vms_removed(priv->monitor, G_OBJECT (mnt), priv);
    g_mutex_unlock (&mutex);
  }
  g_mutex_clear (&mutex);

  return TRUE;
}

static gboolean
melo_browser_file_get_cover (MeloBrowser *browser, const gchar *path,
                             GBytes **data, gchar **type)
{
  MeloBrowserFilePrivate *priv = (MELO_BROWSER_FILE (browser))->priv;
  GMappedFile *file;
  gchar *fpath;

  /* Generate cover file path */
  fpath = g_strdup_printf ("%s/%s", melo_file_db_get_cover_path (priv->fdb),
                           path);

  /* File doesn't not exist */
  if (!g_file_test (fpath, G_FILE_TEST_EXISTS)) {
    g_free (fpath);
    return FALSE;
  }

  /* Map file and create a GBytes */
  file = g_mapped_file_new (fpath, FALSE, NULL);
  *data = g_mapped_file_get_bytes (file);
  g_mapped_file_unref (file);
  g_free (fpath);

  return TRUE;
}
