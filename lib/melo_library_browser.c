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

#include "melo/melo_library.h"
#include "melo/melo_playlist.h"

#define MELO_LOG_TAG "library_browser"
#include "melo/melo_log.h"

#include "browser.pb-c.h"

#include "melo_library_browser.h"

#define MELO_LIBRARY_MAX_COUNT 1000

typedef struct {
  Browser__Response__MediaList *media_list;
  Browser__Response__MediaItem *items;
  Tags__Tags *tags;
} MeloLibraryBrowserAsync;

struct _MeloLibraryBrowser {
  GObject parent_instance;
};

MELO_DEFINE_BROWSER (MeloLibraryBrowser, melo_library_browser)

static bool melo_library_browser_handle_request (
    MeloBrowser *browser, const MeloMessage *msg, MeloRequest *req);

static void
melo_library_browser_class_init (MeloLibraryBrowserClass *klass)
{
  MeloBrowserClass *parent_class = MELO_BROWSER_CLASS (klass);

  /* Setup callbacks */
  parent_class->handle_request = melo_library_browser_handle_request;
}

static void
melo_library_browser_init (MeloLibraryBrowser *self)
{
}

MeloLibraryBrowser *
melo_library_browser_new ()
{
  return g_object_new (MELO_TYPE_LIBRARY_BROWSER, "id", MELO_LIBRARY_BROWSER_ID,
      "name", "Library", "description", "Navigate through all your medias",
      "icon", "fa:music", "support-search", true, NULL);
}

static bool
melo_library_browser_get_root (MeloRequest *req)
{
  static const struct {
    const char *id;
    const char *name;
    const char *icon;
  } root[] = {
      {"favorites", "Favorites", "fa:star"},
      {"artists", "Artists", "fa:user"},
      {"albums", "Albums", "fa:compact-disc"},
      {"songs", "Songs", "fa:music"},
      {"genres", "Genres", "fa:guitar"},
  };
  Browser__Response resp = BROWSER__RESPONSE__INIT;
  Browser__Response__MediaList media_list = BROWSER__RESPONSE__MEDIA_LIST__INIT;
  Browser__Response__MediaItem *items_ptr[G_N_ELEMENTS (root)];
  Browser__Response__MediaItem items[G_N_ELEMENTS (root)];
  Tags__Tags tags[G_N_ELEMENTS (root)];
  MeloMessage *msg;
  unsigned int i;

  /* Set response type */
  resp.resp_case = BROWSER__RESPONSE__RESP_MEDIA_LIST;
  resp.media_list = &media_list;

  /* Set item list */
  media_list.n_items = G_N_ELEMENTS (root);
  media_list.items = items_ptr;

  /* Set list count and offset */
  media_list.count = G_N_ELEMENTS (root);
  media_list.offset = 0;

  /* Add media items */
  for (i = 0; i < G_N_ELEMENTS (root); i++) {
    /* Init media item */
    browser__response__media_item__init (&items[i]);
    media_list.items[i] = &items[i];

    /* Set media */
    items[i].id = (char *) root[i].id;
    items[i].name = (char *) root[i].name;
    items[i].type = BROWSER__RESPONSE__MEDIA_ITEM__TYPE__FOLDER;

    /* Set tags */
    tags__tags__init (&tags[i]);
    items[i].tags = &tags[i];
    tags[i].cover = (char *) root[i].icon;
  }

  /* Pack message */
  msg = melo_message_new (browser__response__get_packed_size (&resp));
  melo_message_set_size (
      msg, browser__response__pack (&resp, melo_message_get_data (msg)));

  /* Send media list response */
  melo_request_send_response (req, msg);

  /* Release request */
  melo_request_complete (req);

  return true;
}

