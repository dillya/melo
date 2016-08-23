/*
 * melo_player_jsonrpc.c: Player base JSON-RPC interface
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

#include "melo_player_jsonrpc.h"

typedef enum {
  MELO_PLAYER_JSONRPC_FIELDS_NONE = 0,
  MELO_PLAYER_JSONRPC_FIELDS_STATE = 1,
  MELO_PLAYER_JSONRPC_FIELDS_NAME = 2,
  MELO_PLAYER_JSONRPC_FIELDS_POS = 4,
  MELO_PLAYER_JSONRPC_FIELDS_DURATION = 8,
  MELO_PLAYER_JSONRPC_FIELDS_TAGS = 16,

  MELO_PLAYER_JSONRPC_FIELDS_FULL = ~0,
} MeloPlayerJSONRPCFields;

static MeloPlayer *
melo_player_jsonrpc_get_player (JsonObject *obj, JsonNode **error)
{
  MeloPlayer *play;
  const gchar *id;

  /* Get player by id */
  id = json_object_get_string_member (obj, "id");
  play = melo_player_get_player_by_id (id);
  if (play)
    return play;

  /* No player found */
  *error = melo_jsonrpc_build_error_node (MELO_JSONRPC_ERROR_INVALID_PARAMS,
                                          "No player found!");
  return NULL;
}

MeloPlayerJSONRPCInfoFields
melo_player_jsonrpc_get_info_fields (JsonObject *obj)
{
  MeloPlayerJSONRPCInfoFields fields = MELO_PLAYER_JSONRPC_INFO_FIELDS_NONE;
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
      fields = MELO_PLAYER_JSONRPC_INFO_FIELDS_NONE;
      break;
    } else if (!g_strcmp0 (field, "full")) {
      fields = MELO_PLAYER_JSONRPC_INFO_FIELDS_FULL;
      break;
    } else if (!g_strcmp0 (field, "playlist"))
      fields |= MELO_PLAYER_JSONRPC_INFO_FIELDS_PLAYLIST;
  }

  return fields;
}

JsonObject *
melo_player_jsonrpc_info_to_object (const gchar *id,
                                    const MeloPlayerInfo *info,
                                    MeloPlayerJSONRPCInfoFields fields)
{
  JsonObject *obj = json_object_new ();
  if (id)
    json_object_set_string_member (obj, "id", id);
  if (info) {
    if (fields & MELO_PLAYER_JSONRPC_INFO_FIELDS_PLAYLIST)
      json_object_set_string_member (obj, "playlist", info->playlist_id);
  }
  return obj;
}

static MeloPlayerJSONRPCFields
melo_player_jsonrpc_get_fields (JsonObject *obj)
{
  MeloPlayerJSONRPCFields fields = MELO_PLAYER_JSONRPC_FIELDS_NONE;
  const gchar *field;
  JsonArray *array;
  guint count, i;

  /* Check if fields is available */
  if (!json_object_has_member (obj, "fields"))
    return MELO_PLAYER_JSONRPC_FIELDS_NONE;

  /* Get fields array */
  array = json_object_get_array_member (obj, "fields");
  if (!array)
    return MELO_PLAYER_JSONRPC_FIELDS_NONE;

  /* Parse array */
  count = json_array_get_length (array);
  for (i = 0; i < count; i++) {
    field = json_array_get_string_element (array, i);
    if (!field)
      break;
    if (!g_strcmp0 (field, "none")) {
      fields = MELO_PLAYER_JSONRPC_FIELDS_NONE;
      break;
    } else if (!g_strcmp0 (field, "full")) {
      fields = MELO_PLAYER_JSONRPC_FIELDS_FULL;
      break;
    } else if (!g_strcmp0 (field, "state"))
      fields |= MELO_PLAYER_JSONRPC_FIELDS_STATE;
    else if (!g_strcmp0 (field, "name"))
      fields |= MELO_PLAYER_JSONRPC_FIELDS_NAME;
    else if (!g_strcmp0 (field, "pos"))
      fields |= MELO_PLAYER_JSONRPC_FIELDS_POS;
    else if (!g_strcmp0 (field, "duration"))
      fields |= MELO_PLAYER_JSONRPC_FIELDS_DURATION;
    else if (!g_strcmp0 (field, "tags"))
      fields |= MELO_PLAYER_JSONRPC_FIELDS_TAGS;
  }

  return fields;
}

