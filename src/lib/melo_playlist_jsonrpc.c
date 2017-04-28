/*
 * melo_playlist_jsonrpc.c: Playlist base JSON-RPC interface
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

#include "melo_playlist_jsonrpc.h"

typedef enum {
  MELO_PLAYLIST_JSONRPC_LIST_FIELDS_NONE = 0,
  MELO_PLAYLIST_JSONRPC_LIST_FIELDS_NAME = 1,
  MELO_PLAYLIST_JSONRPC_LIST_FIELDS_FULL_NAME = 2,
  MELO_PLAYLIST_JSONRPC_LIST_FIELDS_CMDS = 4,
  MELO_PLAYLIST_JSONRPC_LIST_FIELDS_TAGS = 8,

  MELO_PLAYLIST_JSONRPC_LIST_FIELDS_FULL = ~0,
} MeloPlaylistJSONRPCListFields;

static MeloPlaylist *
melo_playlist_jsonrpc_get_playlist (JsonObject *obj, JsonNode **error)
{
  MeloPlaylist *plist;
  const gchar *id;

  /* Get playlist by id */
  id = json_object_get_string_member (obj, "id");
  plist = melo_playlist_get_playlist_by_id (id);
  if (plist)
    return plist;

  /* No playlist found */
  *error = melo_jsonrpc_build_error_node (MELO_JSONRPC_ERROR_INVALID_PARAMS,
                                          "No playlist found!");
  return NULL;
}

MeloPlaylistJSONRPCListFields
melo_playlist_jsonrpc_get_list_fields (JsonObject *obj)
{
  MeloPlaylistJSONRPCListFields fields = MELO_PLAYLIST_JSONRPC_LIST_FIELDS_NONE;
  const gchar *field;
  JsonArray *array;
  guint count, i;

  /* Check if fields is available */
  if (!json_object_has_member (obj, "fields"))
    return fields;

  /* Get fields array */
  array = json_object_get_array_member (obj, "fields");
  if (!array)
    return fields;

  /* Parse array */
  count = json_array_get_length (array);
  for (i = 0; i < count; i++) {
    field = json_array_get_string_element (array, i);
    if (!field)
      break;
    if (!g_strcmp0 (field, "none")) {
      fields = MELO_PLAYLIST_JSONRPC_LIST_FIELDS_NONE;
      break;
    } else if (!g_strcmp0 (field, "full")) {
      fields = MELO_PLAYLIST_JSONRPC_LIST_FIELDS_FULL;
      break;
    } else if (!g_strcmp0 (field, "name"))
      fields |= MELO_PLAYLIST_JSONRPC_LIST_FIELDS_NAME;
    else if (!g_strcmp0 (field, "full_name"))
      fields |= MELO_PLAYLIST_JSONRPC_LIST_FIELDS_FULL_NAME;
    else if (!g_strcmp0 (field, "cmds"))
      fields |=  MELO_PLAYLIST_JSONRPC_LIST_FIELDS_CMDS;
    else if (!g_strcmp0 (field, "tags"))
      fields |= MELO_PLAYLIST_JSONRPC_LIST_FIELDS_TAGS;
  }

  return fields;
}

JsonArray *
melo_playlist_jsonrpc_list_to_array (const GList *list,
                                     MeloPlaylistJSONRPCListFields fields,
                                     MeloTagsFields tags_fields)
{
  JsonArray *array;
  const GList *l;

  /* Parse list and create array */
  array = json_array_new ();
  for (l = list; l != NULL; l = l->next) {
    MeloPlaylistItem *item = (MeloPlaylistItem *) l->data;
    JsonObject *obj = json_object_new ();
    if (fields & MELO_PLAYLIST_JSONRPC_LIST_FIELDS_NAME)
      json_object_set_string_member (obj, "name", item->name);
    if (fields & MELO_PLAYLIST_JSONRPC_LIST_FIELDS_FULL_NAME)
      json_object_set_string_member (obj, "full_name", item->full_name);
    if (fields & MELO_PLAYLIST_JSONRPC_LIST_FIELDS_CMDS) {
      json_object_set_boolean_member (obj, "can_play", item->can_play);
      json_object_set_boolean_member (obj, "can_remove", item->can_remove);
    }
    if (fields & MELO_PLAYLIST_JSONRPC_LIST_FIELDS_TAGS) {
      if (item->tags) {
        JsonObject *tags = melo_tags_to_json_object (item->tags, tags_fields);
        json_object_set_object_member (obj, "tags", tags);
      } else
        json_object_set_null_member (obj, "tags");
    }
    json_array_add_object_element (array, obj);
  }

  return array;
}

