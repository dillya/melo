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

#include <melo/melo_http_client.h>
#include <melo/melo_library.h>
#include <melo/melo_playlist.h>

#define MELO_LOG_TAG "radio_browser"
#include <melo/melo_log.h>

#include "browser.pb-c.h"

#include "melo_radio_browser.h"
#include "melo_radio_player.h"

#define MELO_RADIO_BROWSER_URL "http://fr1.api.radio-browser.info/json/"
#define MELO_RADIO_BROWSER_USER_AGENT "rad.io for Melo (Android API)"
#define MELO_RADIO_BROWSER_ASSET_URL ""

typedef struct {
  unsigned int offset;
  unsigned int count;
} MeloRadioBrowserAsync;

struct _MeloRadioBrowser {
  GObject parent_instance;

  MeloHttpClient *client;
};

MELO_DEFINE_BROWSER (MeloRadioBrowser, melo_radio_browser)

static bool melo_radio_browser_handle_request (
    MeloBrowser *browser, const MeloMessage *msg, MeloRequest *req);
static char *melo_radio_browser_get_asset (
    MeloBrowser *browser, const char *id);

static void
melo_radio_browser_finalize (GObject *object)
{
  MeloRadioBrowser *browser = MELO_RADIO_BROWSER (object);

  /* Release HTTP client */
  g_object_unref (browser->client);

  /* Chain finalize */
  G_OBJECT_CLASS (melo_radio_browser_parent_class)->finalize (object);
}

static void
melo_radio_browser_class_init (MeloRadioBrowserClass *klass)
{
  MeloBrowserClass *parent_class = MELO_BROWSER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Setup callbacks */
  parent_class->handle_request = melo_radio_browser_handle_request;
  parent_class->get_asset = melo_radio_browser_get_asset;

  /* Set finalize */
  object_class->finalize = melo_radio_browser_finalize;
}

static void
melo_radio_browser_init (MeloRadioBrowser *self)
{
  /* Create new HTTP client */
  self->client = melo_http_client_new (NULL);
}

MeloRadioBrowser *
melo_radio_browser_new ()
{
  return g_object_new (MELO_TYPE_RADIO_BROWSER, "id", MELO_RADIO_BROWSER_ID,
      "name", "Radio", "description", "Browse in radio directory", "icon",
      "fa:broadcast-tower", "support-search", true, NULL);
}

static void
list_category_cb (MeloHttpClient *client, JsonNode *node, void *user_data)
{
  MeloRequest *req = user_data;

  /* Make media list response from JSON node */
  if (node) {
    MeloRadioBrowserAsync *async = melo_request_get_user_data (req);
    Browser__Response resp = BROWSER__RESPONSE__INIT;
    Browser__Response__MediaList media_list =
        BROWSER__RESPONSE__MEDIA_LIST__INIT;
    Browser__Response__MediaItem **items_ptr;
    Browser__Response__MediaItem *items;
    MeloMessage *msg;
    JsonArray *array;
    unsigned int i, len, count;

    /* Set response type */
    resp.resp_case = BROWSER__RESPONSE__RESP_MEDIA_LIST;
    resp.media_list = &media_list;

    /* Get array from node */
    array = json_node_get_array (node);
    len = json_array_get_length (array);

    /* Invalid offset */
    if (async->offset >= len) {
      melo_request_complete (req);
      free (async);
      return;
    }

    /* Calculate item list length */
    count = len - async->offset;
    if (count > async->count)
      count = async->count;

    /* Allocate item list */
    items_ptr = malloc (sizeof (*items_ptr) * count);
    items = malloc (sizeof (*items) * count);

    /* Set item list */
    media_list.n_items = count;
    media_list.items = items_ptr;

    /* Set list count and offset */
    media_list.count = count;
    media_list.offset = async->offset;

    /* Add media items */
    for (i = 0; i < count; i++) {
      JsonObject *obj;

      /* Init media item */
      browser__response__media_item__init (&items[i]);
      media_list.items[i] = &items[i];

      /* Get next entry */
      obj = json_array_get_object_element (array, i + async->offset);
      if (!obj)
        continue;

      /* Set media */
      items[i].id = (char *) json_object_get_string_member (obj, "name");
      items[i].name = items[i].id;
      items[i].type = BROWSER__RESPONSE__MEDIA_ITEM__TYPE__FOLDER;
    }

    /* Pack message */
    msg = melo_message_new (browser__response__get_packed_size (&resp));
    melo_message_set_size (
        msg, browser__response__pack (&resp, melo_message_get_data (msg)));

    /* Free item list */
    free (items_ptr);
    free (items);

    /* Free async object */
    free (async);

    /* Send media list response */
    melo_request_send_response (req, msg);
  }

  /* Release request */
  melo_request_complete (req);
}

