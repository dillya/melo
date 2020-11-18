/*
 * Copyright (C) 2020 Alexandre Dilly <dillya@sparod.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#include <ctype.h>
#include <stdio.h>

#include <gio/gio.h>
#include <gst/pbutils/pbutils.h>

#include <melo/melo_cover.h>
#include <melo/melo_library.h>
#include <melo/melo_playlist.h>
#include <melo/melo_settings.h>

#define MELO_LOG_TAG "file_browser"
#include <melo/melo_log.h>

#include "browser.pb-c.h"

#include "melo_file_browser.h"
#include "melo_file_player.h"

#define ITEMS_PER_CALLBACK 100

#define MELO_FILE_BROWSER_ATTRIBUTES \
  G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME \
                                 "," G_FILE_ATTRIBUTE_STANDARD_TARGET_URI \
                                 "," G_FILE_ATTRIBUTE_STANDARD_NAME \
                                 "," G_FILE_ATTRIBUTE_TIME_MODIFIED

#define MELO_FILE_BROWSER_DEFAULT_FILTER \
  "3g2,3gp,aa,aac,aax,act,aiff,alac,amv,ape,asf,au,avi,cda,flac,flv,m2ts,m2v," \
  "m4a,m4b,m4p,m4v,mkv,mmf,mogg,mov,mp2,mp3,mp4,mpc,mpe,mpeg,mpg,mpv,mts,nsv," \
  "oga,ogg,ogv,opus,qt,ra,raw,rm,rmvb,ts,vob,wav,webm,wma,wmv,wv"

typedef enum _MeloFileBrowserType {
  MELO_FILE_BROWSER_TYPE_ROOT,
  MELO_FILE_BROWSER_TYPE_LOCAL,
  MELO_FILE_BROWSER_TYPE_NETWORK,
} MeloFileBrowserType;

typedef struct {
  uint64_t timestamp;
  Browser__Response__MediaItem *item;
  bool en_tags;
} MeloFileBrowserLib;

typedef struct {
  MeloTags *tags;
  uint64_t id;
  uint64_t timestamp;
} MeloFileBrowserActLib;

typedef struct {
  MeloRequest *req;
  GCancellable *cancel;
  char *path;

  GMountOperation *op;
  char *auth;

  GList *dirs;
  GList *files;
  unsigned int total;

  unsigned int count;
  unsigned int offset;

  GstDiscoverer *disco;
  unsigned int disco_count;
  bool done;

  uint64_t player_id;
  uint64_t path_id;
} MeloFileBrowserMediaList;

typedef struct {
  MeloRequest *req;
  GCancellable *cancel;

  Browser__Action__Type type;

  GList *list;

  GstDiscoverer *disco;
  uint64_t player_id;
  unsigned int disco_count;
  unsigned int ref_count;
} MeloFileBrowserAction;

typedef struct _MeloFileBrowserMount {
  char *id;
  char *name;
  GVolume *volume;
  GMount *mount;
} MeloFileBrowserMount;

struct _MeloFileBrowser {
  GObject parent_instance;

  char *root_path;
  GVolumeMonitor *volume_monitor;
  GHashTable *mounts;

  /* Settings */
  MeloSettingsEntry *en_network;
  MeloSettingsEntry *en_tags;
  MeloSettingsEntry *filter;
};

MELO_DEFINE_BROWSER (MeloFileBrowser, melo_file_browser)

static void melo_file_browser_settings (
    MeloBrowser *browser, MeloSettings *settings);
static bool melo_file_browser_handle_request (
    MeloBrowser *browser, const MeloMessage *msg, MeloRequest *req);
static char *melo_file_browser_get_asset (MeloBrowser *browser, const char *id);

static void volume_monitor_added_cb (
    GVolumeMonitor *monitor, GObject *obj, MeloFileBrowser *browser);
static void volume_monitor_removed_cb (
    GVolumeMonitor *monitor, GObject *obj, MeloFileBrowser *browser);

static void
melo_file_browser_finalize (GObject *object)
{
  MeloFileBrowser *browser = MELO_FILE_BROWSER (object);

  /* Release volume monitor */
  if (browser->volume_monitor) {
    MeloFileBrowserMount *bm;
    GHashTableIter iter;

    /* Release mount list */
    g_hash_table_iter_init (&iter, browser->mounts);
    while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &bm)) {
      g_clear_object (&bm->volume);
      g_clear_object (&bm->mount);
      g_free (bm->name);
      g_free (bm->id);
      free (bm);
    }

    /* Destroy hash table */
    g_hash_table_destroy (browser->mounts);

    /* Release volume monitor */
    g_object_unref (browser->volume_monitor);
  }

  /* Free root path */
  g_free (browser->root_path);

  /* Chain finalize */
  G_OBJECT_CLASS (melo_file_browser_parent_class)->finalize (object);
}

static void
melo_file_browser_class_init (MeloFileBrowserClass *klass)
{
  MeloBrowserClass *parent_class = MELO_BROWSER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Setup callbacks */
  parent_class->settings = melo_file_browser_settings;
  parent_class->handle_request = melo_file_browser_handle_request;
  parent_class->get_asset = melo_file_browser_get_asset;

  /* Set finalize */
  object_class->finalize = melo_file_browser_finalize;
}

static void
melo_file_browser_init (MeloFileBrowser *self)
{
  const char *root;

  /* Use user's home directory by default */
  root = g_get_home_dir ();
  if (!root || *root != '/')
    root = "/";
  self->root_path = g_strconcat ("file://", root, NULL);

  /* Get default volume monitor */
  self->volume_monitor = g_volume_monitor_get ();
  if (self->volume_monitor) {
    /* Create hash table */
    self->mounts = g_hash_table_new (g_direct_hash, g_direct_equal);
    if (self->mounts) {
      GList *list;

      /* Fill mount list with volumes first */
      list = g_volume_monitor_get_volumes (self->volume_monitor);
      while (list) {
        volume_monitor_added_cb (self->volume_monitor, list->data, self);
        g_object_unref (list->data);
        list = g_list_delete_link (list, list);
      }

      /* Fill mount list with mounts */
      list = g_volume_monitor_get_mounts (self->volume_monitor);
      while (list) {
        volume_monitor_added_cb (self->volume_monitor, list->data, self);
        g_object_unref (list->data);
        list = g_list_delete_link (list, list);
      }
    }

    /* Subscribe to volume and mount events */
    g_signal_connect (self->volume_monitor, "volume-added",
        (GCallback) volume_monitor_added_cb, self);
    g_signal_connect (self->volume_monitor, "volume-removed",
        (GCallback) volume_monitor_removed_cb, self);
    g_signal_connect (self->volume_monitor, "mount_added",
        (GCallback) volume_monitor_added_cb, self);
    g_signal_connect (self->volume_monitor, "mount_removed",
        (GCallback) volume_monitor_removed_cb, self);
  }
}

MeloFileBrowser *
melo_file_browser_new ()
{
  return g_object_new (MELO_TYPE_FILE_BROWSER, "id", MELO_FILE_BROWSER_ID,
      "name", "Files", "description",
      "Browse in your local and network device(s)", "icon", "fa:folder-open",
      NULL);
}

