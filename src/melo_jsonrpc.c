/*
 * melo_json_rpc.c: JSON-RPC server helpers
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

#include <string.h>
#include <json-glib/json-glib.h>

#include "melo_jsonrpc.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

struct _MeloJSONRPCPrivate {
  /* Final response node */
  JsonNode *root;

  /* Current error and/or result for callback */
  JsonNode *current_error;
  JsonNode *current_result;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloJSONRPC, melo_jsonrpc, G_TYPE_OBJECT)

static JsonNode *melo_jsonrpc_build_error_node (const char *id, gint64 nid,
                                                MeloJSONRPCError error_code,
                                                const char *error_format, ...);

static void
melo_jsonrpc_finalize (GObject *gobject)
{
  MeloJSONRPCPrivate *priv = melo_jsonrpc_get_instance_private (
                                                       (MeloJSONRPC *) gobject);
  /* Free final response */
  if (priv->root)
    json_node_free (priv->root);

  G_OBJECT_CLASS (melo_jsonrpc_parent_class)->finalize (gobject);
}

static void
melo_jsonrpc_class_init (MeloJSONRPCClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  /* Set private finalize */
  gobject_class->finalize = melo_jsonrpc_finalize;
}

static void
melo_jsonrpc_init (MeloJSONRPC *self)
{
  MeloJSONRPCPrivate *priv = melo_jsonrpc_get_instance_private (self);

  /* Get private data */
  self->priv = priv;

  /* Not response at init */
  priv->root = NULL;
}

MeloJSONRPC *
melo_jsonrpc_new (void)
{
  return g_object_new (MELO_TYPE_JSONRPC, NULL);
}

static JsonNode *
melo_jsonrpc_parse_node (MeloJSONRPCPrivate *priv, JsonNode *node,
                         MeloJSONRPCCallback callback, gpointer user_data)
{
  JsonBuilder *builder;
  JsonObject *obj;
  JsonNode *params;
  GVariant *gvar_params = NULL;
  const char *version;
  const char *method;
  const char *id = NULL;
  gint64 nid = -1;

  /* Not an object */
  if (JSON_NODE_TYPE (node) != JSON_NODE_OBJECT)
    goto invalid;

  /* Create a new reader */
  obj = json_node_get_object (node);
  if (!obj)
    goto internal;

  /* Check if jsonrpc is present */
  if (!json_object_has_member (obj, "jsonrpc"))
    goto invalid;

  /* Check JSON-RPC version */
  version = json_object_get_string_member (obj, "jsonrpc");
  if (!version || strcmp (version, "2.0"))
    goto invalid;

  /* Check if method is present */
  if (!json_object_has_member (obj, "method"))
    goto invalid;

  /* Get method */
  method = json_object_get_string_member (obj, "method");
  if (!method)
    goto invalid;

  /* Check if id is present */
  if (!json_object_has_member (obj, "id"))
    return NULL;

  /* Get id */
  nid = json_object_get_int_member (obj, "id");
  id = json_object_get_string_member (obj, "id");

  /* Get params */
  params = json_object_get_member (obj, "params");
  if (params)
    gvar_params = json_gvariant_deserialize (params, NULL, NULL);

  /* No callback provided */
  if (!callback) {
    if (gvar_params)
      g_variant_unref (gvar_params);
    goto not_found;
  }

  /* Call user callback */
  priv->current_error = NULL;
  priv->current_result = NULL;
  callback (method, gvar_params, user_data);

  /* Free GVariant */
  if (gvar_params)
    g_variant_unref (gvar_params);

  /* No error or result */
  if (!priv->current_error && !priv->current_result)
    goto not_found;

  /* Create new builder */
  builder = json_builder_new ();
  if (!builder)
    goto internal;

  /* Begin a new object */
  json_builder_begin_object (builder);

  /* Add jsonrpc member */
  json_builder_set_member_name (builder, "jsonrpc");
  json_builder_add_string_value (builder, "2.0");

  /* Set result or error */
  if (priv->current_error) {
    /* Add error member */
    json_builder_set_member_name (builder, "error");
    json_builder_add_value (builder, priv->current_error);

    /* Free response if exists */
    if (priv->current_result)
      json_node_free (priv->current_result);
  } else if (priv->current_result) {
    /* Add result member */
    json_builder_set_member_name (builder, "result");
    json_builder_add_value (builder, priv->current_result);
  }

  /* Add id member: we assume ID cannot be negative */
  json_builder_set_member_name (builder, "id");
  if (nid < 0 || id)
    json_builder_add_string_value (builder, id);
  else
    json_builder_add_int_value (builder, nid);

  json_builder_end_object (builder);

  /* Get final object */
  node = json_builder_get_root (builder);

  /* Free objects */
  g_object_unref (builder);

  return node;

invalid:
  return melo_jsonrpc_build_error_node (NULL, -1,
                                        MELO_JSONRPC_ERROR_INVALID_REQUEST,
                                        "Invalid request");
not_found:
  return melo_jsonrpc_build_error_node (id, nid,
                                        MELO_JSONRPC_ERROR_METHOD_NOT_FOUND,
                                        "Method not found");
internal:
  return melo_jsonrpc_build_error_node (id, -1,
                                        MELO_JSONRPC_ERROR_INTERNAL_ERROR,
                                        "Internal error");
}

