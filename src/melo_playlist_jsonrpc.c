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

/* Method callbacks */
static void
melo_playlist_jsonrpc_get_list (const gchar *method,
                               JsonArray *s_params, JsonNode *params,
                               JsonNode **result, JsonNode **error,
                               gpointer user_data)
{
  gchar *current = NULL;
  MeloPlaylist *plist;
  JsonArray *array;
  JsonObject *obj;
  GList *list, *l;

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

  /* Get list */
  list = melo_playlist_get_list (plist, &current);
  json_object_unref (obj);
  g_object_unref (plist);

  obj = json_object_new ();
  json_object_set_string_member (obj, "current", current);
  g_free (current);

  /* Parse list and create array */
  array = json_array_new ();
  for (l = list; l != NULL; l = l->next) {
    MeloPlaylistItem *item = (MeloPlaylistItem *) l->data;
    JsonObject *o = json_object_new ();
    json_object_set_string_member (o, "name", item->name);
    json_object_set_string_member (o, "full_name", item->full_name);
    json_object_set_boolean_member (o, "can_play", item->can_play);
    json_object_set_boolean_member (o, "can_remove", item->can_remove);
    json_array_add_object_element (array, o);
  }
  json_object_set_array_member (obj, "list", array);

  /* Free item list */
  g_list_free_full (list, (GDestroyNotify) melo_playlist_item_unref);

  /* Return array */
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

/* List of methods */
static MeloJSONRPCMethod melo_playlist_jsonrpc_methods[] = {
  {
    .method = "get_list",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_playlist_jsonrpc_get_list,
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
    .method = "remove",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"name\", \"type\": \"string\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_playlist_jsonrpc_item_action,
    .user_data = NULL,
  },
};

/* Register / Unregister methods */
void
melo_playlist_register_methods (void)
{
  melo_jsonrpc_register_methods ("playlist", melo_playlist_jsonrpc_methods,
                                 G_N_ELEMENTS (melo_playlist_jsonrpc_methods));
}

void
melo_playlist_unregister_methods (void)
{
  melo_jsonrpc_unregister_methods ("playlist", melo_playlist_jsonrpc_methods,
                                   G_N_ELEMENTS (melo_playlist_jsonrpc_methods));
}