static void
melo_file_browser_settings (MeloBrowser *browser, MeloSettings *settings)
{
  MeloFileBrowser *fbrowser = MELO_FILE_BROWSER (browser);
  MeloSettingsGroup *group;

  /* Create global group */
  group =
      melo_settings_add_group (settings, "global", "Global", NULL, NULL, NULL);
  melo_settings_group_add_string (group, "path", "Local path",
      "Directory path for Local files", fbrowser->root_path, NULL,
      MELO_SETTINGS_FLAG_READ_ONLY);
  fbrowser->en_network = melo_settings_group_add_boolean (group, "network",
      "Enable network", "Enable network device discovering and browsing", true,
      NULL, MELO_SETTINGS_FLAG_NONE);
  melo_settings_group_add_boolean (group, "removable",
      "Enable removable devices",
      "Enable removable devices support such as USB flash drive", false, NULL,
      MELO_SETTINGS_FLAG_READ_ONLY);
  fbrowser->en_tags =
      melo_settings_group_add_boolean (group, "tags", "Display media file tags",
          "Find media tags (title, artist, album, cover) and display them",
          true, NULL, MELO_SETTINGS_FLAG_NONE);
  fbrowser->filter = melo_settings_group_add_string (group, "filter",
      "File extension filter", "File extension to display",
      MELO_FILE_BROWSER_DEFAULT_FILTER, NULL, MELO_SETTINGS_FLAG_NONE);
}

static void
volume_monitor_added_cb (
    GVolumeMonitor *monitor, GObject *obj, MeloFileBrowser *browser)
{
  MeloFileBrowserMount *bm;
  GVolume *volume;
  GMount *mount;
  GObject *key;

  /* Get object */
  if (G_IS_VOLUME (obj)) {
    volume = g_object_ref (G_VOLUME (obj));
    mount = g_volume_get_mount (volume);
  } else {
    mount = g_object_ref (G_MOUNT (obj));
    volume = g_mount_get_volume (mount);
  }
  key = volume ? G_OBJECT (volume) : G_OBJECT (mount);

  /* Find mount */
  bm = g_hash_table_lookup (browser->mounts, key);
  if (!bm) {
    /* Create new entry */
    bm = malloc (sizeof (*bm));
    if (bm) {
      char path[18];

      /* Set objects */
      bm->volume = volume ? g_object_ref (volume) : NULL;
      bm->mount = mount ? g_object_ref (mount) : NULL;

      /* Set ID and name */
      bm->id = g_strdup_printf ("%llx", (unsigned long long) key);
      bm->name = volume ? g_volume_get_name (volume) : g_mount_get_name (mount);

      /* Insert into hash table */
      g_hash_table_insert (browser->mounts, key, bm);
      MELO_LOGD ("add mount '%s' '%s'", bm->id, bm->name);

      /* Notify of creation */
      snprintf (path, sizeof (path), "/%s", bm->id);
      melo_browser_send_media_created_event (MELO_BROWSER (browser), path);
    }
  } else if (!bm->volume && volume)
    bm->volume = g_object_ref (volume);
  else if (!bm->mount && mount)
    bm->mount = g_object_ref (mount);
  if (bm)
    MELO_LOGD ("mount '%s' updated: %p / %p", bm->id, bm->volume, bm->mount);

  /* Release volume and mount */
  g_clear_object (&volume);
  g_clear_object (&mount);
}

static void
volume_monitor_removed_cb (
    GVolumeMonitor *monitor, GObject *obj, MeloFileBrowser *browser)
{
  MeloFileBrowserMount *bm;
  GVolume *volume;
  GMount *mount;
  GObject *key;

  /* Get object */
  if (G_IS_VOLUME (obj)) {
    volume = g_object_ref (G_VOLUME (obj));
    mount = g_volume_get_mount (volume);
  } else {
    mount = g_object_ref (G_MOUNT (obj));
    volume = g_mount_get_volume (mount);
  }
  key = volume ? G_OBJECT (volume) : G_OBJECT (mount);

  /* Find mount */
  bm = g_hash_table_lookup (browser->mounts, key);
  if (bm) {
    /* Remove objects */
    if (key == (GObject *) volume)
      g_clear_object (&bm->volume);
    g_clear_object (&bm->mount);
    MELO_LOGD ("mount '%s' updated: %p / %p", bm->id, bm->volume, bm->mount);

    /* Remove from list */
    if (!bm->mount && !bm->volume) {
      char path[18];

      MELO_LOGD ("remove mount '%s' '%s'", bm->id, bm->name);

      /* Notify of deletion */
      snprintf (path, sizeof (path), "/%s", bm->id);
      melo_browser_send_media_deleted_event (MELO_BROWSER (browser), path);

      /* Release memory */
      g_hash_table_remove (browser->mounts, key);
      g_free (bm->name);
      g_free (bm->id);
      free (bm);
    }
  }

  /* Release volume and mount */
  g_clear_object (&volume);
  g_clear_object (&mount);
}

static gint
ginfo_cmp (const GFileInfo *a, const GFileInfo *b)
{
  return g_strcmp0 (g_file_info_get_display_name ((GFileInfo *) a),
      g_file_info_get_display_name ((GFileInfo *) b));
}

static bool
melo_file_browser_filter (MeloFileBrowser *browser, const char *name)
{
  const char *exts, *ext;
  size_t len, n = 0;

  /* Get extension list */
  if (!browser || !browser->filter ||
      !melo_settings_entry_get_string (browser->filter, &exts, NULL))
    return true;

  /* Get extension */
  ext = strrchr (name, '.');
  if (!ext)
    return false;
  len = strlen (++ext);

  /* Check extension */
  while (*exts != '\0') {
    if (tolower (*exts) == tolower (ext[n])) {
      n++;
      exts++;
      if (n < len)
        continue;
      else if (*exts == ',' || *exts == '\0')
        return true;
    }

    n = 0;
    exts = strchr (exts, ',');
    if (!exts)
      break;
    exts++;
  }

  return false;
}

static void
discover_discovered_cb (GstDiscoverer *disco, GstDiscovererInfo *info,
    GError *error, gpointer user_data)
{
  Browser__Response resp = BROWSER__RESPONSE__INIT;
  Browser__Response__MediaItem item = BROWSER__RESPONSE__MEDIA_ITEM__INIT;
  Tags__Tags item_tags = TAGS__TAGS__INIT;
  MeloFileBrowserMediaList *mlist = user_data;
  struct timespec tp;
  MeloMessage *msg;
  MeloTags *tags;

  /* Prepare message */
  resp.resp_case = BROWSER__RESPONSE__RESP_MEDIA_ITEM;
  resp.media_item = &item;
  item.id = g_path_get_basename (gst_discoverer_info_get_uri (info));
  item.tags = &item_tags;

  /* Get media tags */
  tags = melo_tags_new_from_taglist (melo_request_get_object (mlist->req),
      gst_discoverer_info_get_tags (info));
  if (tags) {
    item_tags.title = (char *) melo_tags_get_title (tags);
    item_tags.artist = (char *) melo_tags_get_artist (tags);
    item_tags.album = (char *) melo_tags_get_album (tags);
    item_tags.genre = (char *) melo_tags_get_genre (tags);
    item_tags.track = melo_tags_get_track (tags);
    item_tags.cover = (char *) melo_tags_get_cover (tags);
  }

  /* Get timestamp for now */
  clock_gettime (CLOCK_REALTIME, &tp);

  /* Update media to library */
  melo_library_add_media (NULL, mlist->player_id, NULL, mlist->path_id, item.id,
      0,
      MELO_LIBRARY_SELECT (TIMESTAMP) | MELO_LIBRARY_SELECT (TITLE) |
          MELO_LIBRARY_SELECT (ARTIST) | MELO_LIBRARY_SELECT (ALBUM) |
          MELO_LIBRARY_SELECT (GENRE) | MELO_LIBRARY_SELECT (TRACK) |
          MELO_LIBRARY_SELECT (COVER),
      NULL, tags, tp.tv_sec, MELO_LIBRARY_FLAG_NONE);

  /* Pack message */
  msg = melo_message_new (browser__response__get_packed_size (&resp));
  melo_message_set_size (
      msg, browser__response__pack (&resp, melo_message_get_data (msg)));

  /* Free ID and tags */
  g_free (item.id);
  melo_tags_unref (tags);

  /* Send media item */
  if (!melo_request_send_response (mlist->req, msg) && mlist->done) {
    /* Stop and release discoverer */
    gst_discoverer_stop (disco);
    g_object_unref (disco);

    /* Release request */
    melo_request_complete (mlist->req);
    free (mlist);
    return;
  }

  /* Last media to discover */
  mlist->disco_count--;
  if (!mlist->disco_count && mlist->done) {
    /* Release discoverer and request */
    g_object_unref (disco);
    melo_request_complete (mlist->req);
    free (mlist);
  }
}