static JsonObject *
melo_player_jsonrpc_status_to_object (const MeloPlayerStatus *status,
                                      MeloPlayerJSONRPCFields fields,
                                      MeloTagsFields tags_fields,
                                      gint64 tags_timestamp)
{
  JsonObject *obj = json_object_new ();
  if (fields & MELO_PLAYER_JSONRPC_FIELDS_STATE) {
    json_object_set_string_member (obj, "state",
                                   melo_player_state_to_string (status->state));
    if (status->state == MELO_PLAYER_STATE_ERROR)
      json_object_set_string_member (obj, "error", status->error);
  }
  if (fields & MELO_PLAYER_JSONRPC_FIELDS_NAME)
    json_object_set_string_member (obj, "name", status->name);
  if (fields & MELO_PLAYER_JSONRPC_FIELDS_POS)
    json_object_set_int_member (obj, "pos", status->pos);
  if (fields & MELO_PLAYER_JSONRPC_FIELDS_DURATION)
    json_object_set_int_member (obj, "duration", status->duration);
  if (fields & MELO_PLAYER_JSONRPC_FIELDS_TAGS) {
    MeloTags *tags;

    /* Get tags from status */
    tags = melo_player_status_get_tags (status);
    if (tags && (tags_timestamp <= 0 ||
                 melo_tags_updated (tags, tags_timestamp))) {
      json_object_set_object_member (obj, "tags",
                                     melo_tags_to_json_object (tags,
                                                               tags_fields));
      melo_tags_unref (tags);
    } else
      json_object_set_null_member (obj, "tags");
  }
  return obj;
}

