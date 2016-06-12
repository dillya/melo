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

static MeloModuleJSONRPCFields
melo_module_jsonrpc_get_fields (JsonObject *obj)
{
  MeloModuleJSONRPCFields fields = MELO_MODULE_JSONRPC_FIELDS_NONE;
  const gchar *field;
  JsonArray *array;
  guint count, i;

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
  obj = melo_jsonrpc_get_object (s_params, params);
  if (obj) {
    fields = melo_module_jsonrpc_get_fields (obj);
    json_object_unref (obj);
  }

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