static bool
library_cb (const MeloLibraryData *data, MeloTags *tags, void *user_data)
{
  MeloFileBrowserLib *lib = user_data;
  Tags__Tags *item_tags;

  /* Save timestamp */
  lib->timestamp = data->timestamp;

  /* Generate tags */
  if (lib->en_tags && tags) {
    item_tags = malloc (sizeof (*item_tags));
    if (item_tags) {
      tags__tags__init (item_tags);
      item_tags->title = g_strdup (melo_tags_get_title (tags));
      item_tags->artist = g_strdup (melo_tags_get_artist (tags));
      item_tags->album = g_strdup (melo_tags_get_album (tags));
      item_tags->genre = g_strdup (melo_tags_get_genre (tags));
      item_tags->track = melo_tags_get_track (tags);
      item_tags->cover = g_strdup (melo_tags_get_cover (tags));
      lib->item->tags = item_tags;
    }
  }

  /* Set favorite status */
  lib->item->favorite = data->flags & MELO_LIBRARY_FLAG_FAVORITE;

  return true;
}

static void
next_files_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GFileEnumerator *en = G_FILE_ENUMERATOR (source_object);
  MeloFileBrowserMediaList *mlist = user_data;
  MeloFileBrowser *browser;
  GList *list;

  /* Get browser */
  browser = MELO_FILE_BROWSER (melo_request_get_object (mlist->req));

  /* Get next files list */
  list = g_file_enumerator_next_files_finish (en, res, NULL);

  /* Parse list */
  if (list == NULL) {
    static Browser__Action actions[] = {
        /* Folder actions */
        {
            .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
            .type = BROWSER__ACTION__TYPE__PLAY,
            .name = "Play all",
            .icon = "fa:play",
        },
        {
            .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
            .type = BROWSER__ACTION__TYPE__ADD,
            .name = "Add all to playlist",
            .icon = "fa:plus",
        },
        {
            .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
            .type = BROWSER__ACTION__TYPE__SCAN,
            .name = "Scan for medias",
            .icon = "fa:search",
        },
        /* File actions */
        {
            .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
            .type = BROWSER__ACTION__TYPE__PLAY,
            .name = "Play file",
            .icon = "fa:play",
        },
        {
            .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
            .type = BROWSER__ACTION__TYPE__ADD,
            .name = "Add file to playlist",
            .icon = "fa:plus",
        },
        {
            .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
            .type = BROWSER__ACTION__TYPE__SET_FAVORITE,
            .name = "Add media to favorites",
            .icon = "fa:star",
        },
        {
            .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
            .type = BROWSER__ACTION__TYPE__UNSET_FAVORITE,
            .name = "Remove media from favorites",
            .icon = "fa:star",
        },
    };
    static Browser__Action *actions_ptr[] = {
        &actions[0],
        &actions[1],
        &actions[2],
        &actions[3],
        &actions[4],
        &actions[5],
        &actions[6],
    };
    static uint32_t folder_actions[] = {0, 1, 2};
    static uint32_t file_set_fav_actions[] = {3, 4, 5};
    static uint32_t file_unset_fav_actions[] = {3, 4, 6};
    Browser__Response resp = BROWSER__RESPONSE__INIT;
    Browser__Response__MediaList media_list =
        BROWSER__RESPONSE__MEDIA_LIST__INIT;
    Browser__Response__MediaItem *items;
    unsigned int i = 0;
    MeloMessage *msg;
    bool en_tags;
    char *en_uri, *path;
    GList *l;

    /* Get tags settings */
    if (!browser ||
        !melo_settings_entry_get_boolean (browser->en_tags, &en_tags, NULL))
      en_tags = true;

    /* Get directory URI */
    en_uri = g_file_get_uri (g_file_enumerator_get_container (en));

    /* Get player and path ID */
    path = g_uri_unescape_string (en_uri, NULL);
    mlist->player_id = melo_library_get_player_id (MELO_FILE_PLAYER_ID);
    mlist->path_id = melo_library_get_path_id (path);
    g_free (path);

    /* Prepare response */
    resp.resp_case = BROWSER__RESPONSE__RESP_MEDIA_LIST;
    resp.media_list = &media_list;

    /* Set item count */
    media_list.n_items = mlist->total - mlist->offset;
    if (media_list.n_items > mlist->count)
      media_list.n_items = mlist->count;

    /* Set list offset and count */
    media_list.count = media_list.n_items;
    media_list.offset = mlist->offset;

    /* Set media list actions and folder action IDs */
    media_list.n_actions = G_N_ELEMENTS (actions_ptr);
    media_list.actions = actions_ptr;
    media_list.n_action_ids = G_N_ELEMENTS (folder_actions);
    media_list.action_ids = folder_actions;

    /* Allocate items */
    media_list.items = malloc (sizeof (*media_list.items) * media_list.n_items);
    items = malloc (sizeof (*items) * media_list.n_items);

    /* Sort directories and files by name */
    mlist->dirs = g_list_sort (mlist->dirs, (GCompareFunc) ginfo_cmp);
    mlist->files = g_list_sort (mlist->files, (GCompareFunc) ginfo_cmp);

    /* Parse directory list */
    for (l = mlist->dirs; l != NULL; l = l->next) {
      GFileInfo *info = G_FILE_INFO (l->data);

      /* Skip first and last items */
      if (mlist->offset) {
        mlist->offset--;
        continue;
      }
      if (i == mlist->count)
        break;

      /* Initialize item */
      browser__response__media_item__init (&items[i]);
      items[i].name = g_strdup (g_file_info_get_display_name (info));
      items[i].type = BROWSER__RESPONSE__MEDIA_ITEM__TYPE__FOLDER;
      items[i].n_action_ids = G_N_ELEMENTS (folder_actions);
      items[i].action_ids = folder_actions;

      /* Generate item ID */
      if (g_file_info_get_file_type (info) == G_FILE_TYPE_SHORTCUT) {
        char *uri =
            g_uri_escape_string (g_file_info_get_attribute_string (info,
                                     G_FILE_ATTRIBUTE_STANDARD_TARGET_URI),
                "", TRUE);
        items[i].id = g_strconcat ("#", uri, NULL);
        g_free (uri);
      } else
        items[i].id = g_strdup (g_file_info_get_name (info));

      /* Add item to list */
      media_list.items[i] = &items[i];
      i++;
    }

    /* Parse file list */
    for (l = mlist->files; l != NULL; l = l->next) {
      GFileInfo *info = G_FILE_INFO (l->data);
      MeloFileBrowserLib lib;
      uint64_t timestamp;

      /* Skip first and last items */
      if (mlist->offset) {
        mlist->offset--;
        continue;
      }
      if (i == mlist->count)
        break;

      /* Initialize item */
      browser__response__media_item__init (&items[i]);
      items[i].id = g_strdup (g_file_info_get_name (info));
      items[i].name = g_strdup (g_file_info_get_display_name (info));
      items[i].type = BROWSER__RESPONSE__MEDIA_ITEM__TYPE__MEDIA;

      /* Create discoverer */
      if (en_tags && !mlist->disco) {
        mlist->disco = gst_discoverer_new (GST_SECOND * 10, NULL);
        g_signal_connect (mlist->disco, "discovered",
            G_CALLBACK (discover_discovered_cb), mlist);
        gst_discoverer_start (mlist->disco);
      }

      /* Get last modified timestamp */
      timestamp = g_file_info_get_attribute_uint64 (
          info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

      /* Find media in library */
      lib.timestamp = 0;
      lib.item = &items[i];
      lib.en_tags = en_tags;
      melo_library_find (MELO_LIBRARY_TYPE_MEDIA, library_cb, &lib,
          MELO_LIBRARY_SELECT (TIMESTAMP) | MELO_LIBRARY_SELECT (TITLE) |
              MELO_LIBRARY_SELECT (ARTIST) | MELO_LIBRARY_SELECT (ALBUM) |
              MELO_LIBRARY_SELECT (GENRE) | MELO_LIBRARY_SELECT (TRACK) |
              MELO_LIBRARY_SELECT (COVER),
          1, 0, MELO_LIBRARY_FIELD_NONE, false, false,
          MELO_LIBRARY_FIELD_PLAYER_ID, mlist->player_id,
          MELO_LIBRARY_FIELD_PATH_ID, mlist->path_id, MELO_LIBRARY_FIELD_MEDIA,
          g_file_info_get_name (info), MELO_LIBRARY_FIELD_LAST);

      /* Update library and tags */
      if (lib.timestamp <= timestamp) {
        /* Add media to library */
        melo_library_add_media (NULL, mlist->player_id, NULL, mlist->path_id,
            g_file_info_get_name (info), 0,
            MELO_LIBRARY_SELECT (NAME) | MELO_LIBRARY_SELECT (TIMESTAMP),
            g_file_info_get_display_name (info), NULL, timestamp,
            MELO_LIBRARY_FLAG_NONE);

        /* Queue file to discoverer */
        if (mlist->disco) {
          char *uri =
              g_build_path ("/", en_uri, g_file_info_get_name (info), NULL);
          mlist->disco_count++;
          gst_discoverer_discover_uri_async (mlist->disco, uri);
          g_free (uri);
        }
      }

      /* Set action IDs */
      if (items[i].favorite) {
        items[i].n_action_ids = G_N_ELEMENTS (file_unset_fav_actions);
        items[i].action_ids = file_unset_fav_actions;
      } else {
        items[i].n_action_ids = G_N_ELEMENTS (file_set_fav_actions);
        items[i].action_ids = file_set_fav_actions;
      }

      /* Add item to list */
      media_list.items[i] = &items[i];
      i++;
    }

    /* Release directory URI */
    g_free (en_uri);

    /* Pack message */
    msg = melo_message_new (browser__response__get_packed_size (&resp));
    melo_message_set_size (
        msg, browser__response__pack (&resp, melo_message_get_data (msg)));

    /* Free message data */
    while (i--) {
      tags__tags__free_unpacked (items[i].tags, NULL);
      g_free (items[i].id);
      g_free (items[i].name);
    }
    free (media_list.items);
    free (items);

    /* Free lists */
    g_list_free_full (mlist->dirs, g_object_unref);
    g_list_free_full (mlist->files, g_object_unref);

    /* Send media list response */
    melo_request_send_response (mlist->req, msg);

    /* Release request, source object and asynchronous object */
    g_object_unref (en);
    g_free (mlist->auth);
    mlist->done = true;
    if (!mlist->disco_count) {
      if (mlist->disco)
        g_object_unref (mlist->disco);
      melo_request_complete (mlist->req);
      free (mlist);
    }
  } else {
    /* Extract files and directories */
    while (list) {
      GList *l = list;
      GFileInfo *info = G_FILE_INFO (l->data);
      GFileType type = g_file_info_get_file_type (info);
      const char *name;

      /* Remove file info from list */
      list = g_list_remove_link (list, l);

      /* Filter file by name and extension */
      name = g_file_info_get_name (info);
      if (!name || *name == '.' ||
          (type == G_FILE_TYPE_REGULAR &&
              !melo_file_browser_filter (browser, name))) {
        g_object_unref (info);
        g_list_free (l);
        continue;
      }

      /* Add file to internal lists */
      if (type == G_FILE_TYPE_REGULAR)
        mlist->files = g_list_concat (l, mlist->files);
      else
        mlist->dirs = g_list_concat (l, mlist->dirs);

      /* Increment file count */
      mlist->total++;
    }

    /* Enumerate next ITEMS_PER_CALLBACK files */
    g_file_enumerator_next_files_async (en, ITEMS_PER_CALLBACK,
        G_PRIORITY_DEFAULT, mlist->cancel, next_files_cb, mlist);
  }
}