/* Method callbacks */
static void
melo_playlist_jsonrpc_get_list (const gchar *method,
                               JsonArray *s_params, JsonNode *params,
                               JsonNode **result, JsonNode **error,
                               gpointer user_data)
{
  MeloPlaylistJSONRPCListFields fields = MELO_PLAYLIST_JSONRPC_LIST_FIELDS_NONE;
  MeloTagsFields tags_fields = MELO_TAGS_FIELDS_NONE;
  MeloPlaylist *plist;
  JsonArray *array;
  JsonObject *obj;
  MeloPlaylistList *list;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get playlist from ID */
  plist = melo_playlist_jsonrpc_get_playlist (obj, error);
  if (!plist) {
    json_object_unref (obj);
    return;
  }

  /* Get list fields */
  fields = melo_playlist_jsonrpc_get_list_fields (obj);

  /* Get tags if needed */
  if (fields & MELO_PLAYLIST_JSONRPC_LIST_FIELDS_TAGS &&
      json_object_has_member (obj, "tags_fields")) {
    array = json_object_get_array_member (obj, "tags_fields");
    if (array)
      tags_fields = melo_tags_get_fields_from_json_array (array);
  }

  /* Get list */
  list = melo_playlist_get_list (plist, tags_fields);
  json_object_unref (obj);
  g_object_unref (plist);

  /* No list provided */
  if (!list) {
    *error = melo_jsonrpc_build_error_node (MELO_JSONRPC_ERROR_INVALID_REQUEST,
                                            "Method not available!");
    return;
  }

  /* Create a new object */
  obj = json_object_new ();
  json_object_set_string_member (obj, "current", list->current);

  /* Create array from list */
  array = melo_playlist_jsonrpc_list_to_array (list->items, fields,
                                               tags_fields);
  json_object_set_array_member (obj, "items", array);

  /* Free playlist list */
  melo_playlist_list_free (list);

  /* Return array */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_playlist_jsonrpc_get_tags (const gchar *method,
                                JsonArray *s_params, JsonNode *params,
                                JsonNode **result, JsonNode **error,
                                gpointer user_data)
{
  MeloTagsFields fields = MELO_TAGS_FIELDS_FULL;
  MeloTags *tags = NULL;
  MeloPlaylist *plist;
  JsonArray *array;
  JsonObject *obj;
  const gchar *name;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get playlist from ID */
  plist = melo_playlist_jsonrpc_get_playlist (obj, error);
  if (!plist) {
    json_object_unref (obj);
    return;
  }

  /* Get name */
  name = json_object_get_string_member (obj, "name");

  /* Get fields */
  if (json_object_has_member (obj, "fields")) {
    /* Get tags fields array */
    array = json_object_get_array_member (obj, "fields");
    if (array)
      fields = melo_tags_get_fields_from_json_array (array);
  }

  /* Get tags from playlist */
  tags = melo_playlist_get_tags (plist, name, fields);
  json_object_unref (obj);
  g_object_unref (plist);

  /* Parse list and create array */
  obj = melo_tags_to_json_object (tags, fields);
  melo_tags_unref (tags);

  /* Return object */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_playlist_jsonrpc_item_action (const gchar *method,
                                  JsonArray *s_params, JsonNode *params,
                                  JsonNode **result, JsonNode **error,
                                  gpointer user_data)
{
  const gchar *name;
  MeloPlaylist *plist;
  JsonObject *obj;
  gboolean ret = FALSE;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get playlist from ID */
  plist = melo_playlist_jsonrpc_get_playlist (obj, error);
  if (!plist) {
    json_object_unref (obj);
    return;
  }

  /* Get name */
  name = json_object_get_string_member (obj, "name");

  /* Do action on item */
  if (!g_strcmp0 (method, "playlist.play"))
    ret = melo_playlist_play (plist, name);
  else if (!g_strcmp0 (method, "playlist.remove"))
    ret = melo_playlist_remove (plist, name);
  json_object_unref (obj);
  g_object_unref (plist);

  /* Create result object */
  obj = json_object_new ();
  json_object_set_boolean_member (obj, "done", ret);

  /* Return array */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_playlist_jsonrpc_move (const gchar *method,
                            JsonArray *s_params, JsonNode *params,
                            JsonNode **result, JsonNode **error,
                            gpointer user_data)
{
  const gchar *name;
  MeloPlaylist *plist;
  JsonObject *obj;
  gboolean ret = FALSE;
  gint count = 1;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get playlist from ID */
  plist = melo_playlist_jsonrpc_get_playlist (obj, error);
  if (!plist) {
    json_object_unref (obj);
    return;
  }

  /* Get name */
  name = json_object_get_string_member (obj, "name");
  if (json_object_has_member (obj, "count"))
    count = json_object_get_int_member (obj, "count");
  else
    count = 1;

  /* Get move settings */
  if (!g_strcmp0 (method, "playlist.move")) {
    gint up;

    /* Move item up / down */
    up = json_object_get_int_member (obj, "up");
    ret = melo_playlist_move (plist, name, up, count);
  } else if (!g_strcmp0 (method, "playlist.move_to")) {
    const gchar *before;

    /* Move item before another item */
    before = json_object_get_string_member (obj, "before");
    ret = melo_playlist_move_to (plist, name, before, count);
  }
  json_object_unref (obj);
  g_object_unref (plist);

  /* Create result object */
  obj = json_object_new ();
  json_object_set_boolean_member (obj, "done", ret);

  /* Return array */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_playlist_jsonrpc_empty (const gchar *method,
                             JsonArray *s_params, JsonNode *params,
                             JsonNode **result, JsonNode **error,
                             gpointer user_data)
{
  const gchar *name;
  MeloPlaylist *plist;
  JsonObject *obj;
  gboolean ret = FALSE;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get playlist from ID */
  plist = melo_playlist_jsonrpc_get_playlist (obj, error);
  json_object_unref (obj);
  if (!plist)
    return;

  /* Empty playlist */
  melo_playlist_empty (plist);
  g_object_unref (plist);

  /* Create result object */
  obj = json_object_new ();
  json_object_set_boolean_member (obj, "done", TRUE);

  /* Return array */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

/* List of methods */
static MeloJSONRPCMethod melo_playlist_jsonrpc_methods[] = {
  {
    .method = "get_list",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"tags_fields\", \"type\": \"array\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_playlist_jsonrpc_get_list,
    .user_data = NULL,
  },
  {
    .method = "get_tags",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"name\", \"type\": \"string\"},"
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_playlist_jsonrpc_get_tags,
    .user_data = NULL,
  },
  {
    .method = "play",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"name\", \"type\": \"string\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_playlist_jsonrpc_item_action,
    .user_data = NULL,
  },
  {
    .method = "move",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"name\", \"type\": \"string\"},"
              "  {\"name\": \"up\", \"type\": \"integer\"},"
              "  {"
              "    \"name\": \"count\", \"type\": \"integer\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_playlist_jsonrpc_move,
    .user_data = NULL,
  },
  {
    .method = "move_to",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"name\", \"type\": \"string\"},"
              "  {\"name\": \"before\", \"type\": \"string\"},"
              "  {"
              "    \"name\": \"count\", \"type\": \"integer\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_playlist_jsonrpc_move,
    .user_data = NULL,
  },
  {
    .method = "remove",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"name\", \"type\": \"string\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_playlist_jsonrpc_item_action,
    .user_data = NULL,
  },
  {
    .method = "empty",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_playlist_jsonrpc_empty,
    .user_data = NULL,
  },
};

/* Register / Unregister methods */
void
melo_playlist_jsonrpc_register_methods (void)
{
  melo_jsonrpc_register_methods ("playlist", melo_playlist_jsonrpc_methods,
                                 G_N_ELEMENTS (melo_playlist_jsonrpc_methods));
}

void
melo_playlist_jsonrpc_unregister_methods (void)
{
  melo_jsonrpc_unregister_methods ("playlist", melo_playlist_jsonrpc_methods,
                                   G_N_ELEMENTS (melo_playlist_jsonrpc_methods));
}
