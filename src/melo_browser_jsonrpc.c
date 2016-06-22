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

#include "melo_browser.h"
#include "melo_jsonrpc.h"

typedef enum {
  MELO_BROWSER_JSONRPC_FIELDS_NONE = 0,
  MELO_BROWSER_JSONRPC_FIELDS_NAME = 1,
  MELO_BROWSER_JSONRPC_FIELDS_DESCRIPTION = 2,
  MELO_BROWSER_JSONRPC_FIELDS_FULL = 255,
} MeloBrowserJSONRPCFields;

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

static MeloBrowserJSONRPCFields
melo_browser_jsonrpc_get_fields (JsonObject *obj)
{
  MeloBrowserJSONRPCFields fields = MELO_BROWSER_JSONRPC_FIELDS_NONE;
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
      fields = MELO_BROWSER_JSONRPC_FIELDS_NONE;
      break;
    } else if (!g_strcmp0 (field, "full")) {
      fields = MELO_BROWSER_JSONRPC_FIELDS_FULL;
      break;
    } else if (!g_strcmp0 (field, "name"))
      fields |= MELO_BROWSER_JSONRPC_FIELDS_NAME;
    else if (!g_strcmp0 (field, "description"))
      fields |= MELO_BROWSER_JSONRPC_FIELDS_DESCRIPTION;
  }

  return fields;
}

static JsonObject *
melo_browser_jsonrpc_info_to_object (const gchar *id,
                                     const MeloBrowserInfo *info,
                                     MeloBrowserJSONRPCFields fields)
{
  JsonObject *obj = json_object_new ();
  if (id)
    json_object_set_string_member (obj, "id", id);
  if (info) {
    if (fields & MELO_BROWSER_JSONRPC_FIELDS_NAME)
      json_object_set_string_member (obj, "name", info->name);
    if (fields & MELO_BROWSER_JSONRPC_FIELDS_DESCRIPTION)
      json_object_set_string_member (obj, "description", info->description);
  }
  return obj;
}

/* Method callbacks */
static void
melo_browser_jsonrpc_get_info (const gchar *method,
                               JsonArray *s_params, JsonNode *params,
                               JsonNode **result, JsonNode **error,
                               gpointer user_data)
{
  MeloBrowserJSONRPCFields fields = MELO_BROWSER_JSONRPC_FIELDS_NONE;
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
  fields = melo_browser_jsonrpc_get_fields (obj);
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
  MeloBrowser *bro;
  JsonArray *array;
  JsonObject *obj;
  GList *list, *l;
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

  /* Get list */
  list = melo_browser_get_list (bro, path);
  json_object_unref (obj);
  g_object_unref (bro);

  /* Parse list and create array */
  array = json_array_new ();
  for (l = list; l != NULL; l = l->next) {
    MeloBrowserItem *item = (MeloBrowserItem *) l->data;
    obj = json_object_new ();
    json_object_set_string_member (obj, "name", item->name);
    json_object_set_string_member (obj, "type", item->type);
    json_array_add_object_element (array, obj);
  }

  /* Free item list */
  g_list_free_full (list, (GDestroyNotify) melo_browser_item_free);

  /* Return array */
  *result = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (*result, array);
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
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"sort\", \"type\": \"object\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"array\"}",
    .callback = melo_browser_jsonrpc_get_list,
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
