/*
 * melo_browser_jsonrpc.c: Browser base JSON-RPC interface
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

#include "melo_browser_jsonrpc.h"

/**
 * SECTION:melo_browser_jsonrpc
 * @title: MeloBrowserJsonRPC
 * @short_description: Basic JSON-RPC methods for Melo Browser
 *
 * Helper which implements all basic JSON-RPC methods for #MeloBrowser.
 */

typedef enum {
  MELO_BROWSER_JSONRPC_LIST_FIELDS_NONE = 0,
  MELO_BROWSER_JSONRPC_LIST_FIELDS_ID = 1,
  MELO_BROWSER_JSONRPC_LIST_FIELDS_NAME = 2,
  MELO_BROWSER_JSONRPC_LIST_FIELDS_TAGS = 4,
  MELO_BROWSER_JSONRPC_LIST_FIELDS_TYPE = 8,
  MELO_BROWSER_JSONRPC_LIST_FIELDS_ACTIONS = 16,

  MELO_BROWSER_JSONRPC_LIST_FIELDS_FULL = ~0,
} MeloBrowserJSONRPCListFields;

#define MELO_BROWSER_JSONRPC_LIST_FIELDS_DEFAULT \
  (MELO_BROWSER_JSONRPC_LIST_FIELDS_ID | \
   MELO_BROWSER_JSONRPC_LIST_FIELDS_NAME | \
   MELO_BROWSER_JSONRPC_LIST_FIELDS_TYPE | \
   MELO_BROWSER_JSONRPC_LIST_FIELDS_ACTIONS)

typedef enum {
  MELO_BROWSER_JSONRPC_TAGS_NONE = 0,
  MELO_BROWSER_JSONRPC_TAGS_NONE_CACHED,
  MELO_BROWSER_JSONRPC_TAGS_CACHED,
  MELO_BROWSER_JSONRPC_TAGS_FULL_CACHED,
  MELO_BROWSER_JSONRPC_TAGS_FULL,
} MeloBrowserJSONRPCTags;

static MeloBrowser *
melo_browser_jsonrpc_get_browser (JsonObject *obj, JsonNode **error)
{
  MeloBrowser *bro;
  const gchar *id;

  /* Get browser by id */
  id = json_object_get_string_member (obj, "id");
  bro = melo_browser_get_browser_by_id (id);
  if (bro)
    return bro;

  /* No browser found */
  *error = melo_jsonrpc_build_error_node (MELO_JSONRPC_ERROR_INVALID_PARAMS,
                                          "No browser found!");
  return NULL;
}

/**
 * melo_browser_jsonrpc_get_info_fields:
 * @obj: the #JsonObject to parse
 * @name: the name of the #JsonArray in @obj containing the fields
 *
 * Generate a #MeloBrowserJSONRPCInfoFields from a #JsonObject passed in @obj,
 * in order to know which fields of #MeloBrowserInfo are requested.
 *
 * Returns: a #MeloBrowserJSONRPCInfoFields generated from the object.
 */
MeloBrowserJSONRPCInfoFields
melo_browser_jsonrpc_get_info_fields (JsonObject *obj, const gchar *name)
{
  MeloBrowserJSONRPCInfoFields fields = MELO_BROWSER_JSONRPC_INFO_FIELDS_NONE;
  const gchar *field;
  JsonArray *array;
  guint count, i;

  /* Check if fields is available */
  if (!json_object_has_member (obj, name))
    return fields;

  /* Get fields array */
  array = json_object_get_array_member (obj, name);
  if (!array)
    return fields;

  /* Parse array */
  count = json_array_get_length (array);
  for (i = 0; i < count; i++) {
    field = json_array_get_string_element (array, i);
    if (!field)
      break;
    if (!g_strcmp0 (field, "none")) {
      fields = MELO_BROWSER_JSONRPC_INFO_FIELDS_NONE;
      break;
    } else if (!g_strcmp0 (field, "full")) {
      fields = MELO_BROWSER_JSONRPC_INFO_FIELDS_FULL;
      break;
    } else if (!g_strcmp0 (field, "name"))
      fields |= MELO_BROWSER_JSONRPC_INFO_FIELDS_NAME;
    else if (!g_strcmp0 (field, "description"))
      fields |= MELO_BROWSER_JSONRPC_INFO_FIELDS_DESCRIPTION;
    else if (!g_strcmp0 (field, "search"))
      fields |= MELO_BROWSER_JSONRPC_INFO_FIELDS_SEARCH;
    else if (!g_strcmp0 (field, "go"))
      fields |= MELO_BROWSER_JSONRPC_INFO_FIELDS_GO;
    else if (!g_strcmp0 (field, "tags"))
      fields |= MELO_BROWSER_JSONRPC_INFO_FIELDS_TAGS;
  }

  return fields;
}