static void children_cb (
    GObject *source_object, GAsyncResult *res, gpointer user_data);

static MeloMessage *
melo_file_browser_message_error (unsigned int code, const char *message)
{
  Browser__Response resp = BROWSER__RESPONSE__INIT;
  Browser__Response__Error error = BROWSER__RESPONSE__ERROR__INIT;
  MeloMessage *msg;

  /* Set error */
  error.code = code;
  error.message = (char *) message;
  resp.resp_case = BROWSER__RESPONSE__RESP_ERROR;
  resp.error = &error;

  /* Pack message */
  msg = melo_message_new (browser__response__get_packed_size (&resp));
  melo_message_set_size (
      msg, browser__response__pack (&resp, melo_message_get_data (msg)));

  return msg;
}

static void
mount_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  MeloFileBrowserMediaList *mlist = user_data;
  GFile *file = G_FILE (source_object);

  /* Finalize mount operation */
  if (!g_file_mount_enclosing_volume_finish (file, res, NULL)) {
    /* Send error response */
    melo_request_send_response (
        mlist->req, melo_file_browser_message_error (401, "Unauthorized"));

    /* Release request, source object and asynchronous object */
    melo_request_complete (mlist->req);
    g_object_unref (mlist->op);
    g_object_unref (file);
    g_free (mlist->auth);
    free (mlist);
    return;
  }

  /* Release operation */
  g_object_unref (mlist->op);

  /* Redo file enumerate again */
  g_file_enumerate_children_async (file, MELO_FILE_BROWSER_ATTRIBUTES, 0,
      G_PRIORITY_DEFAULT, mlist->cancel, children_cb, mlist);
}

static void
ask_password_cb (GMountOperation *op, gchar *message, gchar *default_user,
    gchar *default_domain, GAskPasswordFlags flags, gpointer user_data)
{
  MeloFileBrowserMediaList *mlist = user_data;

  /* Anonymous connection failed */
  if (g_mount_operation_get_anonymous (op)) {
    g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);
    return;
  }

  /* Set authentication */
  if (mlist->auth) {
    char *username, *password = NULL, *domain = NULL;

    /* Parse auth */
    username = strchr (mlist->auth, ';');
    if (username) {
      *username++ = '\0';
      domain = mlist->auth;
    } else
      username = mlist->auth;
    password = strchr (username, ':');
    if (password)
      *password++ = '\0';

    /* Set auth */
    g_mount_operation_set_domain (op, domain);
    g_mount_operation_set_username (op, username);
    g_mount_operation_set_password (op, password);

    /* Free auth */
    g_free (mlist->auth);
    mlist->auth = NULL;
  } else
    g_mount_operation_set_anonymous (op, TRUE);

  g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
}

