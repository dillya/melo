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

#include "melo_module_jsonrpc.h"
#include "melo_browser_jsonrpc.h"
#include "melo_player_jsonrpc.h"

typedef enum {
  MELO_MODULE_JSONRPC_INFO_FIELDS_NONE = 0,
  MELO_MODULE_JSONRPC_INFO_FIELDS_NAME = 1,
  MELO_MODULE_JSONRPC_INFO_FIELDS_DESCRIPTION = 2,
  MELO_MODULE_JSONRPC_INFO_FIELDS_CONFIG_ID = 4,

  MELO_MODULE_JSONRPC_INFO_FIELDS_FULL = ~0,
} MeloModuleJSONRPCInfoFields;

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

static MeloModuleJSONRPCInfoFields
melo_module_jsonrpc_get_fields (JsonObject *obj)
{
  MeloModuleJSONRPCInfoFields fields = MELO_MODULE_JSONRPC_INFO_FIELDS_NONE;
  const gchar *field;
  JsonArray *array;
  guint count, i;

  /* Check if fields is available */
  if (!json_object_has_member (obj, "fields"))
    return MELO_MODULE_JSONRPC_INFO_FIELDS_NONE;

  /* Get fields array */
  array = json_object_get_array_member (obj, "fields");
  if (!array)
    return MELO_MODULE_JSONRPC_INFO_FIELDS_NONE;

  /* Parse array */
  count = json_array_get_length (array);
  for (i = 0; i < count; i++) {
    field = json_array_get_string_element (array, i);
    if (!field)
      break;
    if (!g_strcmp0 (field, "none")) {
      fields = MELO_MODULE_JSONRPC_INFO_FIELDS_NONE;
      break;
    } else if (!g_strcmp0 (field, "full")) {
      fields = MELO_MODULE_JSONRPC_INFO_FIELDS_FULL;
      break;
    } else if (!g_strcmp0 (field, "name"))
      fields |= MELO_MODULE_JSONRPC_INFO_FIELDS_NAME;
    else if (!g_strcmp0 (field, "description"))
      fields |= MELO_MODULE_JSONRPC_INFO_FIELDS_DESCRIPTION;
    else if (!g_strcmp0 (field, "config_id"))
      fields |= MELO_MODULE_JSONRPC_INFO_FIELDS_CONFIG_ID;
  }

  return fields;
}

static JsonObject *
melo_module_jsonrpc_info_to_object (const gchar *id, const MeloModuleInfo *info,
                                    MeloModuleJSONRPCInfoFields fields)
{
  JsonObject *obj = json_object_new ();
  if (id)
    json_object_set_string_member (obj, "id", id);
  if (info) {
    if (fields & MELO_MODULE_JSONRPC_INFO_FIELDS_NAME)
      json_object_set_string_member (obj, "name", info->name);
    if (fields & MELO_MODULE_JSONRPC_INFO_FIELDS_DESCRIPTION)
      json_object_set_string_member (obj, "description", info->description);
    if (fields & MELO_MODULE_JSONRPC_INFO_FIELDS_CONFIG_ID)
      json_object_set_string_member (obj, "config_id", info->config_id);
  }
  return obj;
}

static JsonArray *
melo_module_jsonrpc_browser_list_to_array (GList *list,
                                           MeloBrowserJSONRPCInfoFields fields)
{
  JsonArray *array;
  GList *l;

  /* Create array */
  array = json_array_new ();
  if (!array)
    return NULL;

  /* Generate object list and fill array */
  for (l = list; l != NULL; l = l->next) {
    MeloBrowser *bro = (MeloBrowser *) l->data;
    const MeloBrowserInfo *info;
    const gchar *id;
    JsonObject *obj;

    /* Get browser info and ID */
    info = melo_browser_get_info (bro);
    id = melo_browser_get_id (bro);

    /* Generate object with browser info */
    obj = melo_browser_jsonrpc_info_to_object (id, info, fields);

    /* Add object to array */
    json_array_add_object_element (array, obj);
  }

  return array;
}