static void
list_station_cb (MeloHttpClient *client, JsonNode *node, void *user_data)
{
  static Browser__Action actions[4] = {
      {
          .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
          .type = BROWSER__ACTION__TYPE__PLAY,
          .name = "Play radio",
          .icon = "fa:play",
      },
      {
          .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
          .type = BROWSER__ACTION__TYPE__ADD,
          .name = "Add radio to playlist",
          .icon = "fa:plus",
      },
      {
          .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
          .type = BROWSER__ACTION__TYPE__SET_FAVORITE,
          .name = "Add radio to favorites",
          .icon = "fa:star",
      },
      {
          .base = PROTOBUF_C_MESSAGE_INIT (&browser__action__descriptor),
          .type = BROWSER__ACTION__TYPE__UNSET_FAVORITE,
          .name = "Remove radio from favorites",
          .icon = "fa:star",
      },
  };
  static Browser__Action *set_fav_actions_ptr[3] = {
      &actions[0],
      &actions[1],
      &actions[2],
  };
  static Browser__Action *unset_fav_actions_ptr[3] = {
      &actions[0],
      &actions[1],
      &actions[3],
  };
  MeloRequest *req = user_data;

  /* Make media list response from JSON node */
  if (node) {
    Browser__Response resp = BROWSER__RESPONSE__INIT;
    Browser__Response__MediaList media_list =
        BROWSER__RESPONSE__MEDIA_LIST__INIT;
    Browser__Response__MediaItem **items_ptr;
    Browser__Response__MediaItem *items;
    Tags__Tags *tags = NULL;
    MeloMessage *msg;
    JsonArray *array;
    unsigned int i, len;

    /* Set response type */
    resp.resp_case = BROWSER__RESPONSE__RESP_MEDIA_LIST;
    resp.media_list = &media_list;

    /* Get array from node */
    array = json_node_get_array (node);
    len = json_array_get_length (array);

    /* Allocate item list */
    items_ptr = malloc (sizeof (*items_ptr) * len);
    items = malloc (sizeof (*items) * len);
    tags = malloc (sizeof (*tags) * len);

    /* Set item list */
    media_list.n_items = len;
    media_list.items = items_ptr;

    /* Set list count and offset */
    media_list.count = len;
    media_list.offset = GPOINTER_TO_UINT (melo_request_get_user_data (req));

    /* Add media items */
    for (i = 0; i < len; i++) {
      const char *cover;
      JsonObject *obj;
      uint64_t id;

      /* Init media item */
      browser__response__media_item__init (&items[i]);
      media_list.items[i] = &items[i];
      tags__tags__init (&tags[i]);

      /* Get next entry */
      obj = json_array_get_object_element (array, i);
      if (!obj)
        continue;

      /* Set media */
      items[i].id = (char *) json_object_get_string_member (obj, "stationuuid");
      items[i].name = (char *) json_object_get_string_member (obj, "name");
      items[i].type = BROWSER__RESPONSE__MEDIA_ITEM__TYPE__MEDIA;

      /* Set favorite and actions */
      id = melo_library_get_media_id_from_browser (
          MELO_RADIO_BROWSER_ID, items[i].id);
      items[i].favorite =
          melo_library_media_get_flags (id) & MELO_LIBRARY_FLAG_FAVORITE;
      if (items[i].favorite) {
        items[i].n_actions = G_N_ELEMENTS (unset_fav_actions_ptr);
        items[i].actions = unset_fav_actions_ptr;
      } else {
        items[i].n_actions = G_N_ELEMENTS (set_fav_actions_ptr);
        items[i].actions = set_fav_actions_ptr;
      }

      /* Set tags */
      items[i].tags = &tags[i];

      /* Set cover */
      cover = json_object_get_string_member (obj, "favicon");
      if (cover && *cover != '\0')
        tags[i].cover =
            melo_tags_gen_cover (melo_request_get_object (req), cover);
    }

    /* Pack message */
    msg = melo_message_new (browser__response__get_packed_size (&resp));
    melo_message_set_size (
        msg, browser__response__pack (&resp, melo_message_get_data (msg)));

    /* Free covers */
    for (i = 0; i < len; i++)
      if (tags[i].cover != protobuf_c_empty_string)
        g_free (tags[i].cover);

    /* Free item list */
    free (items_ptr);
    free (items);
    free (tags);

    /* Send media list response */
    melo_request_send_response (req, msg);
  }

  /* Release request */
  melo_request_complete (req);
}

