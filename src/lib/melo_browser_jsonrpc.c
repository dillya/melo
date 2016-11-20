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

MeloBrowserJSONRPCInfoFields
melo_browser_jsonrpc_get_info_fields (JsonObject *obj)
{
  MeloBrowserJSONRPCInfoFields fields = MELO_BROWSER_JSONRPC_INFO_FIELDS_NONE;
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
    } else if (!g_strcmp0 (field, "name"))
      fields |= MELO_BROWSER_JSONRPC_LIST_FIELDS_NAME;
    else if (!g_strcmp0 (field, "full_name"))
      fields |= MELO_BROWSER_JSONRPC_LIST_FIELDS_FULL_NAME;
    else if (!g_strcmp0 (field, "type"))
      fields |= MELO_BROWSER_JSONRPC_LIST_FIELDS_TYPE;
    else if (!g_strcmp0 (field, "cmds"))
      fields |= MELO_BROWSER_JSONRPC_LIST_FIELDS_CMDS;
    else if (!g_strcmp0 (field, "tags"))
      fields |= MELO_BROWSER_JSONRPC_LIST_FIELDS_TAGS;
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
  json_object_set_int_member (object, "count", list->count);

  /* Parse list and create array */
  array = json_array_new ();
  for (l = list->items; l != NULL; l = l->next) {
    MeloBrowserItem *item = (MeloBrowserItem *) l->data;
    JsonObject *obj = json_object_new ();
    if (fields & MELO_BROWSER_JSONRPC_LIST_FIELDS_NAME)
      json_object_set_string_member (obj, "name", item->name);
    if (fields & MELO_BROWSER_JSONRPC_LIST_FIELDS_FULL_NAME)
      json_object_set_string_member (obj, "full_name", item->full_name);
    if (fields & MELO_BROWSER_JSONRPC_LIST_FIELDS_TYPE)
      json_object_set_string_member (obj, "type", item->type);
    if (fields & MELO_BROWSER_JSONRPC_LIST_FIELDS_CMDS) {
      json_object_set_string_member (obj, "add", item->add);
      json_object_set_string_member (obj, "remove", item->remove);
    }
    if (fields & MELO_BROWSER_JSONRPC_LIST_FIELDS_TAGS) {
      if (item->tags) {
        JsonObject *tags = melo_tags_to_json_object (item->tags, tags_fields);
        json_object_set_object_member (obj, "tags", tags);
      } else
        json_object_set_null_member (obj, "tags");
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
  fields = melo_browser_jsonrpc_get_info_fields (obj);
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
  MeloBrowserList *list;
  MeloBrowser *bro;
  JsonObject *obj;
  GList *l;
  const gchar *path, *input;
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

  /* Get tags if needed */
  if (fields & MELO_BROWSER_JSONRPC_LIST_FIELDS_TAGS)
    melo_browser_jsonrpc_get_tags_mode (obj, &tags_mode, &tags_fields);

  /* Get browser list */
  if (!g_strcmp0 (method, "browser.search"))
    list = melo_browser_search (bro, input, offset, count, tags_mode,
                                tags_fields);
  else
    list = melo_browser_get_list (bro, path, offset, count, tags_mode,
                                  tags_fields);
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

  /* Do action on item */
  if (!g_strcmp0 (method, "browser.play"))
    ret = melo_browser_play (bro, path);
  else if (!g_strcmp0 (method, "browser.add"))
    ret = melo_browser_add (bro, path);
  else if (!g_strcmp0 (method, "browser.remove"))
    ret = melo_browser_remove (bro, path);
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
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"sort\", \"type\": \"object\","
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
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"sort\", \"type\": \"object\","
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
    .method = "play",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"path\", \"type\": \"string\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_browser_jsonrpc_item_action,
    .user_data = NULL,
  },
  {
    .method = "add",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"path\", \"type\": \"string\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_browser_jsonrpc_item_action,
    .user_data = NULL,
  },
  {
    .method = "remove",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"path\", \"type\": \"string\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_browser_jsonrpc_item_action,
    .user_data = NULL,
  },
};

/* Register / Unregister methods */
void
melo_browser_register_methods (void)
{
  melo_jsonrpc_register_methods ("browser", melo_browser_jsonrpc_methods,
                                 G_N_ELEMENTS (melo_browser_jsonrpc_methods));
}

void
melo_browser_unregister_methods (void)
{
  melo_jsonrpc_unregister_methods ("browser", melo_browser_jsonrpc_methods,
                                   G_N_ELEMENTS (melo_browser_jsonrpc_methods));
}