static JsonArray *
melo_module_jsonrpc_player_list_to_array (GList *list,
                                          MeloPlayerJSONRPCInfoFields fields)
{
  JsonArray *array;
  GList *l;

  /* Create array */
  array = json_array_new ();
  if (!array)
    return NULL;

  /* Generate object list and fill array */
  for (l = list; l != NULL; l = l->next) {
    MeloPlayer *play = (MeloPlayer *) l->data;
    const MeloPlayerInfo *info;
    const gchar *id;
    JsonObject *obj;

    /* Get player info and ID */
    info = melo_player_get_info (play);
    id = melo_player_get_id (play);

    /* Generate object with player info */
    obj = melo_player_jsonrpc_info_to_object (id, info, fields);

    /* Add object to array */
    json_array_add_object_element (array, obj);
  }

  return array;
}

static JsonArray *
melo_module_jsonrpc_list_to_array (GList *list,
                                   MeloModuleJSONRPCInfoFields fields,
                                   MeloBrowserJSONRPCInfoFields bfields,
                                   MeloPlayerJSONRPCInfoFields pfields)
{
  JsonArray *array;
  GList *l;

  /* Create array */
  array = json_array_new ();
  if (!array)
    return NULL;

  /* Generate object list and fill array */
  for (l = list; l != NULL; l = l->next) {
    MeloModule *mod = (MeloModule *) l->data;
    const MeloModuleInfo *info;
    const gchar *id;
    JsonObject *obj;

    /* Get module info and ID */
    info = melo_module_get_info (mod);
    id = melo_module_get_id (mod);

    /* Generate object with module info */
    obj = melo_module_jsonrpc_info_to_object (id, info, fields);

    /* Add browser list to object */
    if (bfields != MELO_BROWSER_JSONRPC_INFO_FIELDS_NONE) {
      JsonArray *a = NULL;
      GList *list;

      /* Generate browser list */
      list = melo_module_get_browser_list (mod);
      if (list) {
        a = melo_module_jsonrpc_browser_list_to_array (list, fields);
        g_list_free_full (list, g_object_unref);
      }

      /* Add array to object */
      if (a)
        json_object_set_array_member (obj, "browser_list", a);
    }

    /* Add player list to object */
    if (pfields != MELO_PLAYER_JSONRPC_INFO_FIELDS_NONE) {
      JsonArray *a = NULL;
      GList *list;

      /* Generate player list */
      list = melo_module_get_player_list (mod);
      if (list) {
        a = melo_module_jsonrpc_player_list_to_array (list, fields);
        g_list_free_full (list, g_object_unref);
      }

      /* Add array to object */
      if (a)
        json_object_set_array_member (obj, "player_list", a);
    }

    /* Add object to array */
    json_array_add_object_element (array, obj);
  }

  return array;
}

/* Method callbacks */
static void
melo_module_jsonrpc_get_list (const gchar *method,
                              JsonArray *s_params, JsonNode *params,
                              JsonNode **result, JsonNode **error,
                              gpointer user_data)
{
  MeloModuleJSONRPCInfoFields fields = MELO_MODULE_JSONRPC_INFO_FIELDS_NONE;
  JsonArray *array;
  JsonObject *obj;
  GList *list;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get fields */
  fields = melo_module_jsonrpc_get_fields (obj);
  json_object_unref (obj);

  /* Get module list */
  list = melo_module_get_module_list ();

  /* Generate list */
  array = melo_module_jsonrpc_list_to_array (list, fields,
                                         MELO_BROWSER_JSONRPC_INFO_FIELDS_NONE,
                                         MELO_PLAYER_JSONRPC_INFO_FIELDS_NONE);

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
  MeloModuleJSONRPCInfoFields fields = MELO_MODULE_JSONRPC_INFO_FIELDS_NONE;
  const MeloModuleInfo *info = NULL;
  MeloModule *mod;
  JsonObject *obj;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get module from id */
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
  MeloBrowserJSONRPCInfoFields fields = MELO_BROWSER_JSONRPC_INFO_FIELDS_NONE;
  MeloModule *mod;
  JsonArray *array;
  JsonObject *obj;
  GList *list;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get module from id */
  mod = melo_module_jsonrpc_get_module (obj, error);
  if (!mod) {
    json_object_unref (obj);
    return;
  }

  /* Get fields */
  fields = melo_browser_jsonrpc_get_info_fields (obj, "fields");

  /* Get browser list */
  list = melo_module_get_browser_list (mod);
  json_object_unref (obj);
  g_object_unref (mod);

  /* Generate list */
  array = melo_module_jsonrpc_browser_list_to_array (list, fields);

  /* Free browser list */
  g_list_free_full (list, g_object_unref);

  /* Return result */
  *result = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (*result, array);
}