static bool
melo_radio_browser_get_root (MeloRequest *req)
{
  static const struct {
    const char *id;
    const char *name;
    const char *icon;
  } root[] = {
      {"countries", "Countries", "fa:flag"},
      {"states", "States", "fa:map-marker-alt"},
      {"languages", "Languages", "fa:globe-europe"},
      {"tags", "Tags", "fa:hashtag"},
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
melo_radio_browser_get_media_list (MeloRadioBrowser *browser,
    Browser__Request__GetMediaList *r, MeloRequest *req)
{
  MeloHttpClientJsonCb list_cb = list_station_cb;
  const char *query = r->query;
  bool search = false;
  char *url;
  bool ret;

  /* Root media list */
  if (!g_strcmp0 (query, "/"))
    return melo_radio_browser_get_root (req);

  /* Perform search */
  if (g_str_has_prefix (r->query, "search:")) {
    search = true;
    query += 7;
  } else
    query++;

  /* Generate URL */
  if (!search) {
    char *q;

    /* Split request */
    q = strchr (query, '/');

    /* Generate URL */
    if (!q || *q++ == '\0') {
      MeloRadioBrowserAsync *async;

      /* Allocate async object */
      async = malloc (sizeof (*async));
      if (!async)
        return false;

      /* Set async object */
      async->offset = r->offset;
      async->count = r->count;
      melo_request_set_user_data (req, async);

      /* Create category URL */
      url = g_strdup_printf (MELO_RADIO_BROWSER_URL "%s", query);
      list_cb = list_category_cb;
    } else {
      const char *cat;

      /* Get category */
      if (g_str_has_prefix (query, "countries/"))
        cat = "country";
      else if (g_str_has_prefix (query, "states/"))
        cat = "state";
      else if (g_str_has_prefix (query, "languages/"))
        cat = "language";
      else if (g_str_has_prefix (query, "tags/"))
        cat = "tag";
      else
        return false;

      /* Save offset */
      melo_request_set_user_data (req, GUINT_TO_POINTER (r->offset));

      /* Create sub-category URL */
      url = g_strdup_printf (MELO_RADIO_BROWSER_URL
          "stations/by%sexact/%s?offset=%u&limit=%u",
          cat, q, r->offset, r->count);
    }
  } else {
    /* Save offset */
    melo_request_set_user_data (req, GUINT_TO_POINTER (r->offset));

    /* Create search URL */
    url = g_strdup_printf (MELO_RADIO_BROWSER_URL
        "stations/byname/%s?offset=%u&limit=%u",
        query, r->offset, r->count);
  }

  MELO_LOGD ("get_media_list: %s", url);

  /* Get list from URL */
  ret = melo_http_client_get_json (browser->client, url, list_cb, req);
  g_free (url);

  return ret;
}

static void
action_cb (MeloHttpClient *client, JsonNode *node, void *user_data)
{
  MeloRequest *req = user_data;

  /* Extract radio URL from JSON node */
  if (node) {
    Browser__Action__Type type;
    MeloTags *tags = NULL;
    JsonArray *array;
    JsonObject *obj;

    /* Get array */
    array = json_node_get_array (node);
    if (!json_array_get_length (array))
      return;

    /* Get first object */
    obj = json_array_get_object_element (array, 0);
    if (obj) {
      const char *name, *url, *cover;

      /* Get URL and name */
      url = json_object_get_string_member (obj, "url");
      name = json_object_get_string_member (obj, "name");

      /* Get tags from object */
      cover = json_object_get_string_member (obj, "favicon");
      if (cover) {
        tags = melo_tags_new ();
        if (tags) {
          melo_tags_set_cover (tags, melo_request_get_object (req), cover);
          melo_tags_set_browser (tags, MELO_RADIO_BROWSER_ID);
          melo_tags_set_media_id (
              tags, json_object_get_string_member (obj, "stationuuid"));
        }
      }

      MELO_LOGD ("play radio %s: %s", name, url);

      /* Get action type */
      type = (uintptr_t) melo_request_get_user_data (req);

      /* Do action */
      if (type == BROWSER__ACTION__TYPE__PLAY)
        melo_playlist_play_media (MELO_RADIO_PLAYER_ID, url, name, tags);
      else if (type == BROWSER__ACTION__TYPE__ADD)
        melo_playlist_add_media (MELO_RADIO_PLAYER_ID, url, name, tags);
      else {
        char *path, *media;

        /* Separate path */
        path = g_strdup (url);
        media = strrchr (path, '/');
        if (media)
          *media++ = '\0';

        /* Set / unset favorite marker */
        if (type == BROWSER__ACTION__TYPE__UNSET_FAVORITE) {
          uint64_t id;

          /* Get media ID */
          id = melo_library_get_media_id (
              MELO_RADIO_PLAYER_ID, 0, path, 0, media);

          /* Unset favorite */
          melo_library_update_media_flags (
              id, MELO_LIBRARY_FLAG_FAVORITE_ONLY, true);
        } else if (type == BROWSER__ACTION__TYPE__SET_FAVORITE)
          /* Set favorite */
          melo_library_add_media (MELO_RADIO_PLAYER_ID, 0, path, 0, media, 0,
              MELO_LIBRARY_SELECT (COVER), name, tags, 0,
              MELO_LIBRARY_FLAG_FAVORITE_ONLY);

        /* Free resources */
        g_free (path);
        melo_tags_unref (tags);
      }
    }
  }

  /* Release request */
  melo_request_complete (req);
}

static bool
melo_radio_browser_do_action (
    MeloRadioBrowser *browser, Browser__Request__DoAction *r, MeloRequest *req)
{
  const char *path = r->path;
  const char *id;
  char *url;
  bool ret;

  /* Check action type */
  if (r->type != BROWSER__ACTION__TYPE__PLAY &&
      r->type != BROWSER__ACTION__TYPE__ADD &&
      r->type != BROWSER__ACTION__TYPE__SET_FAVORITE &&
      r->type != BROWSER__ACTION__TYPE__UNSET_FAVORITE)
    return false;

  /* Action on search item */
  if (g_str_has_prefix (path, "search:"))
    path += 7;

  /* Get station ID */
  id = strrchr (path, '/');
  if (id)
    id++;
  else
    id = path;

  /* Save action type in request */
  melo_request_set_user_data (req, (void *) r->type);

  /* Generate URL from path */
  url = g_strdup_printf (MELO_RADIO_BROWSER_URL "stations/byuuid/%s", id);

  /* Get radio URL from sparod */
  ret = melo_http_client_get_json (browser->client, url, action_cb, req);
  g_free (url);

  return ret;
}

static bool
melo_radio_browser_handle_request (
    MeloBrowser *browser, const MeloMessage *msg, MeloRequest *req)
{
  MeloRadioBrowser *rbrowser = MELO_RADIO_BROWSER (browser);
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
    ret = melo_radio_browser_get_media_list (rbrowser, r->get_media_list, req);
    break;
  case BROWSER__REQUEST__REQ_DO_ACTION:
    ret = melo_radio_browser_do_action (rbrowser, r->do_action, req);
    break;
  default:
    MELO_LOGE ("request %u not supported", r->req_case);
  }

  /* Free request */
  browser__request__free_unpacked (r, NULL);

  return ret;
}

static char *
melo_radio_browser_get_asset (MeloBrowser *browser, const char *id)
{
  return g_strdup (id);
}
