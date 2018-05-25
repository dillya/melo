/*
 * melo_config_jsonrpc.c: Configuration JSON-RPC interface
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

#include "melo_config_jsonrpc.h"

/**
 * SECTION:melo_config_jsonrpc
 * @title: MeloConfigJsonRPC
 * @short_description: Basic JSON-RPC methods for Melo Configuratio
 *
 * Helper which implements all basic JSON-RPC methods for #MeloConfig.
 */

static MeloConfig *
melo_config_jsonrpc_get_config (JsonObject *obj, JsonNode **error)
{
  MeloConfig *cfg;
  const gchar *id;

  /* Get config by id */
  id = json_object_get_string_member (obj, "id");
  cfg = melo_config_get_config_by_id (id);
  if (cfg)
    return cfg;

  /* No config found */
  *error = melo_jsonrpc_build_error_node (MELO_JSONRPC_ERROR_INVALID_PARAMS,
                                          "No config found!");
  return NULL;
}

static void
melo_config_jsonrpc_set_member (JsonObject *obj, const gchar *member,
                                MeloConfigType type, MeloConfigValue val)
{
  switch (type) {
    case MELO_CONFIG_TYPE_BOOLEAN:
      json_object_set_boolean_member (obj, member, val._boolean);
      break;
    case MELO_CONFIG_TYPE_INTEGER:
      json_object_set_int_member (obj, member, val._integer);
      break;
    case MELO_CONFIG_TYPE_DOUBLE:
      json_object_set_double_member (obj, member, val._double);
      break;
    case MELO_CONFIG_TYPE_STRING:
      json_object_set_string_member (obj, member, val._string);
      break;
    default:
      val._integer = 0;
  };
}

static JsonArray *
melo_config_jsonrpc_gen_item_array (MeloConfigContext *context, gint item_count)
{
  MeloConfigItem *item;
  MeloConfigValue value;
  JsonArray *array;
  JsonObject *obj;

  /* Create and fill item array */
  array = json_array_sized_new (item_count);

  /* List all items */
  while (melo_config_next_item (context, &item, &value)) {
    /* Create new object for item */
    obj = json_object_new ();
    if (!obj)
      continue;

    /* Fill item */
    json_object_set_string_member (obj, "id", item->id);
    json_object_set_string_member (obj, "name", item->name);
    json_object_set_string_member (obj, "type",
                                   melo_config_type_to_string (item->type));
    json_object_set_string_member (obj, "element",
                                 melo_config_element_to_string (item->element));
    json_object_set_boolean_member (obj, "read_only",
                                    item->flags & MELO_CONFIG_FLAGS_READ_ONLY);
    if (item->flags & MELO_CONFIG_FLAGS_WRITE_ONLY)
      json_object_set_null_member (obj, "val");
    else
      melo_config_jsonrpc_set_member (obj, "val", item->type, value);

    /* Add to item array */
    json_array_add_object_element (array, obj);
  }

  return array;
}

static gpointer
melo_config_jsonrpc_gen_array (MeloConfigContext *context, gpointer user_data)
{
  const gchar *group_id = (const gchar *) user_data;
  const MeloConfigGroup *group;
  JsonArray *array;
  JsonObject *obj;
  gint group_count;
  gint item_count;

  /* Generate only array for group */
  if (group_id) {
    /* Find group from group ID */
    if (!melo_config_find_group (context, group_id, &group, &item_count))
      return NULL;
    return melo_config_jsonrpc_gen_item_array (context, item_count);
  }

  /* Get group count */
  group_count = melo_config_get_group_count (context);

  /* Create group array */
  array = json_array_sized_new (group_count);
  if (!array)
    return NULL;

  /* List all groups */
  while (melo_config_next_group (context, &group, &item_count)) {
    /* Create a new object for group */
    obj = json_object_new ();
    if (!obj)
      continue;

    /* Fill group */
    json_object_set_string_member (obj, "id", group->id);
    json_object_set_string_member (obj, "name", group->name);
    json_object_set_array_member (obj, "items",
                      melo_config_jsonrpc_gen_item_array (context, item_count));

    /* Add to group array */
    json_array_add_object_element (array, obj);
  }

  return array;
}

static gboolean
melo_config_jsonrpc_update_items (MeloConfigContext *context, JsonArray *array,
                                  gchar **error)
{
  MeloConfigItem *item;
  JsonObject *obj;
  const gchar *id;
  gint i, count;

  /* Get items count */
  count = json_array_get_length (array);
  if (!count)
    return TRUE;

  /* Parse all items in list */
  for (i = 0; i < count; i++) {
    /* Get item object */
    obj = json_array_get_object_element (array, i);
    if (!obj)
      return FALSE;

    /* Get item ID */
    id = json_object_get_string_member (obj, "id");
    if (!id) {
      if (error)
        *error = g_strdup ("No item ID provided!");
      return FALSE;
    }

    /* Find item */
    if (!melo_config_find_item (context, id, &item, NULL)) {
      if (error)
        *error = g_strdup_printf ("Item '%s' doesn't exist!", id);
      return FALSE;
    }

    /* Read only item */
    if (item->flags & MELO_CONFIG_FLAGS_READ_ONLY) {
      if (error)
        *error = g_strdup_printf ("Item '%s' is read only!", id);
      return FALSE;
    }

    /* Update value */
    switch (item->type) {
      case MELO_CONFIG_TYPE_BOOLEAN:
        melo_config_update_boolean (context,
                                   json_object_get_boolean_member (obj, "val"));
        break;
      case MELO_CONFIG_TYPE_INTEGER:
        melo_config_update_integer (context,
                                    json_object_get_int_member (obj, "val"));
        break;
      case MELO_CONFIG_TYPE_DOUBLE:
        melo_config_update_double (context,
                                   json_object_get_double_member (obj, "val"));
        break;
      case MELO_CONFIG_TYPE_STRING:
        melo_config_update_string (context,
                                   json_object_get_string_member (obj, "val"));
        break;
      default:
        return FALSE;
    }
  }

  return TRUE;
}