static void
children_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  MeloFileBrowserMediaList *mlist = user_data;
  GFile *file = G_FILE (source_object);
  GFileEnumerator *en;
  GError *err = NULL;

  /* Enumeration started */
  en = g_file_enumerate_children_finish (file, res, &err);
  if (!en) {
    /* Try to mount directory when file is not found */
    if (err && err->code == G_IO_ERROR_NOT_MOUNTED) {
      /* Create mount operation */
      mlist->op = g_mount_operation_new ();

      /* Add signal for authentication */
      g_signal_connect (
          mlist->op, "ask_password", G_CALLBACK (ask_password_cb), mlist);

      /* Mount */
      g_file_mount_enclosing_volume (
          file, 0, mlist->op, mlist->cancel, mount_cb, mlist);

      g_error_free (err);
      return;
    }

    /* Release request, source object and asynchronous object */
    melo_request_complete (mlist->req);
    g_object_unref (file);
    g_free (mlist->auth);
    free (mlist);
    return;
  }

  /* Release file */
  g_object_unref (file);

  /* Enumerate ITEMS_PER_CALLBACK first files */
  g_file_enumerator_next_files_async (en, ITEMS_PER_CALLBACK,
      G_PRIORITY_DEFAULT, mlist->cancel, next_files_cb, mlist);
}

static void
request_cancelled_cb (MeloRequest *req, void *user_data)
{
  MeloFileBrowserMediaList *mlist = user_data;

  /* Stop discoverer */
  if (mlist->disco)
    gst_discoverer_stop (mlist->disco);
  mlist->disco_count = 0;

  /* Cancel GIO */
  g_cancellable_cancel (mlist->cancel);
}

static void
request_destroyed_cb (MeloRequest *req, void *user_data)
{
  g_object_unref ((GCancellable *) user_data);
}

static bool
melo_file_browser_get_file_list (GFile *file, MeloFileBrowserMediaList *mlist)
{
  /* Start file enumeration */
  g_file_enumerate_children_async (file, MELO_FILE_BROWSER_ATTRIBUTES, 0,
      G_PRIORITY_DEFAULT, mlist->cancel, children_cb, mlist);

  return true;
}

static GFile *
melo_file_browser_get_file_from_mount (GMount *mount, const char *path)
{
  GFile *root, *file;

  /* Get root from mount */
  root = g_mount_get_root (mount);
  if (!root)
    return NULL;

  /* Find next fragment */
  path = strchr (path, '/');
  path = path ? path + 1 : "";

  /* Open file */
  file = g_file_resolve_relative_path (root, path);
  g_object_unref (root);

  return file;
}

static void
mount_finished_cb (
    GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  MeloFileBrowserMediaList *mlist = user_data;
  GVolume *volume = G_VOLUME (source_object);
  GMount *mount;
  GFile *file;

  /* Mount has finished */
  if (!g_volume_mount_finish (volume, res, NULL)) {
    /* Send error response */
    melo_request_send_response (mlist->req,
        melo_file_browser_message_error (403, "Cannot access location"));

    /* Release request */
    melo_request_complete (mlist->req);
    g_object_unref (volume);
    g_free (mlist->path);
    g_free (mlist->auth);
    free (mlist);
    return;
  }

  /* Get mount */
  mount = g_volume_get_mount (volume);
  g_object_unref (volume);

  /* Generate final file */
  file = melo_file_browser_get_file_from_mount (mount, mlist->path);
  g_object_unref (mount);
  g_free (mlist->path);

  /* Get list */
  melo_file_browser_get_file_list (file, mlist);
}

static int
media_item_cmp (const void *a, const void *b)
{
  Browser__Response__MediaItem *ma = *(Browser__Response__MediaItem **) a;
  Browser__Response__MediaItem *mb = *(Browser__Response__MediaItem **) b;
  return strcasecmp (ma->name, mb->name);
}

static bool
melo_file_browser_get_root_list (MeloRequest *req)
{
  static Browser__Action action = {
      .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
      .type = BROWSER__ACTION__TYPE__DELETE,
      .name = "Eject",
      .icon = "fa:eject",
  };
  static Browser__Action *actions_ptr[] = {
      &action,
  };
  static uint32_t action_ids[] = {0};
  Browser__Response resp = BROWSER__RESPONSE__INIT;
  Browser__Response__MediaList media_list = BROWSER__RESPONSE__MEDIA_LIST__INIT;
  Browser__Response__MediaItem **media_items;
  Browser__Response__MediaItem *items;
  MeloFileBrowser *browser;
  Tags__Tags *tags;
  MeloMessage *msg;
  bool en_network;
  unsigned int count = 2, i;

  /* Get network settings */
  browser = MELO_FILE_BROWSER (melo_request_get_object (req));
  if (!browser ||
      !melo_settings_entry_get_boolean (browser->en_network, &en_network, NULL))
    en_network = true;

  /* Get volumes and mounts */
  if (browser)
    count += g_hash_table_size (browser->mounts);

  /* Allocate lists */
  media_items = malloc (sizeof (*media_items) * count);
  items = malloc (sizeof (*items) * count);
  tags = malloc (sizeof (*tags) * count);
  if (!media_items || !items || !tags) {
    free (media_items);
    free (items);
    free (tags);
    return false;
  }

  /* Prepare response */
  resp.resp_case = BROWSER__RESPONSE__RESP_MEDIA_LIST;
  resp.media_list = &media_list;
  media_list.items = media_items;
  media_list.n_items = 1;
  media_list.count = 1;
  media_list.offset = 0;

  /* Prepare media items */
  for (i = 0; i < count; i++) {
    browser__response__media_item__init (&items[i]);
    tags__tags__init (&tags[i]);
    items[i].tags = &tags[i];
    media_list.items[i] = &items[i];
  }

  /* Add local */
  items[0].id = "local";
  items[0].name = "Local";
  items[0].type = BROWSER__RESPONSE__MEDIA_ITEM__TYPE__FOLDER;
  tags[0].cover = "fa:folder-open";

  /* Add network */
  if (en_network) {
    items[1].id = "network";
    items[1].name = "Network";
    items[1].type = BROWSER__RESPONSE__MEDIA_ITEM__TYPE__FOLDER;
    tags[1].cover = "fa:network-wired";
    media_list.n_items++;
    media_list.count++;
  }

  /* Set actions */
  media_list.actions = actions_ptr;
  media_list.n_actions = 1;

  /* Add volumes and mounts */
  if (browser) {
    GHashTableIter iter;

    /* Initialize iterator */
    g_hash_table_iter_init (&iter, browser->mounts);
    for (i = media_list.count; i < count; i++) {
      MeloFileBrowserMount *bm;

      /* Get next mount / volume */
      if (!g_hash_table_iter_next (&iter, NULL, (gpointer *) &bm))
        break;

      /* Set item */
      items[i].id = bm->id;
      items[i].name = bm->name;
      items[i].type = BROWSER__RESPONSE__MEDIA_ITEM__TYPE__FOLDER;
      tags[i].cover = "fa:hdd";

      /* Set eject action */
      if ((bm->volume && g_volume_can_eject (bm->volume)) ||
          (bm->mount && (g_mount_can_unmount (bm->mount) ||
                            g_mount_can_eject (bm->mount)))) {
        items[i].action_ids = action_ids;
        items[i].n_action_ids = 1;
      }

      /* Move to next volume */
      media_list.n_items++;
      media_list.count++;
    }

    /* Sort volumes / mounts */
    count = media_list.count - (en_network ? 2 : 1);
    qsort (&media_list.items[media_list.count - count], count,
        sizeof (*media_list.items), media_item_cmp);
  }

  /* Generate message */
  msg = melo_message_new (browser__response__get_packed_size (&resp));
  melo_message_set_size (
      msg, browser__response__pack (&resp, melo_message_get_data (msg)));

  /* Free buffers */
  free (media_items);
  free (items);
  free (tags);

  /* Send message and release request */
  melo_request_send_response (req, msg);
  melo_request_complete (req);

  return true;
}

