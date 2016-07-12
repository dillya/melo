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
melo_config_jsonrpc_gen_item_array (MeloConfigItem *items, gint count,
                                    MeloConfigValue *values)
{
  JsonArray *array;
  JsonObject *obj;
  gint i;

  /* Create new array */
  array = json_array_sized_new (count);
  if (!array)
    return NULL;

  /* Fill array with items */
  for (i = 0; i < count; i++) {
    if (items[i].flags & MELO_CONFIG_FLAGS_DONT_SHOW)
      continue;

    /* Create new object */
    obj = json_object_new ();
    if (!obj)
      continue;
    json_object_set_string_member (obj, "id", items[i].id);
    json_object_set_string_member (obj, "name", items[i].name);
    json_object_set_string_member (obj, "type",
                                   melo_config_type_to_string (items[i].type));
    json_object_set_string_member (obj, "element",
                              melo_config_element_to_string (items[i].element));
    melo_config_jsonrpc_set_member (obj, "val", items[i].type, values[i]);

    /* Add to array */
    json_array_add_object_element (array, obj);
  }

  return array;
}

static JsonArray *
melo_config_jsonrpc_gen_group_array (const MeloConfigGroup *groups, gint count,
                                     MeloConfigValues *values)
{
  JsonArray *array;
  JsonObject *obj;
  gint i;

  /* Create new array */
  array = json_array_sized_new (count);
  if (!array)
    return NULL;

  /* Fill array with items */
  for (i = 0; i < count; i++) {
    /* Create new object */
    obj = json_object_new ();
    if (!obj)
      continue;
    json_object_set_string_member (obj, "id", groups[i].id);
    json_object_set_string_member (obj, "name", groups[i].name);
    json_object_set_array_member (obj, "items",
                    melo_config_jsonrpc_gen_item_array (groups[i].items,
                                                        groups[i].items_count,
                                                        values[i].values));

    /* Add to array */
    json_array_add_object_element (array, obj);
  }

  return array;
}

static gpointer
melo_config_jsonrpc_read_cb (const MeloConfigGroup *groups, gint groups_count,
                             MeloConfigValues *values, gpointer user_data)
{
  const gchar *group_id = (const gchar *) user_data;
  gint i;

  /* Generate complete JSON array */
  if (!group_id)
    return melo_config_jsonrpc_gen_group_array (groups, groups_count, values);

  /* Find group and generate its item array */
  for (i = 0; i < groups_count; i++) {
    /* Group found */
    if (g_str_equal (groups[i].id, group_id)) {
      return melo_config_jsonrpc_gen_item_array (groups[i].items,
                                                 groups[i].items_count,
                                                 values[i].values);
    }
  }

  return NULL;
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
  if (!cfg)
    return;

  /* Get group ID */
  if (json_object_has_member (obj, "group"))
    group_id = json_object_get_string_member (obj, "group");

  /* Convert config to JSON */
  array = melo_config_read_all (cfg, melo_config_jsonrpc_read_cb,
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
};

/* Register / Unregister methods */
void
melo_config_register_methods (void)
{
  melo_jsonrpc_register_methods ("config", melo_config_jsonrpc_methods,
                                 G_N_ELEMENTS (melo_config_jsonrpc_methods));
}

void
melo_config_unregister_methods (void)
{
  melo_jsonrpc_unregister_methods ("config", melo_config_jsonrpc_methods,
                                   G_N_ELEMENTS (melo_config_jsonrpc_methods));
}
