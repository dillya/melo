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
#include <libsoup/soup.h>

#include "melo_jsonrpc.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

char *
melo_jsonrpc_build_errorv (const char *id, MeloJSONRPCError error_code,
                           const char *error_format, va_list args)
{
  JsonBuilder *builder;
  JsonGenerator *gen;
  JsonNode * root;
  char *error_message;
  char *str;

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
  json_builder_add_string_value (builder, id);

  json_builder_end_object (builder);

  /* Get final object */
  root = json_builder_get_root (builder);

  /* Generate final string */
  gen = json_generator_new ();
  json_generator_set_root (gen, root);
  str = json_generator_to_data (gen, NULL);

  /* Free objects */
  json_node_free (root);
  g_object_unref (gen);
  g_object_unref (builder);
  g_free (error_message);

  return str;
}

char *
melo_jsonrpc_build_error (const char *id, MeloJSONRPCError error_code,
                          const char *error_format, ...)
{
  va_list args;
  char *str;

  va_start (args, error_format);
  str = melo_jsonrpc_build_errorv (id, error_code, error_format, args);
  va_end (args);

  return str;
}

void
melo_jsonrpc_message_set_error (SoupMessage *msg, const char *id,
                                MeloJSONRPCError error_code,
                                const char *error_format, ...)
{
  va_list args;
  char *str;

  va_start (args, error_format);
  str = melo_jsonrpc_build_errorv (id, error_code, error_format, args);
  va_end (args);

  soup_message_set_status (msg, SOUP_STATUS_OK);
  soup_message_set_response (msg, "application/json", SOUP_MEMORY_TAKE,
                             str, strlen (str));
}