static bool
melo_file_browser_get_uri (MeloFileBrowser *browser, const char *path,
    GFile **file, GVolume **vol, GMount **mount)
{
  const char *prefix = "";
  char *link = NULL;
  char *uri;

  /* Invalid path */
  if (!path || *path++ != '/')
    return false;

  /* Root path */
  if (*path == '\0') {
    if (file)
      *file = NULL;
    return true;
  }

  /* Parse firsut fragment */
  if (!strncmp (path, "local", 5)) {
    prefix = browser->root_path;
    path += 5;
  } else if (!strncmp (path, "network", 7)) {
    prefix = "network://";
    path += 7;
  } else {
    MeloFileBrowserMount *bm;
    unsigned long long id;

    /* Extract ID from path */
    id = strtoull (path, NULL, 16);

    /* Find our mount */
    bm = g_hash_table_lookup (browser->mounts, (gpointer) id);
    if (bm) {
      /* No mount set */
      if (!bm->mount) {
        /* No volume found */
        if (!bm->volume || !vol)
          return false;

        /* Return volume */
        *vol = g_object_ref (bm->volume);
        return true;
      }

      /* Create final file */
      if (!file) {
        if (vol && bm->volume)
          *vol = g_object_ref (bm->volume);
        if (mount && bm->mount)
          *mount = g_object_ref (bm->mount);
      } else
        *file = melo_file_browser_get_file_from_mount (bm->mount, path);
      return true;
    }

    return false;
  }

  /* Invalid fragment */
  if (*path != '\0' && *path != '/')
    return false;

  /* Find last shortcut */
  link = strrchr (path, '#');
  if (link) {
    path = strchr (++link, '/');
    link = g_uri_unescape_segment (link, path, NULL);
    prefix = link;
  }

  /* Generate final URI */
  uri = g_strconcat (prefix, path, NULL);
  g_free (link);

  /* Generate final file */
  if (file)
    *file = g_file_new_for_uri (uri);
  g_free (uri);

  return true;
}

static bool
melo_file_browser_get_media_list (MeloFileBrowser *browser,
    Browser__Request__GetMediaList *r, MeloRequest *req)
{
  GVolume *volume = NULL;
  GFile *file = NULL;
  bool ret;

  /* Generate URI from query */
  if (!melo_file_browser_get_uri (browser, r->query, &file, &volume, NULL))
    return false;

  /* Get media list */
  if (volume || file) {
    MeloFileBrowserMediaList *mlist;

    /* Create asynchronous object */
    mlist = calloc (1, sizeof (*mlist));
    if (r->auth != protobuf_c_empty_string)
      mlist->auth = g_strdup (r->auth);
    mlist->req = req;
    mlist->count = r->count;
    mlist->offset = r->offset;

    /* Create and connect a cancellable for GIO */
    mlist->cancel = g_cancellable_new ();
    g_signal_connect (
        req, "cancelled", G_CALLBACK (request_cancelled_cb), mlist);
    g_signal_connect (
        req, "destroyed", G_CALLBACK (request_destroyed_cb), mlist->cancel);

    /* Volume is not yet mounted */
    if (volume) {
      mlist->path = g_strdup (r->query + 1);
      g_volume_mount (volume, 0, NULL, mlist->cancel, mount_finished_cb, mlist);
      return true;
    }

    /* List with GIO */
    ret = melo_file_browser_get_file_list (file, mlist);
  } else
    ret = melo_file_browser_get_root_list (req);

  return ret;
}

static void
action_cancelled_cb (MeloRequest *req, void *user_data)
{
  MeloFileBrowserAction *action = user_data;

  /* Stop discoverer */
  if (action->disco)
    gst_discoverer_stop (action->disco);
  action->disco_count = 0;

  /* Cancel GIO */
  g_cancellable_cancel (action->cancel);
}

static bool
action_library_cb (const MeloLibraryData *data, MeloTags *tags, void *user_data)
{
  MeloFileBrowserActLib *lib = user_data;

  /* Save tags */
  lib->tags = melo_tags_ref (tags);
  lib->id = data->media_id;

  return true;
}

static void
action_next_files_cb (
    GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GFileEnumerator *en = G_FILE_ENUMERATOR (source_object);
  MeloFileBrowserAction *action = user_data;
  GList *list;

  /* Get next files list */
  list = g_file_enumerator_next_files_finish (en, res, NULL);

  /* Parse list */
  if (list == NULL) {
    uint64_t player_id, path_id;
    MeloPlaylistEntry *entry;
    MeloFileBrowserActLib lib;
    char *name, *uri, *path;

    /* Get directory URI */
    uri = g_file_get_uri (g_file_enumerator_get_container (en));

    /* Get player and path ID */
    path = g_uri_unescape_string (uri, NULL);
    player_id = melo_library_get_player_id (MELO_FILE_PLAYER_ID);
    path_id = melo_library_get_path_id (path);

    /* Create new playlist entry */
    name = strrchr (path, '/');
    if (name)
      name++;
    entry = melo_playlist_entry_new (NULL, NULL, name, NULL);
    g_free (path);

    /* Sort files by name */
    action->list = g_list_sort (action->list, (GCompareFunc) ginfo_cmp);

    /* Add medias to playlist entry */
    while (action->list) {
      GFileInfo *info = G_FILE_INFO (action->list->data);
      char *path;

      /* Build media path */
      path = g_build_path ("/", uri, g_file_info_get_name (info), NULL);

      /* Find media in library */
      lib.tags = NULL;
      melo_library_find (MELO_LIBRARY_TYPE_MEDIA, action_library_cb, &lib,
          MELO_LIBRARY_SELECT (TITLE) | MELO_LIBRARY_SELECT (ARTIST) |
              MELO_LIBRARY_SELECT (ALBUM) | MELO_LIBRARY_SELECT (GENRE) |
              MELO_LIBRARY_SELECT (TRACK) | MELO_LIBRARY_SELECT (COVER),
          1, 0, MELO_LIBRARY_FIELD_NONE, false, false,
          MELO_LIBRARY_FIELD_PLAYER_ID, player_id, MELO_LIBRARY_FIELD_PATH_ID,
          path_id, MELO_LIBRARY_FIELD_MEDIA, g_file_info_get_name (info),
          MELO_LIBRARY_FIELD_LAST);

      /* Add file to playlist entry */
      melo_playlist_entry_add_media (entry, MELO_FILE_PLAYER_ID, path,
          g_file_info_get_display_name (info), lib.tags, NULL);
      g_free (path);

      /* Remove entry */
      action->list = g_list_delete_link (action->list, action->list);
      g_object_unref (info);
    }

    /* Do action */
    if (action->type == BROWSER__ACTION__TYPE__PLAY)
      melo_playlist_play_entry (entry);
    else if (action->type == BROWSER__ACTION__TYPE__ADD)
      melo_playlist_add_entry (entry);

    /* Release request, source object and asynchronous object */
    melo_request_complete (action->req);
    g_object_unref (en);
    free (action);
    g_free (uri);
  } else {
    MeloFileBrowser *browser;

    /* Get browser */
    browser = MELO_FILE_BROWSER (melo_request_get_object (action->req));

    /* Extract files and directories */
    while (list) {
      GList *l = list;
      GFileInfo *info = G_FILE_INFO (l->data);
      const char *name;

      /* Remove file info from list */
      list = g_list_remove_link (list, l);

      /* Skip all entries except regular files */
      name = g_file_info_get_name (info);
      if (!name || *name == '.' ||
          g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR ||
          !melo_file_browser_filter (browser, name)) {
        /* Free entry */
        g_object_unref (info);
        g_list_free (l);
        continue;
      }

      /* Save regular file */
      action->list = g_list_concat (l, action->list);
    }

    /* Enumerate next ITEMS_PER_CALLBACK files */
    g_file_enumerator_next_files_async (en, ITEMS_PER_CALLBACK,
        G_PRIORITY_DEFAULT, action->cancel, action_next_files_cb, action);
  }
}