/* Method callbacks */
static void
melo_player_jsonrpc_get_info (const gchar *method,
                              JsonArray *s_params, JsonNode *params,
                              JsonNode **result, JsonNode **error,
                              gpointer user_data)
{
  MeloPlayerJSONRPCInfoFields fields = MELO_PLAYER_JSONRPC_INFO_FIELDS_NONE;
  const MeloPlayerInfo *info = NULL;
  MeloPlayer *play;
  JsonObject *obj;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get player from ID */
  play = melo_player_jsonrpc_get_player (obj, error);
  if (!play) {
    json_object_unref (obj);
    return;
  }

  /* Get info fields */
  fields = melo_player_jsonrpc_get_info_fields (obj);
  json_object_unref (obj);

  /* Generate object */
  info = melo_player_get_info (play);
  obj = melo_player_jsonrpc_info_to_object (NULL, info, fields);
  g_object_unref (play);

  /* Return result */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_player_jsonrpc_set_state (const gchar *method,
                               JsonArray *s_params, JsonNode *params,
                               JsonNode **result, JsonNode **error,
                               gpointer user_data)
{
  MeloPlayerState state = MELO_PLAYER_STATE_NONE;
  const gchar *sstate;
  MeloPlayer *play;
  JsonObject *obj;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get player from id */
  play = melo_player_jsonrpc_get_player (obj, error);
  if (!play) {
    json_object_unref (obj);
    return;
  }

  /* Get requested state */
  sstate = json_object_get_string_member (obj, "state");
  state = melo_player_state_from_string (sstate);
  json_object_unref (obj);

  /* Set new state */
  melo_player_set_state (play, state);
  g_object_unref (play);

  /* Create and fill object */
  obj = json_object_new ();
  json_object_set_string_member (obj, "state",
                                 melo_player_state_to_string (state));

  /* Return result */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_player_jsonrpc_set_pos (const gchar *method,
                             JsonArray *s_params, JsonNode *params,
                             JsonNode **result, JsonNode **error,
                             gpointer user_data)
{
  MeloPlayer *play;
  JsonObject *obj;
  gint pos;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get player from id */
  play = melo_player_jsonrpc_get_player (obj, error);
  if (!play) {
    json_object_unref (obj);
    return;
  }

  /* Get requested position */
  pos = json_object_get_int_member (obj, "pos");
  json_object_unref (obj);

  /* Set new position */
  pos = melo_player_set_pos (play, pos);
  g_object_unref (play);

  /* Create and fill object */
  obj = json_object_new ();
  json_object_set_int_member (obj, "pos", pos);

  /* Return result */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_player_jsonrpc_get_status (const gchar *method,
                                JsonArray *s_params, JsonNode *params,
                                JsonNode **result, JsonNode **error,
                                gpointer user_data)
{
  MeloPlayerJSONRPCFields fields = MELO_PLAYER_JSONRPC_FIELDS_NONE;
  MeloTagsFields tags_fields = MELO_TAGS_FIELDS_NONE;
  MeloPlayerStatus *status = NULL;
  MeloPlayer *play;
  JsonArray *array;
  JsonObject *obj;
  gint64 tags_ts = 0;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get player from id */
  play = melo_player_jsonrpc_get_player (obj, error);
  if (!play) {
    json_object_unref (obj);
    return;
  }

  /* Get fields */
  fields = melo_player_jsonrpc_get_fields (obj);

  /* Get tags fields */
  if (fields & MELO_PLAYER_JSONRPC_FIELDS_TAGS &&
      json_object_has_member (obj, "tags")) {
    /* Get tags fields array */
    array = json_object_get_array_member (obj, "tags");
    if (array)
      tags_fields = melo_tags_get_fields_from_json_array (array);

    /* Get tags timestamp */
    if (json_object_has_member (obj, "tags_ts"))
      tags_ts = json_object_get_int_member (obj, "tags_ts");
  }
  json_object_unref (obj);

  /* Get status */
  status = melo_player_get_status (play);
  g_object_unref (play);
  if (!status)
    return;

  /* Generate status */
  obj = melo_player_jsonrpc_status_to_object (status, fields,
                                              tags_fields, tags_ts);
  melo_player_status_unref (status);

  /* Return result */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_player_jsonrpc_action (const gchar *method,
                            JsonArray *s_params, JsonNode *params,
                            JsonNode **result, JsonNode **error,
                            gpointer user_data)
{
  MeloPlayer *play;
  JsonObject *obj;
  gboolean ret = FALSE;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get player from id */
  play = melo_player_jsonrpc_get_player (obj, error);
  json_object_unref (obj);
  if (!play)
    return;

  /* Do action */
  if (g_str_equal (method, "player.prev"))
    ret = melo_player_prev (play);
  else if (g_str_equal (method, "player.next"))
    ret = melo_player_next (play);
  g_object_unref (play);

  /* Create answer */
  obj = json_object_new ();
  json_object_set_boolean_member (obj, "done", ret);

  /* Return result */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}
/* List of methods */
static MeloJSONRPCMethod melo_player_jsonrpc_methods[] = {
  {
    .method = "get_info",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_player_jsonrpc_get_info,
    .user_data = NULL,
  },
  {
    .method = "set_state",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"state\", \"type\": \"string\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_player_jsonrpc_set_state,
    .user_data = NULL,
  },
  {
    .method = "set_pos",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"pos\", \"type\": \"int\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_player_jsonrpc_set_pos,
    .user_data = NULL,
  },
  {
    .method = "get_status",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"tags\", \"type\": \"array\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"tags_ts\", \"type\": \"int\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_player_jsonrpc_get_status,
    .user_data = NULL,
  },
  {
    .method = "prev",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_player_jsonrpc_action,
    .user_data = NULL,
  },
  {
    .method = "next",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_player_jsonrpc_action,
    .user_data = NULL,
  },
};

/* Register / Unregister methods */
void
melo_player_register_methods (void)
{
  melo_jsonrpc_register_methods ("player", melo_player_jsonrpc_methods,
                                 G_N_ELEMENTS (melo_player_jsonrpc_methods));
}

void
melo_player_unregister_methods (void)
{
  melo_jsonrpc_unregister_methods ("player", melo_player_jsonrpc_methods,
                                   G_N_ELEMENTS (melo_player_jsonrpc_methods));
}