static bool
melo_library_parse_query (const char *query, MeloLibraryType *type,
    MeloLibraryField *fields, uint64_t *ids)
{
  const char *frag[4];
  size_t len[4];
  size_t count = 0;

  /* Split query */
  frag[count] = query;
  do {
    query = strchr (query, '/');
    if (!query) {
      len[count] = strlen (frag[count]);
      if (len[count])
        count++;
      break;
    }
    len[count] = query - frag[count];
    frag[++count] = ++query;
  } while (count < 4);

  /* Parse query */
  if (len[0] == 7 && !strncmp (frag[0], "artists", len[0])) {
    if (count == 1) {
      *type = MELO_LIBRARY_TYPE_ARTIST;
      return true;
    } else if (count == 2) {
      *type = MELO_LIBRARY_TYPE_MEDIA;
      fields[0] = MELO_LIBRARY_FIELD_ARTIST_ID;
      ids[0] = strtoull (frag[1], NULL, 10);
      return true;
    } else if (count == 3) {
      *type = MELO_LIBRARY_TYPE_MEDIA;
      fields[0] = MELO_LIBRARY_FIELD_MEDIA_ID;
      ids[0] = strtoull (frag[2], NULL, 10);
      return true;
    }
  } else if (len[0] == 6 && !strncmp (frag[0], "albums", len[0])) {
    if (count == 1) {
      *type = MELO_LIBRARY_TYPE_ALBUM;
      return true;
    } else if (count == 2) {
      *type = MELO_LIBRARY_TYPE_MEDIA;
      fields[0] = MELO_LIBRARY_FIELD_ALBUM_ID;
      ids[0] = strtoull (frag[1], NULL, 10);
      return true;
    } else if (count == 3) {
      *type = MELO_LIBRARY_TYPE_MEDIA;
      fields[0] = MELO_LIBRARY_FIELD_MEDIA_ID;
      ids[0] = strtoull (frag[2], NULL, 10);
      return true;
    }
  } else if (len[0] == 6 && !strncmp (frag[0], "genres", len[0])) {
    if (count == 1) {
      *type = MELO_LIBRARY_TYPE_GENRE;
      return true;
    } else if (count == 2) {
      *type = MELO_LIBRARY_TYPE_MEDIA;
      fields[0] = MELO_LIBRARY_FIELD_GENRE_ID;
      ids[0] = strtoull (frag[1], NULL, 10);
      return true;
    } else if (count == 3) {
      *type = MELO_LIBRARY_TYPE_MEDIA;
      fields[0] = MELO_LIBRARY_FIELD_MEDIA_ID;
      ids[0] = strtoull (frag[2], NULL, 10);
      return true;
    }
  } else if (len[0] == 5 && !strncmp (frag[0], "songs", len[0])) {
    if (count == 1) {
      *type = MELO_LIBRARY_TYPE_MEDIA;
      return true;
    } else if (count == 2) {
      *type = MELO_LIBRARY_TYPE_MEDIA;
      fields[0] = MELO_LIBRARY_FIELD_MEDIA_ID;
      ids[0] = strtoull (frag[1], NULL, 10);
      return true;
    }
  } else if (len[0] == 9 && !strncmp (frag[0], "favorites", len[0])) {
    if (count == 1) {
      *type = MELO_LIBRARY_TYPE_MEDIA;
      fields[0] = MELO_LIBRARY_FIELD_FAVORITE;
      ids[0] = true;
      return true;
    } else if (count == 2) {
      *type = MELO_LIBRARY_TYPE_MEDIA;
      fields[0] = MELO_LIBRARY_FIELD_MEDIA_ID;
      ids[0] = strtoull (frag[1], NULL, 10);
      return true;
    }
  }

  return false;
}