/**
 * melo_browser_jsonrpc_info_to_object:
 * @id: the ID of the #MeloBrowser from which getting the #MeloBrowserInfo
 * @info: the #MeloBrowserInfo associated to the #MeloBrowser
 * @fields: the fields to fill in the #JsonObject from the @info
 *
 * Generate a #JsonObject which contains the translated details on the
 * #MeloBrowser pointed by @id.
 *
 * Returns: (transfer full): a new #JsonObject containing request details or
 * %NULL if an error occurred.
 */
JsonObject *
melo_browser_jsonrpc_info_to_object (const gchar *id,
                                     const MeloBrowserInfo *info,
                                     MeloBrowserJSONRPCInfoFields fields)
{
  JsonObject *obj = json_object_new ();
  if (id)
    json_object_set_string_member (obj, "id", id);
  if (info) {
    if (fields & MELO_BROWSER_JSONRPC_INFO_FIELDS_NAME)
      json_object_set_string_member (obj, "name", info->name);
    if (fields & MELO_BROWSER_JSONRPC_INFO_FIELDS_DESCRIPTION)
      json_object_set_string_member (obj, "description", info->description);
    if (fields & MELO_BROWSER_JSONRPC_INFO_FIELDS_SEARCH) {
      JsonObject *o = json_object_new ();
      json_object_set_boolean_member (o, "support", info->search_support);
      json_object_set_boolean_member (o, "hint_support",
                                      info->search_hint_support);
      json_object_set_string_member (o, "input_text", info->search_input_text);
      json_object_set_string_member (o, "button_text",
                                     info->search_button_text);
      json_object_set_object_member (obj, "search", o);
    }
    if (fields & MELO_BROWSER_JSONRPC_INFO_FIELDS_GO) {
      JsonObject *o = json_object_new ();
      json_object_set_boolean_member (o, "support", info->go_support);
      json_object_set_boolean_member (o, "list_support", info->go_list_support);
      json_object_set_boolean_member (o, "play_support", info->go_play_support);
      json_object_set_boolean_member (o, "add_support", info->go_add_support);
      json_object_set_string_member (o, "input_text", info->go_input_text);
      json_object_set_string_member (o, "button_list_text",
                                     info->go_button_list_text);
      json_object_set_string_member (o, "button_play_text",
                                     info->go_button_play_text);
      json_object_set_string_member (o, "button_add_text",
                                     info->go_button_add_text);
      json_object_set_object_member (obj, "go", o);
    }
    if (fields & MELO_BROWSER_JSONRPC_INFO_FIELDS_TAGS) {
      JsonObject *o = json_object_new ();
      json_object_set_boolean_member (o, "support", info->tags_support);
      json_object_set_boolean_member (o, "cache_support",
                                      info->tags_cache_support);
      json_object_set_object_member (obj, "tags", o);
    }
  }
  return obj;
}