static void
melo_module_jsonrpc_get_player_list (const gchar *method,
                                     JsonArray *s_params, JsonNode *params,
                                     JsonNode **result, JsonNode **error,
                                     gpointer user_data)
{
  MeloPlayerJSONRPCInfoFields fields = MELO_PLAYER_JSONRPC_INFO_FIELDS_NONE;
  MeloModule *mod;
  JsonArray *array;
  JsonObject *obj;
  GList *list;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get module from id */
  mod = melo_module_jsonrpc_get_module (obj, error);
  if (!mod) {
    json_object_unref (obj);
    return;
  }

  /* Get fields */
  fields = melo_player_jsonrpc_get_info_fields (obj, "fields");

  /* Get player list */
  list = melo_module_get_player_list (mod);
  json_object_unref (obj);
  g_object_unref (mod);

  /* Generate list */
  array = melo_module_jsonrpc_player_list_to_array (list, fields);

  /* Free player list */
  g_list_free_full (list, g_object_unref);

  /* Return result */
  *result = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (*result, array);
}

static void
melo_module_jsonrpc_get_full_list (const gchar *method,
                                   JsonArray *s_params, JsonNode *params,
                                   JsonNode **result, JsonNode **error,
                                   gpointer user_data)
{
  MeloModuleJSONRPCInfoFields fields = MELO_MODULE_JSONRPC_INFO_FIELDS_NONE;
  MeloBrowserJSONRPCInfoFields bfields = MELO_BROWSER_JSONRPC_INFO_FIELDS_NONE;
  MeloPlayerJSONRPCInfoFields pfields = MELO_PLAYER_JSONRPC_INFO_FIELDS_NONE;
  JsonArray *array;
  JsonObject *obj;
  GList *list;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get fields */
  fields = melo_module_jsonrpc_get_fields (obj);
  bfields = melo_browser_jsonrpc_get_info_fields (obj, "browser_fields");
  pfields = melo_player_jsonrpc_get_info_fields (obj, "player_fields");
  json_object_unref (obj);

  /* Get module list */
  list = melo_module_get_module_list ();

  /* Generate full list */
  array = melo_module_jsonrpc_list_to_array (list, fields, bfields, pfields);

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
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"array\"}",
    .callback = melo_module_jsonrpc_get_browser_list,
    .user_data = NULL,
  },
  {
    .method = "get_player_list",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"},"
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"array\"}",
    .callback = melo_module_jsonrpc_get_player_list,
    .user_data = NULL,
  },
  {
    .method = "get_full_list",
    .params = "["
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"browser_fields\", \"type\": \"array\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"player_fields\", \"type\": \"array\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"array\"}",
    .callback = melo_module_jsonrpc_get_full_list,
    .user_data = NULL,
  },
};

/* Register / Unregister methods */
void
melo_module_jsonrpc_register_methods (void)
{
  melo_jsonrpc_register_methods ("module", melo_module_jsonrpc_methods,
                                 G_N_ELEMENTS (melo_module_jsonrpc_methods));
}

void
melo_module_jsonrpc_unregister_methods (void)
{
  melo_jsonrpc_unregister_methods ("module", melo_module_jsonrpc_methods,
                                   G_N_ELEMENTS (melo_module_jsonrpc_methods));
}