gboolean
melo_jsonrpc_parse_request (MeloJSONRPC *self,
                            const char *request, gsize length,
                            MeloJSONRPCCallback callback,
                            gpointer user_data)
{
  MeloJSONRPCPrivate *priv = self->priv;
  JsonParser *parser;
  JsonNodeType type;
  JsonNode *root;
  GError *err = NULL;

  /* Free previous final node */
  if (priv->root)
    json_node_free (priv->root);

  /* Create parser */
  parser = json_parser_new ();
  if (!parser) {
    priv->root = melo_jsonrpc_build_error_node (
                                              NULL, -1,
                                              MELO_JSONRPC_ERROR_INTERNAL_ERROR,
                                              "Internal error");
    return FALSE;
  }

  /* Parse request */
  if (!json_parser_load_from_data (parser, request, length, &err) ||
      (root = json_parser_get_root (parser)) == NULL) {
    g_clear_error (&err);
    g_object_unref (parser);
    priv->root = melo_jsonrpc_build_error_node (NULL, -1,
                                                MELO_JSONRPC_ERROR_PARSE_ERROR,
                                                "Parse error");
    return FALSE;
  }

  /* Get node type */
  type = json_node_get_node_type (root);

  /* Parse node */
  if (type == JSON_NODE_OBJECT) {
    /* Parse single request */
    priv->root = melo_jsonrpc_parse_node (priv, root, callback, user_data);
  } else if (type == JSON_NODE_ARRAY) {
    /* Parse multiple requests: batch */
    JsonArray *req_array;
    JsonArray *res_array;
    JsonNode *node;
    guint count, i;

    /* Get array from node */
    req_array = json_node_get_array (root);
    count = json_array_get_length (req_array);
    if (!count) {
      /* Invalid request */
      g_object_unref (parser);
      priv->root = melo_jsonrpc_build_error_node (
                                             NULL, -1,
                                             MELO_JSONRPC_ERROR_INVALID_REQUEST,
                                             "Invalid request");
      return FALSE;
    }

    /* Create a new array for response */
    res_array = json_array_sized_new (count);
    priv->root = json_node_new (JSON_NODE_ARRAY);
    json_node_take_array (priv->root, res_array);

    /* Parse each elements of array */
    for (i = 0; i < count; i++) {
      /* Get element */
      node = json_array_get_element (req_array, i);

      /* Process requesit */
      node = melo_jsonrpc_parse_node (priv, node, callback, user_data);

      /* Add new response to array */
      if (node)
        json_array_add_element (res_array, node);
    }
  } else {
    /* Invalid request */
    g_object_unref (parser);
    priv->root = melo_jsonrpc_build_error_node (
                                             NULL, -1,
                                             MELO_JSONRPC_ERROR_INVALID_REQUEST,
                                             "Invalid request");
    return FALSE;
  }

  g_object_unref (parser);
  return TRUE;
}

