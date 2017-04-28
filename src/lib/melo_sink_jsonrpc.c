/*
 * melo_sink_jsonrpc.c: Global audio sink JSON-RPC interface
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

#include "melo_sink_jsonrpc.h"

typedef enum {
  MELO_SINK_JSONRPC_FIELDS_NONE = 0,
  MELO_SINK_JSONRPC_FIELDS_ID,
  MELO_SINK_JSONRPC_FIELDS_NAME,
  MELO_SINK_JSONRPC_FIELDS_VOLUME,
  MELO_SINK_JSONRPC_FIELDS_MUTE,
  MELO_SINK_JSONRPC_FIELDS_SAMPLERATE,
  MELO_SINK_JSONRPC_FIELDS_CHANNELS,

  MELO_SINK_JSONRPC_FIELDS_FULL = ~0
} MeloSinkJSONRPCFields;

static MeloSink *
melo_sink_jsonrpc_get_sink (JsonObject *obj, JsonNode **error)
{
  MeloSink *sink;
  const gchar *id;

  /* Get sink by id */
  id = json_object_get_string_member (obj, "id");
  sink = melo_sink_get_sink_by_id (id);
  if (sink)
    return sink;

  /* No sink found */
  *error = melo_jsonrpc_build_error_node (MELO_JSONRPC_ERROR_INVALID_PARAMS,
                                          "No sink found!");
  return NULL;
}

MeloSinkJSONRPCFields
melo_sink_jsonrpc_get_fields (JsonObject *obj, const gchar *name)
{
  MeloSinkJSONRPCFields fields = MELO_SINK_JSONRPC_FIELDS_NONE;
  const gchar *field;
  JsonArray *array;
  guint count, i;

  /* Check if fields is available */
  if (!json_object_has_member (obj, name))
    return MELO_SINK_JSONRPC_FIELDS_FULL;

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
      fields = MELO_SINK_JSONRPC_FIELDS_NONE;
      break;
    } else if (!g_strcmp0 (field, "full")) {
      fields = MELO_SINK_JSONRPC_FIELDS_FULL;
      break;
    } else if (!g_strcmp0 (field, "id"))
      fields |= MELO_SINK_JSONRPC_FIELDS_ID;
    else if (!g_strcmp0 (field, "name"))
      fields |= MELO_SINK_JSONRPC_FIELDS_NAME;
    else if (!g_strcmp0 (field, "volume"))
      fields |= MELO_SINK_JSONRPC_FIELDS_VOLUME;
    else if (!g_strcmp0 (field, "mute"))
      fields |= MELO_SINK_JSONRPC_FIELDS_MUTE;
    else if (!g_strcmp0 (field, "samplerate"))
      fields |= MELO_SINK_JSONRPC_FIELDS_SAMPLERATE;
    else if (!g_strcmp0 (field, "channels"))
      fields |= MELO_SINK_JSONRPC_FIELDS_CHANNELS;
  }

  return fields;
}

static JsonObject *
melo_sink_jsonrpc_main_to_object (MeloSinkJSONRPCFields fields)
{
  gint rate, channels;
  JsonObject *obj;

  /* Get main output settings */
  melo_sink_get_main_config (&rate, &channels);

  /* Generate object */
  obj = json_object_new ();
  if (obj) {
    if (fields & MELO_SINK_JSONRPC_FIELDS_ID)
      json_object_set_string_member (obj, "id", "main");
    if (fields & MELO_SINK_JSONRPC_FIELDS_VOLUME)
      json_object_set_double_member (obj, "volume",
                                     melo_sink_get_main_volume ());
    if (fields & MELO_SINK_JSONRPC_FIELDS_MUTE)
      json_object_set_boolean_member (obj, "mute", melo_sink_get_main_mute ());
    if (fields & MELO_SINK_JSONRPC_FIELDS_SAMPLERATE)
      json_object_set_int_member (obj, "samplerate", rate);
    if (fields & MELO_SINK_JSONRPC_FIELDS_CHANNELS)
      json_object_set_int_member (obj, "channels", channels);
  }

  return obj;
}

static JsonObject *
melo_sink_jsonrpc_sink_to_object (MeloSink *sink, MeloSinkJSONRPCFields fields)
{
  JsonObject *obj;

  /* Generate object */
  obj = json_object_new ();
  if (obj) {
    if (fields & MELO_SINK_JSONRPC_FIELDS_ID)
      json_object_set_string_member (obj, "id", melo_sink_get_id (sink));
    if (fields & MELO_SINK_JSONRPC_FIELDS_NAME)
      json_object_set_string_member (obj, "name", melo_sink_get_name (sink));
    if (fields & MELO_SINK_JSONRPC_FIELDS_VOLUME)
      json_object_set_double_member (obj, "volume",
                                     melo_sink_get_volume (sink));
    if (fields & MELO_SINK_JSONRPC_FIELDS_MUTE)
      json_object_set_boolean_member (obj, "mute", melo_sink_get_mute (sink));
  }

  return obj;
}