static void action_children_cb (
    GObject *source_object, GAsyncResult *res, gpointer user_data);

static bool
action_scan_library_cb (
    const MeloLibraryData *data, MeloTags *tags, void *user_data)
{
  MeloFileBrowserActLib *lib = user_data;

  /* Save timestamp */
  lib->timestamp = data->timestamp;

  return true;
}

static void
action_scan_discovered_cb (GstDiscoverer *discoverer, GstDiscovererInfo *info,
    const GError *err, gpointer user_data)
{
  MeloFileBrowserAction *action = user_data;
  struct timespec tp;
  const char *uri;
  char *path, *id;
  MeloTags *tags;

  /* Get media tags */
  tags = melo_tags_new_from_taglist (melo_request_get_object (action->req),
      gst_discoverer_info_get_tags (info));

  /* Get timestamp for now */
  clock_gettime (CLOCK_REALTIME, &tp);

  uri = gst_discoverer_info_get_uri (info);
  path = g_path_get_dirname (uri);
  id = g_path_get_basename (uri);

  /* Update media to library */
  melo_library_add_media (NULL, action->player_id, path, 0, id, 0,
      MELO_LIBRARY_SELECT (TIMESTAMP) | MELO_LIBRARY_SELECT (TITLE) |
          MELO_LIBRARY_SELECT (ARTIST) | MELO_LIBRARY_SELECT (ALBUM) |
          MELO_LIBRARY_SELECT (GENRE) | MELO_LIBRARY_SELECT (TRACK) |
          MELO_LIBRARY_SELECT (COVER),
      NULL, tags, tp.tv_sec, MELO_LIBRARY_FLAG_NONE);

  /* Free ID and tags */
  g_free (path);
  g_free (id);
  melo_tags_unref (tags);

  /* Finished discovery */
  action->disco_count--;
  if (!action->ref_count && !action->disco_count) {
    melo_request_complete (action->req);
    g_object_unref (discoverer);
    free (action);
  }
}

static void
action_scan_files_cb (
    GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GFileEnumerator *en = G_FILE_ENUMERATOR (source_object);
  MeloFileBrowserAction *action = user_data;
  GList *list;

  /* Get next files list */
  list = g_file_enumerator_next_files_finish (en, res, NULL);

  /* Parse list */
  if (list == NULL) {
    /* Finished enumeration */
    action->ref_count--;
    if (!action->ref_count && !action->disco_count) {
      melo_request_complete (action->req);
      g_object_unref (action->disco);
      free (action);
    }
    g_object_unref (en);
  } else {
    MeloFileBrowser *browser;
    bool en_tags = false;
    uint64_t path_id;
    char *uri, *path;

    /* Get browser */
    browser = MELO_FILE_BROWSER (melo_request_get_object (action->req));

    /* Get tags settings */
    if (!browser ||
        !melo_settings_entry_get_boolean (browser->en_tags, &en_tags, NULL))
      en_tags = true;

    /* Get directory URI */
    uri = g_file_get_uri (g_file_enumerator_get_container (en));

    /* Get player and path ID */
    path = g_uri_unescape_string (uri, NULL);
    action->player_id = melo_library_get_player_id (MELO_FILE_PLAYER_ID);
    path_id = melo_library_get_path_id (path);
    g_free (uri);

    /* Extract files and directories */
    while (list) {
      GList *l = list;
      GFileInfo *info = G_FILE_INFO (l->data);
      GFileType type = g_file_info_get_file_type (info);
      const char *name;

      /* Remove file info from list */
      list = g_list_remove_link (list, l);

      /* Filter entries on name and extension */
      name = g_file_info_get_name (info);
      if (name && *name != '.') {
        /* Process entry */
        if (type == G_FILE_TYPE_DIRECTORY) {
          GFile *file;

          /* Get child */
          file = g_file_enumerator_get_child (en, info);

          /* Scan sub-directory */
          action->ref_count++;
          g_file_enumerate_children_async (file, MELO_FILE_BROWSER_ATTRIBUTES,
              0, G_PRIORITY_DEFAULT, action->cancel, action_children_cb,
              action);
        } else if (type == G_FILE_TYPE_REGULAR &&
                   melo_file_browser_filter (browser, name)) {
          MeloFileBrowserActLib lib;
          uint64_t timestamp;

          /* Get last modified timestamp */
          timestamp = g_file_info_get_attribute_uint64 (
              info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

          /* Find media in library */
          lib.timestamp = 0;
          melo_library_find (MELO_LIBRARY_TYPE_MEDIA, action_scan_library_cb,
              &lib, MELO_LIBRARY_SELECT (TIMESTAMP), 1, 0,
              MELO_LIBRARY_FIELD_NONE, false, false,
              MELO_LIBRARY_FIELD_PLAYER_ID, action->player_id,
              MELO_LIBRARY_FIELD_PATH_ID, path_id, MELO_LIBRARY_FIELD_MEDIA,
              name, MELO_LIBRARY_FIELD_LAST);

          /* Update library and tags */
          if (lib.timestamp <= timestamp) {
            /* Add media to library */
            melo_library_add_media (NULL, action->player_id, NULL, path_id,
                name, 0,
                MELO_LIBRARY_SELECT (NAME) | MELO_LIBRARY_SELECT (TIMESTAMP),
                g_file_info_get_display_name (info), NULL, timestamp,
                MELO_LIBRARY_FLAG_NONE);

            /* Add media to discoverer queue */
            if (en_tags) {
              action->disco_count++;
              uri = g_build_filename (path, name, NULL);
              gst_discoverer_discover_uri_async (action->disco, uri);
              g_free (uri);
            }
          }
        }
      }

      /* Free entry */
      g_object_unref (info);
      g_list_free (l);
    }

    /* Free path */
    g_free (path);

    /* Enumerate next ITEMS_PER_CALLBACK files */
    g_file_enumerator_next_files_async (en, ITEMS_PER_CALLBACK,
        G_PRIORITY_DEFAULT, action->cancel, action_scan_files_cb, action);
  }
}

static void
action_children_cb (
    GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  MeloFileBrowserAction *action = user_data;
  GFile *file = G_FILE (source_object);
  GFileEnumerator *en;

  /* Enumeration started */
  en = g_file_enumerate_children_finish (file, res, NULL);
  if (!en) {
    char *uri, *path, *media;
    MeloFileBrowserActLib lib;

    /* Handle scan */
    if (action->type == BROWSER__ACTION__TYPE__SCAN) {
      action->ref_count--;
      if (!action->ref_count && !action->disco_count) {
        melo_request_complete (action->req);
        g_object_unref (action->disco);
        free (action);
      }
      g_object_unref (file);
      return;
    }

    /* Get file URI */
    uri = g_file_get_uri (file);

    /* Get path and media from URI */
    path = g_uri_unescape_string (uri, NULL);
    media = strrchr (path, '/');
    if (media)
      *media++ = '\0';

    /* Find media in library */
    lib.id = 0;
    lib.tags = NULL;
    melo_library_find (MELO_LIBRARY_TYPE_MEDIA, action_library_cb, &lib,
        MELO_LIBRARY_SELECT (MEDIA_ID) | MELO_LIBRARY_SELECT (TITLE) |
            MELO_LIBRARY_SELECT (ARTIST) | MELO_LIBRARY_SELECT (ALBUM) |
            MELO_LIBRARY_SELECT (GENRE) | MELO_LIBRARY_SELECT (TRACK) |
            MELO_LIBRARY_SELECT (COVER),
        1, 0, MELO_LIBRARY_FIELD_NONE, false, false, MELO_LIBRARY_FIELD_PLAYER,
        MELO_FILE_PLAYER_ID, MELO_LIBRARY_FIELD_PATH, path,
        MELO_LIBRARY_FIELD_MEDIA, media, MELO_LIBRARY_FIELD_LAST);
    g_free (path);

    /* Do action as regular file */
    if (action->type == BROWSER__ACTION__TYPE__PLAY)
      melo_playlist_play_media (MELO_FILE_PLAYER_ID, uri, NULL, lib.tags);
    else if (action->type == BROWSER__ACTION__TYPE__ADD)
      melo_playlist_add_media (MELO_FILE_PLAYER_ID, uri, NULL, lib.tags);
    else {
      if (action->type == BROWSER__ACTION__TYPE__SET_FAVORITE && lib.id)
        melo_library_update_media_flags (
            lib.id, MELO_LIBRARY_FLAG_FAVORITE, false);
      else if (action->type == BROWSER__ACTION__TYPE__UNSET_FAVORITE && lib.id)
        melo_library_update_media_flags (
            lib.id, MELO_LIBRARY_FLAG_FAVORITE, true);
      melo_tags_unref (lib.tags);
    }

    /* Release request, source object and asynchronous object */
    melo_request_complete (action->req);
    g_object_unref (file);
    free (action);
    g_free (uri);
    return;
  }

  /* Release file */
  g_object_unref (file);

  /* Handle scan */
  if (action->type == BROWSER__ACTION__TYPE__SCAN) {
    /* Create discoverer */
    if (!action->disco) {
      action->player_id = melo_library_get_player_id (MELO_FILE_PLAYER_ID);
      action->disco = gst_discoverer_new (GST_SECOND * 10, NULL);
      g_signal_connect (action->disco, "discovered",
          G_CALLBACK (action_scan_discovered_cb), action);
      gst_discoverer_start (action->disco);
    }

    /* Get children */
    g_file_enumerator_next_files_async (en, ITEMS_PER_CALLBACK,
        G_PRIORITY_DEFAULT, action->cancel, action_scan_files_cb, action);
    return;
  }

  /* Enumerate ITEMS_PER_CALLBACK first files */
  g_file_enumerator_next_files_async (en, ITEMS_PER_CALLBACK,
      G_PRIORITY_DEFAULT, action->cancel, action_next_files_cb, action);
}

static void
volume_eject_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GVolume *volume = G_VOLUME (source_object);
  MeloRequest *req = user_data;
  GError *error = NULL;

  if (!g_volume_eject_with_operation_finish (volume, res, &error)) {
    MELO_LOGE ("failed to eject a volume: %s", error->message);
    melo_request_send_response (
        req, melo_file_browser_message_error (403, "Failed to eject device"));
    g_error_free (error);
  }
  melo_request_complete (req);
  g_object_unref (volume);
}

