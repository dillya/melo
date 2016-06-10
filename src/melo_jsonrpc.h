/*
 * melo_jsonrpc.h: JSON-RPC 2.0 Parser
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

#ifndef __MELO_JSONRPC_H__
#define __MELO_JSONRPC_H__

#include <glib.h>
#include <json-glib/json-glib.h>

/* JSON RPC error codes */
typedef enum {
  MELO_JSONRPC_ERROR_PARSE_ERROR = -32700,
  MELO_JSONRPC_ERROR_INVALID_REQUEST = -32600,
  MELO_JSONRPC_ERROR_METHOD_NOT_FOUND = -32601,
  MELO_JSONRPC_ERROR_INVALID_PARAMS = -32602,
  MELO_JSONRPC_ERROR_INTERNAL_ERROR = -32603,
  MELO_JSONRPC_ERROR_SERVER_ERROR = -32000,
} MeloJSONRPCError;

/* Callback for method */
typedef void (*MeloJSONRPCCallback) (const gchar *method,
                                     JsonArray *schema_params, JsonNode *params,
                                     JsonNode **result, JsonNode **error,
                                     gpointer user_data);

/* Method definition */
typedef struct _MeloJSONRPCMethod {
  /* Method name */
  const gchar *method;
  /* Params and result schemas */
  const gchar *params;
  const gchar *result;
  /* Method callback */
  MeloJSONRPCCallback callback;
  gpointer user_data;
} MeloJSONRPCMethod;

/* Register a JSON-RPC method */
gboolean melo_jsonrpc_register_method (const gchar *group, const gchar *method,
                                       JsonArray *params, JsonObject *result,
                                       MeloJSONRPCCallback callback,
                                       gpointer user_data);
void melo_jsonrpc_unregister_method (const gchar *group, const gchar *method);

/* Register an array of JSON-RPC methods */
guint melo_jsonrpc_register_methods (const gchar *group,
                                     MeloJSONRPCMethod *methods, guint count);
void melo_jsonrpc_unregister_methods (const gchar *group,
                                     MeloJSONRPCMethod *methods, guint count);

/* Parse a JSON-RPC request */
gchar *melo_jsonrpc_parse_request (const gchar *request, gsize length,
                                   GError **eror);

/* Parameters utils */
gboolean melo_jsonrpc_check_params (JsonArray *schema_params, JsonNode *params);
JsonArray *melo_jsonrpc_get_array (JsonArray *schema_params, JsonNode *params);
JsonObject *melo_jsonrpc_get_object (JsonArray *schema_params,
                                     JsonNode *params);

/* Utils */
JsonNode *melo_jsonrpc_build_error_node (MeloJSONRPCError error_code,
                                         const char *error_format, ...);

#endif /* __MELO_JSONRPC_H__ */