/* Method callbacks */
static void
melo_sink_jsonrpc_get_list (const gchar *method,
                            JsonArray *s_params, JsonNode *params,
                            JsonNode **result, JsonNode **error,
                            gpointer user_data)
{
  MeloSinkJSONRPCFields fields;
  JsonArray *array;
  JsonObject *obj;
  GList *list, *l;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get fields */
  fields = melo_sink_jsonrpc_get_fields (obj, "fields");
  json_object_unref (obj);

  /* Create a new array */
  array = json_array_new ();
  if (!array)
    return;

  /* Add main settings */
  obj = melo_sink_jsonrpc_main_to_object (fields);
  if (obj)
    json_array_add_object_element (array, obj);

  /* Get sink list */
  list = melo_sink_get_sink_list ();

  /* Generate sink list */
  for (l = list; l != NULL; l = l->next) {
    MeloSink *sink = (MeloSink *) l->data;

    /* Generate object */
    obj = melo_sink_jsonrpc_sink_to_object (sink, fields);
    if (!obj)
      continue;

    /* Add item to array */
    json_array_add_object_element (array, obj);
  }

  /* Free sink list */
  g_list_free_full (list, (GDestroyNotify) g_object_unref);

  /* Return result */
  *result = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (*result, array);
}

static void
melo_sink_jsonrpc_get (const gchar *method,
                       JsonArray *s_params, JsonNode *params,
                       JsonNode **result, JsonNode **error,
                       gpointer user_data)
{
  MeloSinkJSONRPCFields fields;
  JsonObject *obj;
  MeloSink *sink;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get fields */
  fields = melo_sink_jsonrpc_get_fields (obj, "fields");

  /* Get main properties */
  if (!g_strcmp0 (json_object_get_string_member (obj, "id"), "main")) {
    json_object_unref (obj);
    obj = melo_sink_jsonrpc_main_to_object (fields);
    goto end;
  }

  /* Get sink from ID */
  sink = melo_sink_jsonrpc_get_sink (obj, error);
  json_object_unref (obj);
  if (!sink)
    return;

  /* Get values from sink */
  obj = melo_sink_jsonrpc_sink_to_object (sink, fields);
  g_object_unref (sink);
  if (!obj)
    return;

end:
  /* Return result */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

static void
melo_sink_jsonrpc_set (const gchar *method,
                       JsonArray *s_params, JsonNode *params,
                       JsonNode **result, JsonNode **error,
                       gpointer user_data)
{
  MeloSink *sink = NULL;
  JsonObject *obj;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get sink from ID (NULL if ID = main) */
  if (g_strcmp0 (json_object_get_string_member (obj, "id"), "main"))
    sink = melo_sink_jsonrpc_get_sink (obj, error);

  /* Set volume */
  if (json_object_has_member (obj, "volume")) {
    gdouble volume = json_object_get_double_member (obj, "volume");
    volume = melo_sink_set_volume (sink, volume);
    json_object_set_double_member (obj, "volume", volume);
  }

  /* Set mute */
  if (json_object_has_member (obj, "mute")) {
    gboolean mute = json_object_get_boolean_member (obj, "mute");
    mute = melo_sink_set_mute (sink, mute);
    json_object_set_boolean_member (obj, "mute", mute);
  }

  /* Unref sink */
  if (sink)
    g_object_unref (sink);

  /* Return result */
  *result = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (*result, obj);
}

/* List of methods */
static MeloJSONRPCMethod melo_sink_jsonrpc_methods[] = {
  {
    .method = "get_list",
    .params = "["
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"array\"}",
    .callback = melo_sink_jsonrpc_get_list,
    .user_data = NULL,
  },
  {
    .method = "get",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"}"
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_sink_jsonrpc_get,
    .user_data = NULL,
  },
  {
    .method = "set",
    .params = "["
              "  {\"name\": \"id\", \"type\": \"string\"}"
              "  {"
              "    \"name\": \"volume\", \"type\": \"double\","
              "    \"required\": false"
              "  },"
              "  {"
              "    \"name\": \"mute\", \"type\": \"boolean\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"object\"}",
    .callback = melo_sink_jsonrpc_set,
    .user_data = NULL,
  },
};

/* Register / Unregister methods */
void
melo_sink_jsonrpc_register_methods (void)
{
  melo_jsonrpc_register_methods ("sink", melo_sink_jsonrpc_methods,
                                 G_N_ELEMENTS (melo_sink_jsonrpc_methods));
}

void
melo_sink_jsonrpc_unregister_methods (void)
{
  melo_jsonrpc_unregister_methods ("sink", melo_sink_jsonrpc_methods,
                                   G_N_ELEMENTS (melo_sink_jsonrpc_methods));
}