void
melo_jsonrpc_set_response (MeloJSONRPC *self, GVariant *variant)
{
  /* Transform GVariant to JsonNode */
  self->priv->current_result = json_gvariant_serialize (variant);
}

static void
melo_jsonrpc_set_errorv (MeloJSONRPC *self, MeloJSONRPCError error_code,
                         const char *error_format, va_list args)
{
  JsonBuilder *builder;
  char *error_message;

  /* Generate error message */
  error_message = g_strdup_vprintf (error_format, args);

  /* Create a new JSON builder */
  builder = json_builder_new ();

  /* Create error object */
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "code");
  json_builder_add_int_value (builder, error_code);
  json_builder_set_member_name (builder, "message");
  json_builder_add_string_value (builder, error_message);
  json_builder_end_object (builder);

  /* Get final object */
  self->priv->current_error = json_builder_get_root (builder);

  /* Free objects */
  g_object_unref (builder);
  g_free (error_message);
}

void
melo_jsonrpc_set_error (MeloJSONRPC *self, MeloJSONRPCError error_code,
                        const char *error_format, ...)
{
  va_list args;

  /* Generate message error */
  va_start (args, error_format);
  melo_jsonrpc_set_errorv (self, error_code, error_format, args);
  va_end (args);
}

char *
melo_jsonrpc_get_response (MeloJSONRPC *self)
{
  JsonGenerator *gen;
  char *str;

  /* No root built */
  if (!self->priv->root)
    return melo_jsonrpc_build_error (NULL, MELO_JSONRPC_ERROR_INTERNAL_ERROR,
                                     "Internal error");

  /* Generate final string */
  gen = json_generator_new ();
  json_generator_set_root (gen, self->priv->root);
  str = json_generator_to_data (gen, NULL);

  /* Free objects */
  g_object_unref (gen);

  return str;
}

/* Utils */
static JsonNode *
melo_jsonrpc_build_errorv (const char *id, gint64 nid,
                           MeloJSONRPCError error_code,
                           const char *error_format, va_list args)
{
  JsonBuilder *builder;
  JsonNode *node;
  char *error_message;

  /* Generate error message */
  error_message = g_strdup_vprintf (error_format, args);

  /* Create a new JSON builder */
  builder = json_builder_new ();

  /* Begin a new object */
  json_builder_begin_object (builder);

  /* Set required jsonrpc version field */
  json_builder_set_member_name (builder, "jsonrpc");
  json_builder_add_string_value (builder, "2.0");

  /* Create error object */
  json_builder_set_member_name (builder, "error");
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "code");
  json_builder_add_int_value (builder, error_code);
  json_builder_set_member_name (builder, "message");
  json_builder_add_string_value (builder, error_message);
  json_builder_end_object (builder);

  /* Set id */
  json_builder_set_member_name (builder, "id");
  if (nid < 0 || id)
    json_builder_add_string_value (builder, id);
  else
    json_builder_add_int_value (builder, nid);

  json_builder_end_object (builder);

  /* Get final object */
  node = json_builder_get_root (builder);

  /* Free objects */
  g_object_unref (builder);
  g_free (error_message);

  return node;
}

static JsonNode *
melo_jsonrpc_build_error_node (const char *id, gint64 nid,
                               MeloJSONRPCError error_code,
                               const char *error_format, ...)
{
  va_list args;
  JsonNode *node;

  va_start (args, error_format);
  node = melo_jsonrpc_build_errorv (id, nid, error_code, error_format, args);
  va_end (args);

  return node;
}

char *
melo_jsonrpc_build_error (const char *id, MeloJSONRPCError error_code,
                          const char *error_format, ...)
{
  JsonGenerator *gen;
  JsonNode *node;
  va_list args;
  char *str;

  va_start (args, error_format);
  node = melo_jsonrpc_build_errorv (id, -1, error_code, error_format, args);
  va_end (args);

  /* Generate final string */
  gen = json_generator_new ();
  json_generator_set_root (gen, node);
  str = json_generator_to_data (gen, NULL);

  /* Free objects */
  json_node_free (node);
  g_object_unref (gen);

  return str;
}