MeloBrowserJSONRPCListFields
melo_browser_jsonrpc_get_list_fields (JsonObject *obj)
{
  MeloBrowserJSONRPCListFields fields = MELO_BROWSER_JSONRPC_LIST_FIELDS_NONE;
  const gchar *field;
  JsonArray *array;
  guint count, i;

  /* Check if fields is available */
  if (!json_object_has_member (obj, "fields"))
    return MELO_BROWSER_JSONRPC_LIST_FIELDS_DEFAULT;

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
      fields = MELO_BROWSER_JSONRPC_LIST_FIELDS_NONE;
      break;
    } else if (!g_strcmp0 (field, "full")) {
      fields = MELO_BROWSER_JSONRPC_LIST_FIELDS_FULL;
      break;
    } else if (!g_strcmp0 (field, "id"))
      fields |= MELO_BROWSER_JSONRPC_LIST_FIELDS_ID;
    else if (!g_strcmp0 (field, "name"))
      fields |= MELO_BROWSER_JSONRPC_LIST_FIELDS_NAME;
    else if (!g_strcmp0 (field, "tags"))
      fields |= MELO_BROWSER_JSONRPC_LIST_FIELDS_TAGS;
    else if (!g_strcmp0 (field, "type"))
      fields |= MELO_BROWSER_JSONRPC_LIST_FIELDS_TYPE;
    else if (!g_strcmp0 (field, "actions"))
      fields |= MELO_BROWSER_JSONRPC_LIST_FIELDS_ACTIONS;
  }

  return fields;
}

JsonObject *
melo_browser_jsonrpc_list_to_object (const MeloBrowserList *list,
                                     MeloBrowserJSONRPCListFields fields,
                                     MeloTagsFields tags_fields)
{
  JsonArray *array;
  JsonObject *object;
  const GList *l;

  /* Create object */
  object = json_object_new ();
  if (!object)
    return NULL;

  /* Add list properties */
  json_object_set_string_member (object, "path", list->path);
  json_object_set_int_member (object, "count", list->count);
  json_object_set_string_member (object, "prev_token", list->prev_token);
  json_object_set_string_member (object, "next_token", list->next_token);

  /* Parse list and create array */
  array = json_array_new ();
  for (l = list->items; l != NULL; l = l->next) {
    MeloBrowserItem *item = (MeloBrowserItem *) l->data;
    JsonObject *obj = json_object_new ();
    if (fields & MELO_BROWSER_JSONRPC_LIST_FIELDS_ID)
      json_object_set_string_member (obj, "id", item->id);
    if (fields & MELO_BROWSER_JSONRPC_LIST_FIELDS_NAME)
      json_object_set_string_member (obj, "name", item->name);
    if (fields & MELO_BROWSER_JSONRPC_LIST_FIELDS_TAGS) {
      if (item->tags) {
        JsonObject *tags = melo_tags_to_json_object (item->tags, tags_fields);
        json_object_set_object_member (obj, "tags", tags);
      } else
        json_object_set_null_member (obj, "tags");
    }
    if (fields & MELO_BROWSER_JSONRPC_LIST_FIELDS_TYPE) {
      json_object_set_string_member (obj, "type",
                                 melo_browser_item_type_to_string (item->type));
      if (item->type == MELO_BROWSER_ITEM_TYPE_CUSTOM)
        json_object_set_string_member (obj, "type_custom", item->type_custom);
    }
    if (fields & MELO_BROWSER_JSONRPC_LIST_FIELDS_ACTIONS) {
      JsonArray *actions;

      /* Generate action list */
      actions = json_array_new ();
      if (actions) {
        gint i;

        for (i = 0; i < MELO_BROWSER_ITEM_ACTION_COUNT; i++)
          if (item->actions & (1 << i))
            json_array_add_string_element (actions,
                                        melo_browser_item_action_to_string (i));

        json_object_set_array_member (obj, "actions", actions);
      }

      /* Generate custom action list */
      if (item->actions_custom) {
        actions = json_array_new ();
        if (actions) {
          JsonObject *o;

          while (item->actions_custom->id) {
            o = json_object_new ();
            if (o) {
              json_object_set_string_member (o, "id", item->actions_custom->id);
              json_object_set_string_member (o, "name",
                                             item->actions_custom->name);
              json_array_add_object_element (actions, o);
            }
            item->actions_custom++;
          }

          json_object_set_array_member (obj, "actions_custom", actions);
        }
      }
    }
    json_array_add_object_element (array, obj);
  }
  json_object_set_array_member (object, "items", array);

  return object;
}

