/*
 * melo_module_jsonrpc.c: Module base JSON-RPC interface
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

#include "melo_module.h"
#include "melo_jsonrpc.h"

typedef enum {
  MELO_MODULE_JSONRPC_FIELDS_NONE = 0,
  MELO_MODULE_JSONRPC_FIELDS_NAME = 1,
  MELO_MODULE_JSONRPC_FIELDS_DESCRIPTION = 2,
  MELO_MODULE_JSONRPC_FIELDS_FULL = 255,
} MeloModuleJSONRPCFields;

static MeloModule *
melo_module_jsonrpc_get_module (JsonObject *obj, JsonNode **error)
{
  MeloModule *mod;
  const gchar *id;

  /* Get module by id */
  id = json_object_get_string_member (obj, "id");
  mod = melo_module_get_module_by_id (id);
  if (mod)
    return mod;

  /* No module found */
  *error = melo_jsonrpc_build_error_node (MELO_JSONRPC_ERROR_INVALID_PARAMS,
                                          "No module found!");
  return NULL;
}

static MeloModuleJSONRPCFields
melo_module_jsonrpc_get_fields (JsonObject *obj)
{
  MeloModuleJSONRPCFields fields = MELO_MODULE_JSONRPC_FIELDS_NONE;
  const gchar *field;
  JsonArray *array;
  guint count, i;

  /* Check if fields is available */
  if (!json_object_has_member (obj, "fields"))
    return MELO_MODULE_JSONRPC_FIELDS_NONE;

  /* Get fields array */
  array = json_object_get_array_member (obj, "fields");
  if (!array)
    return MELO_MODULE_JSONRPC_FIELDS_NONE;

  /* Parse array */
  count = json_array_get_length (array);
  for (i = 0; i < count; i++) {
    field = json_array_get_string_element (array, i);
    if (!field)
      break;
    if (!g_strcmp0 (field, "none")) {
      fields = MELO_MODULE_JSONRPC_FIELDS_NONE;
      break;
    } else if (!g_strcmp0 (field, "full")) {
      fields = MELO_MODULE_JSONRPC_FIELDS_FULL;
      break;
    } else if (!g_strcmp0 (field, "name"))
      fields |= MELO_MODULE_JSONRPC_FIELDS_NAME;
    else if (!g_strcmp0 (field, "description"))
      fields |= MELO_MODULE_JSONRPC_FIELDS_DESCRIPTION;
  }

  return fields;
}

static JsonObject *
melo_module_jsonrpc_info_to_object (const gchar *id, const MeloModuleInfo *info,
                                    MeloModuleJSONRPCFields fields)
{
  JsonObject *obj = json_object_new ();
  if (id)
    json_object_set_string_member (obj, "id", id);
  if (info) {
    if (fields & MELO_MODULE_JSONRPC_FIELDS_NAME)
      json_object_set_string_member (obj, "name", info->name);
    if (fields & MELO_MODULE_JSONRPC_FIELDS_DESCRIPTION)
      json_object_set_string_member (obj, "description", info->description);
  }
  return obj;
}

/* Method callbacks */
static void
melo_module_jsonrpc_get_list (const gchar *method,
                              JsonArray *s_params, JsonNode *params,
                              JsonNode **result, JsonNode **error,
                              gpointer user_data)
{
  MeloModuleJSONRPCFields fields = MELO_MODULE_JSONRPC_FIELDS_NONE;
  const MeloModuleInfo *info = NULL;
  MeloModule *mod;
  JsonArray *array;
  JsonObject *obj;
  const gchar *id;
  GList *list;
  GList *l;

  /* Get fields */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  fields = melo_module_jsonrpc_get_fields (obj);
  json_object_unref (obj);

  /* Get module list */
  list = melo_module_get_module_list ();

  /* Generate list */
  array = json_array_new ();
  for (l = list; l != NULL; l = l->next) {
    mod = (MeloModule *) l->data;
    id = melo_module_get_id (mod);
    if (fields != MELO_MODULE_JSONRPC_FIELDS_NONE)
      info = melo_module_get_info (mod);
    obj = melo_module_jsonrpc_info_to_object (id, info, fields);
    json_array_add_object_element (array, obj);
  }

  /* Free module list */
  g_list_free_full (list, g_object_unref);

  /* Return result */
  *result = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (*result, array);
}

static void
melo_module_jsonrpc_get_info (const gchar *method,
                              JsonArray *s_params, JsonNode *params,
                              JsonNode **result, JsonNode **error,
                              gpointer user_data)
{
  MeloModuleJSONRPCFields fields = MELO_MODULE_JSONRPC_FIELDS_NONE;
  const MeloModuleInfo *info = NULL;
  MeloModule *mod;
  JsonObject *obj;

  /* Get module from id */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  mod = melo_module_jsonrpc_get_module (obj, error);
  if (!mod) {
    json_object_unref (obj);
    return;
  }

  /* Get fields */
  fields = melo_module_jsonrpc_get_fields (obj);
  json_object_unref (obj);

  /* Generate list */
  info = melo_module_get_info (mod);
  obj = melo_module_jsonrpc_info_to_object (NULL, info, fields);
  g_object_unref (mod);

  /* Return result */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_module_jsonrpc_get_browser_list (const gchar *method,
                                      JsonArray *s_params, JsonNode *params,
                                      JsonNode **result, JsonNode **error,
                                      gpointer user_data)
{
  MeloBrowser *bro;
  MeloModule *mod;
  JsonArray *array;
  JsonObject *obj;
  const gchar *id;
  GList *list;
  GList *l;

  /* Get module from id */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  mod = melo_module_jsonrpc_get_module (obj, error);
  if (!mod) {
    json_object_unref (obj);
    return;
  }

  /* Get browser list */
  list = melo_module_get_browser_list (mod);
  json_object_unref (obj);
  g_object_unref (mod);

  /* Generate list */
  array = json_array_new ();
  for (l = list; l != NULL; l = l->next) {
    bro = (MeloBrowser *) l->data;
    id = melo_browser_get_id (bro);
    json_array_add_string_element (array, id);
  }

  /* Free browser list */
  g_list_free_full (list, g_object_unref);

  /* Return result */
  *result = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (*result, array);
}



/* List of methods */
static MeloJSONRPCMethod melo_module_jsonrpc_methods[] = {
  {
    .method = "get_list",
    .params = "["
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"array\"}",
    .callback = melo_module_jsonrpc_get_list,
    .user_data = NULL,
  },
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
    .callback = melo_module_jsonrpc_get_info,
    .user_data = NULL,
  },
  {
    .method = "get_browser_list",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"}"
              "]",
    .result = "{\"type\":\"array\"}",
    .callback = melo_module_jsonrpc_get_browser_list,
    .user_data = NULL,
  },
};

/* Register / Unregister methods */
void
melo_module_register_methods (void)
{
  melo_jsonrpc_register_methods ("module", melo_module_jsonrpc_methods,
                                 G_N_ELEMENTS (melo_module_jsonrpc_methods));
}

void
melo_module_unregister_methods (void)
{
  melo_jsonrpc_unregister_methods ("module", melo_module_jsonrpc_methods,
                                   G_N_ELEMENTS (melo_module_jsonrpc_methods));
}
