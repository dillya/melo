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

/**
 * MeloJSONRPCError:
 * @MELO_JSONRPC_ERROR_PARSE_ERROR: parse error
 * @MELO_JSONRPC_ERROR_INVALID_REQUEST: invalid request
 * @MELO_JSONRPC_ERROR_METHOD_NOT_FOUND: method not found
 * @MELO_JSONRPC_ERROR_INVALID_PARAMS: invalid parameters
 * @MELO_JSONRPC_ERROR_INTERNAL_ERROR: internal error
 * @MELO_JSONRPC_ERROR_SERVER_ERROR: server error
 *
 * Default JSON-RPC error codes.
 */
typedef enum {
  MELO_JSONRPC_ERROR_PARSE_ERROR = -32700,
  MELO_JSONRPC_ERROR_INVALID_REQUEST = -32600,
  MELO_JSONRPC_ERROR_METHOD_NOT_FOUND = -32601,
  MELO_JSONRPC_ERROR_INVALID_PARAMS = -32602,
  MELO_JSONRPC_ERROR_INTERNAL_ERROR = -32603,
  MELO_JSONRPC_ERROR_SERVER_ERROR = -32000,
} MeloJSONRPCError;

/**
 * MeloJSONRPCCallback:
 * @method: the current method name
 * @schema_params: the parameters schema associated to the method, translated
 *    into a #JsonArray
 * @params: the current parameters presented as a #JsonNode
 * @result: a pointer to the result which is set when method execution is
 *    successful, otherwise, the @error should be set
 * @error: a pointer to the error which is set when method execution has failed,
 *    otherwise, it must be untouched and @result should be set. If none of
 *    @result and @error are set, a MELO_JSONRPC_ERROR_METHOD_NOT_FOUND is
 *    returned
 * @user_data: the user data provided during method registration with
 *    melo_jsonrpc_register_method()
 *
 * During a call to melo_jsonrpc_parse_request(), if the current method match
 * to one of the registered methods, the associated callback of type
 * #MeloJSONRPCCallback is called.
 *
 * The @params can be converted into a #JsonArray with melo_jsonrpc_get_array()
 * or into a #JsonObject with  melo_jsonrpc_get_object(). It uses the
 * @schema_params to present the #JsonNode as a more readable object.
 *
 * The @result or @error should be set before returning, in order to prevent a
 * default MELO_JSONRPC_ERROR_METHOD_NOT_FOUND error.
 */
typedef void (*MeloJSONRPCCallback) (const gchar *method,
                                     JsonArray *schema_params, JsonNode *params,
                                     JsonNode **result, JsonNode **error,
                                     gpointer user_data);

/**
 * MeloJSONRPCMethod:
 * @method: the method name
 * @params: the schema of the parameters accepted as JSON
 * @result: the schema of the result provided as JSON
 * @callback: the callback of type #MeloJSONRPCCallback to call when @method,
 *    @params and @result are matching
 * @user_data: the user data to use when calling @callback
 *
 * The #MeloJSONRPCMethod describe a JSON-RPC method to register in JSON-RPC
 * parser. The registration is done with melo_jsonrpc_register_method() or
 * melo_jsonrpc_register_methods().
 *
 * During registration, the @params and @result schemas are translated to two
 * #JsonArray and are used to check parameters and results are valid. If one
 * of them is not matching the schemas, the MELO_JSONRPC_ERROR_INVALID_PARAMS
 * error is returned and the callback @callback is not called.
 *
 * The @params schema must be presented as a JSON array containing multiples
 * JSON object describing each one parameter, in the order of presentation, with
 * the following members:
 *  - "name": the name of the parameter (a string)
 *  - "type": the type of the parameter (a string "string", "integer",
 *            "boolean", "double", "object", "array")
 *  - "required": the parameter is not optional (a boolean). This field can be
 *                omitted if the parameter is required
 *
 * Example of a @params definition:
 * |[<!-- language="JSON" -->
 * [
 *  { "name": "first_parameter",  "type": "string" },
 *  { "name": "second_parameter", "type": "boolean", "required": true  },
 *  { "name": "third_parameter",  "type": "array",   "required": false }
 * ]
 * ]|
 *
 * The @result schema must be presented as a JSON object containing the
 * following members:
 *  - "type": the expected result JSON type (a string "object" or "array")
 *
 * Example of a @result definition:
 * |[<!-- language="JSON" -->
 * { "type": "string" }
 * ]|
 *
 * If the @params or @result cannot be parsed, the registration function will
 * fail.
 */
typedef struct _MeloJSONRPCMethod {
  const gchar *method;
  const gchar *params;
  const gchar *result;
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
                                   GError **error);

/* Parameters utils */
gboolean melo_jsonrpc_check_params (JsonArray *schema_params, JsonNode *params,
                                    JsonNode **error);
JsonArray *melo_jsonrpc_get_array (JsonArray *schema_params, JsonNode *params,
                                   JsonNode **error);
JsonObject *melo_jsonrpc_get_object (JsonArray *schema_params,
                                     JsonNode *params, JsonNode **error);

/* Utils */
JsonNode *melo_jsonrpc_build_error_node (MeloJSONRPCError error_code,
                                         const char *error_format, ...);

#endif /* __MELO_JSONRPC_H__ */