static void
melo_browser_jsonrpc_get_tags_mode (JsonObject *obj, MeloBrowserTagsMode *mode,
                                    MeloTagsFields *fields)
{
  const gchar *mod;

  /* Check if tags node is available */
  if (!json_object_has_member (obj, "tags"))
    return;

  /* Get object node */
  obj = json_object_get_object_member (obj, "tags");
  if (!obj)
    return;

  /* Get tags mode string */
  mod = json_object_get_string_member (obj, "mode");
  if (!mod)
    return;

  /* Convert to MeloBrowserTagsMode */
  if (!g_strcmp0 (mod, "none_with_caching"))
    *mode = MELO_BROWSER_TAGS_MODE_NONE_WITH_CACHING;
  else if (!g_strcmp0 (mod, "only_cached"))
    *mode = MELO_BROWSER_TAGS_MODE_ONLY_CACHED;
  else if (!g_strcmp0 (mod, "full_cwith_caching"))
    *mode = MELO_BROWSER_TAGS_MODE_FULL_WITH_CACHING;
  else if (!g_strcmp0 (mod, "full"))
    *mode = MELO_BROWSER_TAGS_MODE_FULL;
  else
    *mode = MELO_BROWSER_TAGS_MODE_NONE;

  /* Get tags fields and convert */
  if (json_object_has_member (obj, "fields")) {
    JsonArray *array = json_object_get_array_member (obj, "fields");
    if (array)
      *fields = melo_tags_get_fields_from_json_array (array);
  }
}