static Browser__Action actions[] = {
    /* Category actions */
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
    /* Media actions */
    {
        .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
        .type = BROWSER__ACTION__TYPE__PLAY,
        .name = "Play media",
        .icon = "fa:play",
    },
    {
        .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
        .type = BROWSER__ACTION__TYPE__ADD,
        .name = "Add media to playlist",
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
};

static uint32_t category_actions[] = {0, 1};
static uint32_t media_set_fav_actions[] = {2, 3, 4};
static uint32_t media_unset_fav_actions[] = {2, 3, 5};

static Browser__SortMenu__Item sort_menu_items[6] = {
    {.base = PROTOBUF_C_MESSAGE_INIT (&browser__sort_menu__item__descriptor),
        .id = "n",
        .name = "Name"},
    {
        .base = PROTOBUF_C_MESSAGE_INIT (&browser__sort_menu__item__descriptor),
        .id = "t",
        .name = "Title",
    },
    {
        .base = PROTOBUF_C_MESSAGE_INIT (&browser__sort_menu__item__descriptor),
        .id = "a",
        .name = "Artistt",
    },
    {
        .base = PROTOBUF_C_MESSAGE_INIT (&browser__sort_menu__item__descriptor),
        .id = "l",
        .name = "Album",
    },
    {
        .base = PROTOBUF_C_MESSAGE_INIT (&browser__sort_menu__item__descriptor),
        .id = "g",
        .name = "Genre",
    },
    {
        .base = PROTOBUF_C_MESSAGE_INIT (&browser__sort_menu__item__descriptor),
        .id = "k",
        .name = "Track",
    },
};
static Browser__SortMenu__Item sort_dir_menu_items[2] = {
    {.base = PROTOBUF_C_MESSAGE_INIT (&browser__sort_menu__item__descriptor),
        .id = "a",
        .name = "Ascending"},
    {
        .base = PROTOBUF_C_MESSAGE_INIT (&browser__sort_menu__item__descriptor),
        .id = "d",
        .name = "Descending",
    },
};

static Browser__SortMenu__Item *sort_dir_menu_items_ptr[2] = {
    &sort_dir_menu_items[0],
    &sort_dir_menu_items[1],
};
static Browser__SortMenu sort_dir_menus = {
    .base = PROTOBUF_C_MESSAGE_INIT (&browser__sort_menu__descriptor),
    .n_items = G_N_ELEMENTS (sort_dir_menu_items_ptr),
    .items = sort_dir_menu_items_ptr,
};

static Browser__SortMenu__Item *sort_category_menu_items_ptr[1] = {
    &sort_menu_items[0],
};
static Browser__SortMenu sort_category_menus = {
    .base = PROTOBUF_C_MESSAGE_INIT (&browser__sort_menu__descriptor),
    .n_items = G_N_ELEMENTS (sort_category_menu_items_ptr),
    .items = sort_category_menu_items_ptr,
};
static Browser__SortMenu *sort_category_menus_ptr[2] = {
    &sort_category_menus,
    &sort_dir_menus,
};

static Browser__SortMenu__Item *sort_media_menu_items_ptr[6] = {
    &sort_menu_items[0],
    &sort_menu_items[1],
    &sort_menu_items[2],
    &sort_menu_items[3],
    &sort_menu_items[4],
    &sort_menu_items[5],
};
static Browser__SortMenu sort_media_menus = {
    .base = PROTOBUF_C_MESSAGE_INIT (&browser__sort_menu__descriptor),
    .n_items = G_N_ELEMENTS (sort_media_menu_items_ptr),
    .items = sort_media_menu_items_ptr,
};
static Browser__SortMenu *sort_media_menus_ptr[2] = {
    &sort_media_menus,
    &sort_dir_menus,
};

static bool
media_cb (const MeloLibraryData *data, MeloTags *tags, void *user_data)
{
  MeloLibraryBrowserAsync *async = user_data;
  Browser__Response__MediaItem *item;

  /* Set media item */
  item = &async->items[async->media_list->n_items];
  browser__response__media_item__init (item);
  item->type = BROWSER__RESPONSE__MEDIA_ITEM__TYPE__MEDIA;
  item->id = g_strdup (data->id);
  if (!data->name)
    item->name = g_strdup ("Unknown");
  else
    item->name = g_strdup (data->name);

  /* Set tags */
  if (tags) {
    Tags__Tags *item_tags;

    /* Allocate a new tags and set it */
    item_tags = malloc (sizeof (*item_tags));
    if (item_tags) {
      tags__tags__init (item_tags);
      item_tags->title = g_strdup (melo_tags_get_title (tags));
      item_tags->artist = g_strdup (melo_tags_get_artist (tags));
      item_tags->album = g_strdup (melo_tags_get_album (tags));
      item_tags->genre = g_strdup (melo_tags_get_genre (tags));
      item_tags->track = melo_tags_get_track (tags);
      item_tags->cover = g_strdup (melo_tags_get_cover (tags));
      item->tags = item_tags;
    }
  }

  /* Add action IDs */
  if (data->flags & MELO_LIBRARY_FLAG_FAVORITE) {
    item->n_action_ids = G_N_ELEMENTS (media_unset_fav_actions);
    item->action_ids = media_unset_fav_actions;
  } else {
    item->n_action_ids = G_N_ELEMENTS (media_set_fav_actions);
    item->action_ids = media_set_fav_actions;
  }
  item->favorite = data->flags & MELO_LIBRARY_FLAG_FAVORITE;

  /* Add item to list */
  async->media_list->items[async->media_list->n_items] = item;
  async->media_list->n_items++;

  return true;
}

static bool
category_cb (const MeloLibraryData *data, MeloTags *tags, void *user_data)
{
  MeloLibraryBrowserAsync *async = user_data;
  Browser__Response__MediaItem *item;

  /* Set category item */
  item = &async->items[async->media_list->n_items];
  browser__response__media_item__init (item);
  item->type = BROWSER__RESPONSE__MEDIA_ITEM__TYPE__FOLDER;
  item->id = g_strdup (data->id);
  if (!data->name)
    item->name = g_strdup ("Unknown");
  else
    item->name = g_strdup (data->name);

  /* Add action IDs */
  item->n_action_ids = G_N_ELEMENTS (category_actions);
  item->action_ids = category_actions;

  /* Add item to list */
  async->media_list->items[async->media_list->n_items] = item;
  async->media_list->n_items++;

  return true;
}

static bool
melo_library_browser_get_media_list (MeloLibraryBrowser *browser,
    Browser__Request__GetMediaList *r, MeloRequest *req)
{
  Browser__Response resp = BROWSER__RESPONSE__INIT;
  Browser__Response__MediaList media_list = BROWSER__RESPONSE__MEDIA_LIST__INIT;
  MeloLibraryField sort_field = MELO_LIBRARY_FIELD_NAME;
  MeloLibraryField fields[3] = {0};
  MeloLibraryBrowserAsync async;
  MeloLibraryType type;
  MeloMessage *msg;
  const char *query = r->query;
  uint64_t ids[3] = {0};
  unsigned int i;
  size_t count;
  bool search = false;
  bool sort_desc = false;
  char *sort[2] = {"n", "a"};

  /* Root media list */
  if (!g_strcmp0 (query, "/"))
    return melo_library_browser_get_root (req);

  /* Perform search */
  if (g_str_has_prefix (r->query, "search:")) {
    search = true;
    query += 7;
  } else
    query++;

  /* Parse query */
  if (!search && !melo_library_parse_query (query, &type, fields, ids))
    return false;

  /* Set sort field */
  if (r->n_sort && (search || type == MELO_LIBRARY_TYPE_MEDIA)) {
    switch (*r->sort[0]) {
    case 't':
      sort_field = MELO_LIBRARY_FIELD_TITLE;
      sort[0] = "t";
      break;
    case 'a':
      sort_field = MELO_LIBRARY_FIELD_ARTIST;
      sort[0] = "a";
      break;
    case 'l':
      sort_field = MELO_LIBRARY_FIELD_ALBUM;
      sort[0] = "l";
      break;
    case 'g':
      sort_field = MELO_LIBRARY_FIELD_GENRE;
      sort[0] = "g";
      break;
    case 'k':
      sort_field = MELO_LIBRARY_FIELD_TRACK;
      sort[0] = "k";
      break;
    }
  }

  /* Set sort order */
  if (r->n_sort > 1 && *r->sort[1] == 'd') {
    sort[1] = "d";
    sort_desc = true;
  }

  /* Limit count */
  count = r->count < MELO_LIBRARY_MAX_COUNT ? r->count : MELO_LIBRARY_MAX_COUNT;

  /* Set response type */
  resp.resp_case = BROWSER__RESPONSE__RESP_MEDIA_LIST;
  resp.media_list = &media_list;

  /* Allocate media item list */
  media_list.items = malloc (sizeof (*media_list.items) * count);
  async.items = malloc (sizeof (*async.items) * count);
  async.media_list = &media_list;

  /* Get data from library */
  if (search) {
    melo_library_find (MELO_LIBRARY_TYPE_MEDIA, media_cb, &async,
        MELO_LIBRARY_SELECT (NAME) | MELO_LIBRARY_SELECT (TITLE) |
            MELO_LIBRARY_SELECT (ARTIST) | MELO_LIBRARY_SELECT (ALBUM) |
            MELO_LIBRARY_SELECT (GENRE) | MELO_LIBRARY_SELECT (TRACK) |
            MELO_LIBRARY_SELECT (COVER),
        count, r->offset, sort_field, sort_desc, true, MELO_LIBRARY_FIELD_MEDIA,
        query, MELO_LIBRARY_FIELD_NAME, query, MELO_LIBRARY_FIELD_TITLE, query,
        MELO_LIBRARY_FIELD_ARTIST, query, MELO_LIBRARY_FIELD_ALBUM, query,
        MELO_LIBRARY_FIELD_LAST);
  } else if (type == MELO_LIBRARY_TYPE_MEDIA) {
    melo_library_find (MELO_LIBRARY_TYPE_MEDIA, media_cb, &async,
        MELO_LIBRARY_SELECT (NAME) | MELO_LIBRARY_SELECT (TITLE) |
            MELO_LIBRARY_SELECT (ARTIST) | MELO_LIBRARY_SELECT (ALBUM) |
            MELO_LIBRARY_SELECT (GENRE) | MELO_LIBRARY_SELECT (TRACK) |
            MELO_LIBRARY_SELECT (COVER),
        count, r->offset, sort_field, sort_desc, false, fields[0], ids[0],
        fields[1], ids[1], MELO_LIBRARY_FIELD_LAST);
  } else {
    melo_library_find (type, category_cb, &async, MELO_LIBRARY_SELECT (NAME),
        count, r->offset, sort_field, sort_desc, false, fields[0], ids[0],
        MELO_LIBRARY_FIELD_LAST);
  }

  /* Set actions */
  media_list.n_actions = G_N_ELEMENTS (actions_ptr);
  media_list.actions = actions_ptr;

  /* Add action IDs and sort menu */
  if (!search && type == MELO_LIBRARY_TYPE_MEDIA) {
    media_list.n_action_ids = G_N_ELEMENTS (category_actions);
    media_list.action_ids = category_actions;
    media_list.n_sort_menus = G_N_ELEMENTS (sort_media_menus_ptr);
    media_list.sort_menus = sort_media_menus_ptr;
  } else {
    media_list.n_sort_menus = G_N_ELEMENTS (sort_category_menus_ptr);
    media_list.sort_menus = sort_category_menus_ptr;
  }

  /* Set current sort setting */
  media_list.n_sort = 2;
  media_list.sort = sort;

  /* Update count and offset */
  media_list.count = media_list.n_items;
  media_list.offset = r->offset;

  /* Pack message */
  msg = melo_message_new (browser__response__get_packed_size (&resp));
  melo_message_set_size (
      msg, browser__response__pack (&resp, melo_message_get_data (msg)));

  /* Free items */
  for (i = 0; i < media_list.n_items; i++) {
    tags__tags__free_unpacked (media_list.items[i]->tags, NULL);
    g_free (media_list.items[i]->id);
    g_free (media_list.items[i]->name);
  }
  free (async.items);
  free (media_list.items);

  /* Send media list response */
  melo_request_send_response (req, msg);

  /* Release request */
  melo_request_complete (req);

  return true;
}

typedef struct {
  MeloPlaylistEntry *entry;
  MeloLibraryField field;
} MeloLibraryBrowserAction;

static bool
action_cb (const MeloLibraryData *data, MeloTags *tags, void *user_data)
{
  MeloLibraryBrowserAction *action = user_data;
  char *path;

  path = g_build_filename (data->path, data->media, NULL);
  if (action->entry) {
    /* First media */
    if (action->field != MELO_LIBRARY_FIELD_NONE) {
      const char *name;

      /* Generate name */
      switch (action->field) {
      case MELO_LIBRARY_FIELD_ARTIST_ID:
        name = melo_tags_get_artist (tags);
        break;
      case MELO_LIBRARY_FIELD_ALBUM_ID:
        name = melo_tags_get_album (tags);
        break;
      case MELO_LIBRARY_FIELD_GENRE_ID:
        name = melo_tags_get_genre (tags);
        break;
      case MELO_LIBRARY_FIELD_FAVORITE:
        name = "Favorites";
        break;
      default:
        name = NULL;
      }

      /* Update entry name and tags */
      melo_playlist_entry_update (action->entry, name, NULL, true);
      action->field = MELO_LIBRARY_FIELD_NONE;
    }

    /* Add media */
    melo_playlist_entry_add_media (action->entry, data->player, path,
        data->name, melo_tags_ref (tags), NULL);
  } else
    action->entry = melo_playlist_entry_new (
        data->player, path, data->name, melo_tags_ref (tags));
  g_free (path);

  return true;
}

static bool
melo_library_browser_do_action (MeloLibraryBrowser *browser,
    Browser__Request__DoAction *r, MeloRequest *req)
{
  MeloLibraryType type = MELO_LIBRARY_TYPE_MEDIA;
  MeloLibraryField fields[3] = {0};
  const char *path = r->path;
  uint64_t ids[3] = {0};
  bool ret = true;

  /* Check action type */
  if (r->type != BROWSER__ACTION__TYPE__PLAY &&
      r->type != BROWSER__ACTION__TYPE__ADD &&
      r->type != BROWSER__ACTION__TYPE__SET_FAVORITE &&
      r->type != BROWSER__ACTION__TYPE__UNSET_FAVORITE)
    return false;

  /* Action on search item */
  if (g_str_has_prefix (path, "search:")) {
    fields[0] = MELO_LIBRARY_FIELD_MEDIA_ID;
    ids[0] = strtoull (path + 7, NULL, 10);
  } else if (!melo_library_parse_query (path + 1, &type, fields, ids))
    return false;

  /* Can play only medias */
  if (type != MELO_LIBRARY_TYPE_MEDIA)
    return false;

  /* Do action */
  if (r->type == BROWSER__ACTION__TYPE__SET_FAVORITE)
    melo_library_update_media_flags (ids[0], MELO_LIBRARY_FLAG_FAVORITE, false);
  else if (r->type == BROWSER__ACTION__TYPE__UNSET_FAVORITE)
    melo_library_update_media_flags (ids[0], MELO_LIBRARY_FLAG_FAVORITE, true);
  else {
    MeloLibraryBrowserAction action;

    /* Create playlist entry to handle multiple medias */
    action.entry =
        fields[0] != MELO_LIBRARY_FIELD_MEDIA_ID
            ? melo_playlist_entry_new (NULL, NULL, "Library selection", NULL)
            : NULL;
    action.field = fields[0];

    /* Prepare media(s) */
    melo_library_find (MELO_LIBRARY_TYPE_MEDIA, action_cb, &action,
        MELO_LIBRARY_SELECT (PLAYER) | MELO_LIBRARY_SELECT (PATH) |
            MELO_LIBRARY_SELECT (MEDIA) | MELO_LIBRARY_SELECT (NAME) |
            MELO_LIBRARY_SELECT (TITLE) | MELO_LIBRARY_SELECT (ARTIST) |
            MELO_LIBRARY_SELECT (ALBUM) | MELO_LIBRARY_SELECT (GENRE) |
            MELO_LIBRARY_SELECT (TRACK) | MELO_LIBRARY_SELECT (COVER),
        fields[0] != MELO_LIBRARY_FIELD_MEDIA_ID ? MELO_LIBRARY_MAX_COUNT : 1,
        0, MELO_LIBRARY_FIELD_NONE, false, false, fields[0], ids[0],
        MELO_LIBRARY_FIELD_LAST);

    /* Do action */
    if (action.entry) {
      if (r->type == BROWSER__ACTION__TYPE__PLAY)
        melo_playlist_play_entry (action.entry);
      else
        melo_playlist_add_entry (action.entry);
    }
  }

  return ret;
}

static bool
melo_library_browser_handle_request (
    MeloBrowser *browser, const MeloMessage *msg, MeloRequest *req)
{
  MeloLibraryBrowser *lbrowser = MELO_LIBRARY_BROWSER (browser);
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
    ret =
        melo_library_browser_get_media_list (lbrowser, r->get_media_list, req);
    break;
  case BROWSER__REQUEST__REQ_DO_ACTION:
    ret = melo_library_browser_do_action (lbrowser, r->do_action, req);
    break;
  default:
    MELO_LOGE ("request %u not supported", r->req_case);
  }

  /* Free request */
  browser__request__free_unpacked (r, NULL);

  return ret;
}