static void
mount_eject_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GMount *mount = G_MOUNT (source_object);
  MeloRequest *req = user_data;
  GError *error = NULL;

  if (!g_mount_eject_with_operation_finish (mount, res, &error)) {
    MELO_LOGE ("failed to eject a mount: %s", error->message);
    melo_request_send_response (
        req, melo_file_browser_message_error (403, "Failed to eject"));
    g_error_free (error);
  }
  melo_request_complete (req);
  g_object_unref (mount);
}

static void
mount_unmount_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GMount *mount = G_MOUNT (source_object);
  MeloRequest *req = user_data;
  GError *error = NULL;

  if (!g_mount_unmount_with_operation_finish (mount, res, &error)) {
    MELO_LOGE ("failed to unmount a mount: %s", error->message);
    melo_request_send_response (
        req, melo_file_browser_message_error (403, "Failed to unmount"));
    g_error_free (error);
  }
  melo_request_complete (req);
  g_object_unref (mount);
}

static bool
melo_file_browser_do_action (
    MeloFileBrowser *browser, Browser__Request__DoAction *r, MeloRequest *req)
{
  MeloFileBrowserAction *action;
  GFile *file;

  /* Do action */
  switch (r->type) {
  case BROWSER__ACTION__TYPE__PLAY:
  case BROWSER__ACTION__TYPE__ADD:
  case BROWSER__ACTION__TYPE__SET_FAVORITE:
  case BROWSER__ACTION__TYPE__UNSET_FAVORITE:
  case BROWSER__ACTION__TYPE__SCAN:
  case BROWSER__ACTION__TYPE__DELETE:
    break;
  default:
    MELO_LOGE ("action %u not supported", r->type);
    return false;
  }

  /* Handle ejection */
  if (r->type == BROWSER__ACTION__TYPE__DELETE) {
    GVolume *volume = NULL;
    GMount *mount = NULL;

    /* Get volume and mount */
    if (!melo_file_browser_get_uri (browser, r->path, NULL, &volume, &mount))
      return false;

    /* Ejection first, unmount otherwise */
    if (volume && g_volume_can_eject (volume)) {
      g_volume_eject_with_operation (
          volume, G_MOUNT_UNMOUNT_NONE, NULL, NULL, volume_eject_cb, req);
      volume = NULL;
    } else if (mount && g_mount_can_eject (mount)) {
      g_mount_eject_with_operation (
          mount, G_MOUNT_UNMOUNT_NONE, NULL, NULL, mount_eject_cb, req);
      mount = NULL;
    } else if (mount && g_mount_can_unmount (mount)) {
      g_mount_unmount_with_operation (
          mount, G_MOUNT_UNMOUNT_NONE, NULL, NULL, mount_unmount_cb, req);
      mount = NULL;
    } else
      melo_request_complete (req);

    /* Release objects */
    g_clear_object (&mount);
    g_clear_object (&volume);

    return true;
  }

  /* Generate URI from path */
  if (!melo_file_browser_get_uri (browser, r->path, &file, NULL, NULL) || !file)
    return false;

  /* Create asynchronous object */
  action = calloc (1, sizeof (*action));
  if (!file) {
    g_object_unref (file);
    return false;
  }
  action->type = r->type;
  action->req = req;

  /* Create and connect a cancellable for GIO */
  action->cancel = g_cancellable_new ();
  g_signal_connect (req, "cancelled", G_CALLBACK (action_cancelled_cb), action);
  g_signal_connect (
      req, "destroyed", G_CALLBACK (request_destroyed_cb), action->cancel);

  /* Start file enumeration */
  action->ref_count++;
  g_file_enumerate_children_async (file, MELO_FILE_BROWSER_ATTRIBUTES, 0,
      G_PRIORITY_DEFAULT, action->cancel, action_children_cb, action);

  return true;
}

static bool
melo_file_browser_handle_request (
    MeloBrowser *browser, const MeloMessage *msg, MeloRequest *req)
{
  MeloFileBrowser *fbrowser = MELO_FILE_BROWSER (browser);
  Browser__Request *r;
  bool ret = false;

  /* Unpack request */
  r = browser__request__unpack (
      NULL, melo_message_get_size (msg), melo_message_get_cdata (msg, NULL));
  if (!r) {
    MELO_LOGE ("failed to unpack request");
    return false;
  }

  /* Handle request */
  switch (r->req_case) {
  case BROWSER__REQUEST__REQ_GET_MEDIA_LIST:
    ret = melo_file_browser_get_media_list (fbrowser, r->get_media_list, req);
    break;
  case BROWSER__REQUEST__REQ_DO_ACTION:
    ret = melo_file_browser_do_action (fbrowser, r->do_action, req);
    break;
  default:
    MELO_LOGE ("request %u not supported", r->req_case);
  }

  /* Free request */
  browser__request__free_unpacked (r, NULL);

  return ret;
}

static char *
melo_file_browser_get_asset (MeloBrowser *browser, const char *id)
{
  return melo_cover_cache_get_path (id);
}
