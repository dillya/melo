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
  MeloTags *tags;
} MeloFileBrowserLib;

typedef struct {
  MeloRequest *req;
  GCancellable *cancel;

  char *auth;

  GList *dirs;
  GList *files;
  unsigned int total;

  unsigned int count;
  unsigned int offset;

  GstDiscoverer *disco;

  uint64_t player_id;
  uint64_t path_id;
} MeloFileBrowserMediaList;

typedef struct {
  MeloRequest *req;
  GCancellable *cancel;

  Browser__Action__Type type;
  char *uri;

  GList *list;
} MeloFileBrowserAction;

struct _MeloFileBrowser {
  GObject parent_instance;

  char *root_path;

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

static void
melo_file_browser_finalize (GObject *object)
{
  MeloFileBrowser *browser = MELO_FILE_BROWSER (object);

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
  if (!melo_settings_entry_get_string (browser->filter, &exts, NULL))
    return true;

  /* Get extension */
  ext = strrchr (name, '.');
  if (!ext)
    return false;
  len = strlen (++ext);

  /* Check extension */
  while (*exts != '\0') {
    if (*exts == ext[n]) {
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
  if (!melo_request_send_response (mlist->req, msg)) {
    /* Stop and release discoverer */
    gst_discoverer_stop (disco);
    g_object_unref (disco);

    /* Release request */
    melo_request_unref (mlist->req);
    free (mlist);
  }
}

static void
discover_finished_cb (GstDiscoverer *disco, gpointer user_data)
{
  MeloFileBrowserMediaList *mlist = user_data;

  /* Release discoverer and request */
  g_object_unref (disco);
  melo_request_unref (mlist->req);
  free (mlist);
}

static bool
library_cb (const MeloLibraryData *data, MeloTags *tags, void *user_data)
{
  MeloFileBrowserLib *lib = user_data;
  Tags__Tags *item_tags;

  /* Save timestamp */
  lib->timestamp = data->timestamp;

  /* Generate tags */
  if (tags) {
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
    static Browser__Action folder_actions[3] = {
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
            .type = BROWSER__ACTION__TYPE__CUSTOM,
            .custom_id = "scan",
            .name = "Scan for medias",
            .icon = "fa:search",
        },
    };
    static Browser__Action file_actions[2] = {
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
    };
    static Browser__Action *folder_actions_ptr[3] = {
        &folder_actions[0],
        &folder_actions[1],
        &folder_actions[2],
    };
    static Browser__Action *file_actions_ptr[2] = {
        &file_actions[0],
        &file_actions[1],
    };
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

    /* Set media list actions */
    media_list.n_actions = G_N_ELEMENTS (folder_actions_ptr);
    media_list.actions = folder_actions_ptr;

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
      items[i].n_actions = G_N_ELEMENTS (folder_actions_ptr);
      items[i].actions = folder_actions_ptr;

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
      items[i].n_actions = G_N_ELEMENTS (file_actions_ptr);
      items[i].actions = file_actions_ptr;

      /* Create discoverer */
      if (en_tags && !mlist->disco) {
        mlist->disco = gst_discoverer_new (GST_SECOND * 10, NULL);
        g_signal_connect (mlist->disco, "discovered",
            G_CALLBACK (discover_discovered_cb), mlist);
        g_signal_connect (
            mlist->disco, "finished", G_CALLBACK (discover_finished_cb), mlist);
        gst_discoverer_start (mlist->disco);
      }

      /* Get last modified timestamp */
      timestamp = g_file_info_get_attribute_uint64 (
          info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

      /* Find media in library */
      lib.timestamp = 0;
      lib.item = &items[i];
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
          gst_discoverer_discover_uri_async (mlist->disco, uri);
          g_free (uri);
        }
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
    free (mlist->auth);
    if (!mlist->disco) {
      melo_request_unref (mlist->req);
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

static void
mount_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  MeloFileBrowserMediaList *mlist = user_data;
  GFile *file = G_FILE (source_object);

  /* Finalize mount operation */
  if (!g_file_mount_enclosing_volume_finish (file, res, NULL)) {
    Browser__Response resp = BROWSER__RESPONSE__INIT;
    Browser__Response__Error error = BROWSER__RESPONSE__ERROR__INIT;
    MeloMessage *msg;

    /* Set error */
    error.code = 401;
    error.message = "Unauthorized";
    resp.resp_case = BROWSER__RESPONSE__RESP_ERROR;
    resp.error = &error;

    /* Pack message */
    msg = melo_message_new (browser__response__get_packed_size (&resp));
    melo_message_set_size (
        msg, browser__response__pack (&resp, melo_message_get_data (msg)));

    /* Send response */
    melo_request_send_response (mlist->req, msg);

    /* Release request, source object and asynchronous object */
    melo_request_unref (mlist->req);
    g_object_unref (file);
    free (mlist->auth);
    free (mlist);
    return;
  }

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
      GMountOperation *op;

      /* Create mount operation */
      op = g_mount_operation_new ();

      /* Add signal for authentication */
      g_signal_connect (
          op, "ask_password", G_CALLBACK (ask_password_cb), mlist);

      /* Mount */
      g_file_mount_enclosing_volume (
          file, 0, op, mlist->cancel, mount_cb, mlist);

      g_error_free (err);
      return;
    }

    /* Release request, source object and asynchronous object */
    melo_request_unref (mlist->req);
    g_object_unref (file);
    free (mlist->auth);
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
  g_cancellable_cancel ((GCancellable *) user_data);
}

static void
request_destroyed_cb (MeloRequest *req, void *user_data)
{
  g_object_unref ((GCancellable *) user_data);
}

static bool
melo_file_browser_get_file_list (
    GFile *file, Browser__Request__GetMediaList *r, MeloRequest *req)
{
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
      req, "cancelled", G_CALLBACK (request_cancelled_cb), mlist->cancel);
  g_signal_connect (
      req, "destroyed", G_CALLBACK (request_destroyed_cb), mlist->cancel);

  /* Start file enumeration */
  g_file_enumerate_children_async (file, MELO_FILE_BROWSER_ATTRIBUTES, 0,
      G_PRIORITY_DEFAULT, mlist->cancel, children_cb, mlist);

  return true;
}

static bool
melo_file_browser_get_root_list (MeloRequest *req)
{
  Browser__Response resp = BROWSER__RESPONSE__INIT;
  Browser__Response__MediaList media_list = BROWSER__RESPONSE__MEDIA_LIST__INIT;
  Browser__Response__MediaItem *media_items[2];
  Browser__Response__MediaItem items[2];
  MeloFileBrowser *browser;
  Tags__Tags tags[2];
  MeloMessage *msg;
  bool en_network;
  unsigned int i;

  /* Get network settings */
  browser = MELO_FILE_BROWSER (melo_request_get_object (req));
  if (!browser ||
      !melo_settings_entry_get_boolean (browser->en_network, &en_network, NULL))
    en_network = true;

  /* Prepare response */
  resp.resp_case = BROWSER__RESPONSE__RESP_MEDIA_LIST;
  resp.media_list = &media_list;
  media_list.items = media_items;
  media_list.n_items = 1;
  media_list.count = 1;
  media_list.offset = 0;

  /* Prepare media items */
  for (i = 0; i < 2; i++) {
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

  /* Generate message */
  msg = melo_message_new (browser__response__get_packed_size (&resp));
  melo_message_set_size (
      msg, browser__response__pack (&resp, melo_message_get_data (msg)));

  /* Send message and release request */
  melo_request_send_response (req, msg);
  melo_request_unref (req);

  return true;
}

static bool
melo_file_browser_get_uri (
    MeloFileBrowser *browser, const char *path, char **uri)
{
  const char *prefix = "";
  char *link = NULL;

  /* Invalid path */
  if (!path || *path++ != '/')
    return false;

  /* Root path */
  if (*path == '\0') {
    uri = NULL;
    return true;
  }

  /* Parse first fragment */
  if (!strncmp (path, "local", 5)) {
    prefix = browser->root_path;
    path += 5;
  } else if (!strncmp (path, "network", 7)) {
    prefix = "network://";
    path += 7;
  } else
    return false;

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
  *uri = g_strconcat (prefix, path, NULL);
  g_free (link);

  return true;
}

static bool
melo_file_browser_get_media_list (MeloFileBrowser *browser,
    Browser__Request__GetMediaList *r, MeloRequest *req)
{
  char *uri = NULL;
  bool ret;

  /* Generate URI from query */
  if (!melo_file_browser_get_uri (browser, r->query, &uri))
    return false;

  /* Get media list */
  if (uri) {
    GFile *file;

    /* Create file */
    file = g_file_new_for_uri (uri);
    g_free (uri);

    /* List with GIO */
    ret = melo_file_browser_get_file_list (file, r, req);
  } else
    ret = melo_file_browser_get_root_list (req);

  return ret;
}

static bool
action_library_cb (const MeloLibraryData *data, MeloTags *tags, void *user_data)
{
  MeloFileBrowserLib *lib = user_data;

  /* Save tags */
  lib->tags = melo_tags_ref (tags);

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
    MeloFileBrowserLib lib;
    char *name;

    /* Get player and path ID */
    player_id = melo_library_get_player_id (MELO_FILE_PLAYER_ID);
    path_id = melo_library_get_path_id (action->uri);

    /* Create new playlist entry */
    name = g_path_get_basename (action->uri);
    entry = melo_playlist_entry_new (MELO_FILE_PLAYER_ID, NULL, name, NULL);
    g_free (name);

    /* Sort files by name */
    action->list = g_list_sort (action->list, (GCompareFunc) ginfo_cmp);

    /* Add medias to playlist entry */
    while (action->list) {
      GFileInfo *info = G_FILE_INFO (action->list->data);
      char *path;

      /* Build media path */
      path = g_build_path ("/", action->uri, g_file_info_get_name (info), NULL);

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
      melo_playlist_entry_add_media (
          entry, path, g_file_info_get_display_name (info), lib.tags, NULL);
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
    melo_request_unref (action->req);
    g_free (action->uri);
    g_object_unref (en);
    free (action);
  } else {
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
          g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR) {
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
    MeloFileBrowserLib lib;
    char *path, *media;

    /* Get dirname */
    path = g_path_get_dirname (action->uri);
    media = g_path_get_basename (action->uri);

    /* Find media in library */
    lib.tags = NULL;
    melo_library_find (MELO_LIBRARY_TYPE_MEDIA, action_library_cb, &lib,
        MELO_LIBRARY_SELECT (TITLE) | MELO_LIBRARY_SELECT (ARTIST) |
            MELO_LIBRARY_SELECT (ALBUM) | MELO_LIBRARY_SELECT (GENRE) |
            MELO_LIBRARY_SELECT (TRACK) | MELO_LIBRARY_SELECT (COVER),
        1, 0, MELO_LIBRARY_FIELD_NONE, false, false, MELO_LIBRARY_FIELD_PLAYER,
        MELO_FILE_PLAYER_ID, MELO_LIBRARY_FIELD_PATH, path,
        MELO_LIBRARY_FIELD_MEDIA, media, MELO_LIBRARY_FIELD_LAST);
    g_free (media);
    g_free (path);

    /* Do action as regular file */
    if (action->type == BROWSER__ACTION__TYPE__PLAY)
      melo_playlist_play_media (
          MELO_FILE_PLAYER_ID, action->uri, NULL, lib.tags);
    else if (action->type == BROWSER__ACTION__TYPE__ADD)
      melo_playlist_add_media (
          MELO_FILE_PLAYER_ID, action->uri, NULL, lib.tags);

    /* Release riequest, source object and asynchronous object */
    melo_request_unref (action->req);
    g_object_unref (file);
    g_free (action->uri);
    free (action);
    return;
  }

  /* Release file */
  g_object_unref (file);

  /* Enumerate ITEMS_PER_CALLBACK first files */
  g_file_enumerator_next_files_async (en, ITEMS_PER_CALLBACK,
      G_PRIORITY_DEFAULT, action->cancel, action_next_files_cb, action);
}

static bool
melo_file_browser_do_action (
    MeloFileBrowser *browser, Browser__Request__DoAction *r, MeloRequest *req)
{
  MeloFileBrowserAction *action;
  char *uri = NULL;
  GFile *file;

  /* Generate URI from path */
  if (!melo_file_browser_get_uri (browser, r->path, &uri) || !uri)
    return false;

  /* Do action */
  switch (r->type) {
  case BROWSER__ACTION__TYPE__PLAY:
  case BROWSER__ACTION__TYPE__ADD:
    break;
  default:
    MELO_LOGE ("action %u not supported", r->type);
    g_free (uri);
    return false;
  }

  /* Create file */
  file = g_file_new_for_uri (uri);
  if (!file) {
    g_free (uri);
    return false;
  }

  /* Create asynchronous object */
  action = calloc (1, sizeof (*action));
  if (!file) {
    g_object_unref (file);
    g_free (uri);
    return false;
  }
  action->uri = uri;
  action->type = r->type;
  action->req = req;

  /* Create and connect a cancellable for GIO */
  action->cancel = g_cancellable_new ();
  g_signal_connect (
      req, "cancelled", G_CALLBACK (request_cancelled_cb), action->cancel);
  g_signal_connect (
      req, "destroyed", G_CALLBACK (request_destroyed_cb), action->cancel);

  /* Start file enumeration */
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