/* Method callbacks */
static void
melo_browser_jsonrpc_get_info (const gchar *method,
                               JsonArray *s_params, JsonNode *params,
                               JsonNode **result, JsonNode **error,
                               gpointer user_data)
{
  MeloBrowserJSONRPCInfoFields fields = MELO_BROWSER_JSONRPC_INFO_FIELDS_NONE;
  const MeloBrowserInfo *info = NULL;
  MeloBrowser *bro;
  JsonObject *obj;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get browser from ID */
  bro = melo_browser_jsonrpc_get_browser (obj, error);
  if (!bro) {
    json_object_unref (obj);
    return;
  }

  /* Get fields */
  fields = melo_browser_jsonrpc_get_info_fields (obj, "fields");
  json_object_unref (obj);

  /* Generate list */
  info = melo_browser_get_info (bro);
  obj = melo_browser_jsonrpc_info_to_object (NULL, info, fields);
  g_object_unref (bro);

  /* Return result */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_browser_jsonrpc_get_list (const gchar *method,
                               JsonArray *s_params, JsonNode *params,
                               JsonNode **result, JsonNode **error,
                               gpointer user_data)
{
  MeloBrowserJSONRPCListFields fields;
  MeloBrowserTagsMode tags_mode = MELO_BROWSER_TAGS_MODE_NONE;
  MeloTagsFields tags_fields = MELO_TAGS_FIELDS_NONE;
  MeloSort sort = MELO_SORT_NONE;
  MeloBrowserList *list;
  MeloBrowser *bro;
  JsonObject *obj;
  GList *l;
  const gchar *path, *input;
  const gchar *token = NULL;
  gint offset, count;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get browser from ID */
  bro = melo_browser_jsonrpc_get_browser (obj, error);
  if (!bro) {
    json_object_unref (obj);
    return;
  }

  /* Get path */
  if (!g_strcmp0 (method, "browser.search"))
    input = json_object_get_string_member (obj, "input");
  else
    path = json_object_get_string_member (obj, "path");

  /* Get fields */
  fields = melo_browser_jsonrpc_get_list_fields (obj);

  /* Get list position */
  offset = json_object_get_int_member (obj, "offset");
  count = json_object_get_int_member (obj, "count");

  /* Get sort */
  if (json_object_has_member (obj, "sort"))
    sort = melo_sort_from_string (json_object_get_string_member (obj, "sort"));

  /* Get navigation token */
  if (json_object_has_member (obj, "token"))
    token = json_object_get_string_member (obj, "token");

  /* Get tags if needed */
  if (fields & MELO_BROWSER_JSONRPC_LIST_FIELDS_TAGS)
    melo_browser_jsonrpc_get_tags_mode (obj, &tags_mode, &tags_fields);

  /* Get browser list */
  if (!g_strcmp0 (method, "browser.search")) {
    MeloBrowserSearchParams params = {
      .offset = offset, .count = count, .sort = sort,
      .token = token, .tags_mode = tags_mode, .tags_fields = tags_fields,
    };

    list = melo_browser_search (bro, input, &params);
  } else {
    MeloBrowserGetListParams params = {
      .offset = offset, .count = count, .sort = sort,
      .token = token, .tags_mode = tags_mode, .tags_fields = tags_fields,
    };

    list = melo_browser_get_list (bro, path, &params);
  }
  json_object_unref (obj);
  g_object_unref (bro);

  /* No list provided */
  if (!list) {
    *error = melo_jsonrpc_build_error_node (MELO_JSONRPC_ERROR_INVALID_REQUEST,
                                            "Method not available!");
    return;
  }

  /* Create response with item list */
  obj = melo_browser_jsonrpc_list_to_object (list, fields, tags_fields);

  /* Free browser list */
  melo_browser_list_free (list);

  /* Return object */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_browser_jsonrpc_search_hint (const gchar *method,
                                  JsonArray *s_params, JsonNode *params,
                                  JsonNode **result, JsonNode **error,
                                  gpointer user_data)
{
  MeloBrowser *bro;
  JsonObject *obj;
  const gchar *input;
  gchar *hint;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get browser from ID */
  bro = melo_browser_jsonrpc_get_browser (obj, error);
  if (!bro) {
    json_object_unref (obj);
    return;
  }

  /* Get input */
  input = json_object_get_string_member (obj, "input");

  /* Get hint */
  hint = melo_browser_search_hint (bro, input);
  json_object_unref (obj);
  g_object_unref (bro);

  /* Create result object */
  obj = json_object_new ();
  json_object_set_string_member (obj, "hint", hint);
  g_free (hint);

  /* Return object */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_browser_jsonrpc_get_tags (const gchar *method,
                               JsonArray *s_params, JsonNode *params,
                               JsonNode **result, JsonNode **error,
                               gpointer user_data)
{
  MeloTagsFields fields = MELO_TAGS_FIELDS_FULL;
  MeloTags *tags = NULL;
  MeloBrowser *bro;
  JsonArray *array;
  JsonObject *obj;
  const gchar *path;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get browser from ID */
  bro = melo_browser_jsonrpc_get_browser (obj, error);
  if (!bro) {
    json_object_unref (obj);
    return;
  }

  /* Get path */
  path = json_object_get_string_member (obj, "path");

  /* Get fields */
  if (json_object_has_member (obj, "fields")) {
    /* Get tags fields array */
    array = json_object_get_array_member (obj, "fields");
    if (array)
      fields = melo_tags_get_fields_from_json_array (array);
  }

  /* Get tags from path */
  tags = melo_browser_get_tags (bro, path, fields);
  json_object_unref (obj);
  g_object_unref (bro);

  /* Parse list and create array */
  obj = melo_tags_to_json_object (tags, fields);
  if (tags)
    melo_tags_unref (tags);

  /* Return object */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_browser_jsonrpc_item_action (const gchar *method,
                                  JsonArray *s_params, JsonNode *params,
                                  JsonNode **result, JsonNode **error,
                                  gpointer user_data)
{
  const gchar *path;
  MeloBrowserActionParams action_params;
  MeloBrowserItemAction action;
  MeloBrowser *bro;
  JsonObject *obj;
  gboolean ret = FALSE;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get browser from ID */
  bro = melo_browser_jsonrpc_get_browser (obj, error);
  if (!bro) {
    json_object_unref (obj);
    return;
  }

  /* Get path */
  path = json_object_get_string_member (obj, "path");
  action = melo_browser_item_action_from_string (
                                 json_object_get_string_member (obj, "action"));

  /* Get token if available */
  if (json_object_has_member (obj, "token"))
    action_params.token = json_object_get_string_member (obj, "token");
  else
    action_params.token = NULL;

  /* Get sort */
  if (json_object_has_member (obj, "sort"))
    action_params.sort = melo_sort_from_string (
                                   json_object_get_string_member (obj, "sort"));
  else
    action_params.sort = MELO_SORT_NONE;

  /* Do action */
  ret = melo_browser_action (bro, path, action, &action_params);
  json_object_unref (obj);
  g_object_unref (bro);

  /* Create result object */
  obj = json_object_new ();
  json_object_set_boolean_member (obj, "done", ret);

  /* Return array */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

/* List of methods */
static MeloJSONRPCMethod melo_browser_jsonrpc_methods[] = {
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
    .callback = melo_browser_jsonrpc_get_info,
    .user_data = NULL,
  },
  {
    .method = "get_list",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"path\", \"type\": \"string\"},"
              "  {\"name\": \"offset\", \"type\": \"integer\"},"
              "  {\"name\": \"count\", \"type\": \"integer\"},"
              "  {\"name\": \"token\", \"type\": \"string\"},"
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"sort\", \"type\": \"string\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"tags\", \"type\": \"object\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_browser_jsonrpc_get_list,
    .user_data = NULL,
  },
  {
    .method = "search",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"input\", \"type\": \"string\"},"
              "  {\"name\": \"offset\", \"type\": \"integer\"},"
              "  {\"name\": \"count\", \"type\": \"integer\"},"
              "  {\"name\": \"token\", \"type\": \"string\"},"
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"sort\", \"type\": \"string\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"tags\", \"type\": \"object\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_browser_jsonrpc_get_list,
    .user_data = NULL,
  },
  {
    .method = "search_hint",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"input\", \"type\": \"string\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_browser_jsonrpc_search_hint,
    .user_data = NULL,
  },
  {
    .method = "get_tags",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"path\", \"type\": \"string\"},"
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_browser_jsonrpc_get_tags,
    .user_data = NULL,
  },
  {
    .method = "action",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"path\", \"type\": \"string\"},"
              "  {\"name\": \"action\", \"type\": \"string\"},"
              "  {"
              "    \"name\": \"sort\", \"type\": \"string\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"token\", \"type\": \"string\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_browser_jsonrpc_item_action,
    .user_data = NULL,
  },
};

/**
 * melo_browser_jsonrpc_register_methods:
 *
 * Register all JSON-RPC methods for #MeloBrowser.
 */
void
melo_browser_jsonrpc_register_methods (void)
{
  melo_jsonrpc_register_methods ("browser", melo_browser_jsonrpc_methods,
                                 G_N_ELEMENTS (melo_browser_jsonrpc_methods));
}

/**
 * melo_browser_jsonrpc_unregister_methods:
 *
 * Unregister all JSON-RPC methods for #MeloBrowser.
 */
void
melo_browser_jsonrpc_unregister_methods (void)
{
  melo_jsonrpc_unregister_methods ("browser", melo_browser_jsonrpc_methods,
                                   G_N_ELEMENTS (melo_browser_jsonrpc_methods));
}