static gboolean
melo_config_jsonrpc_update (MeloConfigContext *context, gpointer user_data,
                            gchar **error)
{
  JsonObject *obj = (JsonObject *) user_data;
  JsonArray *array, *list;
  const gchar *id;
  gint i, count;

  /* Get groups list */
  array = json_object_get_array_member (obj, "list");
  if (!array)
    goto bad_request;

  /* Get groups count */
  count = json_array_get_length (array);
  if (!count)
    return TRUE;

  /* Parse all groups */
  for (i = 0; i < count; i++) {
    /* Get group object */
    obj = json_array_get_object_element (array, i);
    if (!obj)
      return FALSE;

    /* Get group ID and item list */
    id = json_object_get_string_member (obj, "id");
    list = json_object_get_array_member (obj, "list");
    if (!id || !list)
      goto bad_request;

    /* Find group */
    if (!melo_config_find_group (context, id, NULL, NULL))
      goto bad_request;

    /* Parse item list */
    if (!melo_config_jsonrpc_update_items (context, list, error))
      return FALSE;
  }

  return TRUE;

bad_request:
  if (error)
    *error = g_strdup ("Bad JSON-RPC request!");
  return FALSE;
}

/* Method callbacks */
static void
melo_config_jsonrpc_get (const gchar *method,
                         JsonArray *s_params, JsonNode *params,
                         JsonNode **result, JsonNode **error,
                         gpointer user_data)
{
  const gchar *group_id = NULL;
  JsonArray *array;
  JsonObject *obj;
  MeloConfig *cfg;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get config from ID */
  cfg = melo_config_jsonrpc_get_config (obj, error);
  if (!cfg) {
    json_object_unref (obj);
    return;
  }

  /* Get group ID */
  if (json_object_has_member (obj, "group"))
    group_id = json_object_get_string_member (obj, "group");

  /* Convert config to JSON */
  array = melo_config_parse (cfg, melo_config_jsonrpc_gen_array,
                             (gpointer) group_id);
  json_object_unref (obj);
  g_object_unref (cfg);
  if (!array) {
    *error = melo_jsonrpc_build_error_node (MELO_JSONRPC_ERROR_INVALID_PARAMS,
                                            "Invalid group!");
    return;
  }

  /* Return result */
  *result = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (*result, array);
}

static void
melo_config_jsonrpc_set (const gchar *method,
                         JsonArray *s_params, JsonNode *params,
                         JsonNode **result, JsonNode **error,
                         gpointer user_data)
{
  gchar *err_str = NULL;
  MeloConfig *cfg;
  JsonObject *obj;
  gboolean ret;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get config from ID */
  cfg = melo_config_jsonrpc_get_config (obj, error);
  if (!cfg) {
    json_object_unref (obj);
    return;
  }

  /* Convert config to JSON */
  ret = melo_config_update (cfg, melo_config_jsonrpc_update, (gpointer) obj,
                            &err_str);
  json_object_unref (obj);
  g_object_unref (cfg);

  /* Create response */
  obj = json_object_new ();
  json_object_set_boolean_member (obj, "done", ret);

  /* Set error message */
  if (err_str) {
    json_object_set_string_member (obj, "error", err_str);
    g_free (err_str);
  }

  /* Return result */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

/* List of methods */
static MeloJSONRPCMethod melo_config_jsonrpc_methods[] = {
  {
    .method = "get",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {"
              "    \"name\": \"group\", \"type\": \"string\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"array\"}",
    .callback = melo_config_jsonrpc_get,
    .user_data = NULL,
  },
  {
    .method = "set",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {\"name\": \"list\", \"type\": \"array\"}"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_config_jsonrpc_set,
    .user_data = NULL,
  },
};

/**
 * melo_config_jsonrpc_register_methods:
 *
 * Register all JSON-RPC methods for #MeloConfig.
 */
void
melo_config_jsonrpc_register_methods (void)
{
  melo_jsonrpc_register_methods ("config", melo_config_jsonrpc_methods,
                                 G_N_ELEMENTS (melo_config_jsonrpc_methods));
}

/**
 * melo_config_jsonrpc_unregister_methods:
 *
 * Unregister all JSON-RPC methods for #MeloConfig.
 */
void
melo_config_jsonrpc_unregister_methods (void)
{
  melo_jsonrpc_unregister_methods ("config", melo_config_jsonrpc_methods,
                                   G_N_ELEMENTS (melo_config_jsonrpc_methods));
}
