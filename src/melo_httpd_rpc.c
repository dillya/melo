/*
 * melo_httpd_rpc.c: Json-RPC handler for Melo HTTP server
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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "melo_jsonrpc.h"

#include "melo_httpd_rpc.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static void
melo_httpd_rpc_parse_handler (const char *method, GVariant *params,
                              gpointer user_data)
{
}

void
melo_httpd_rpc_handler (SoupServer *server, SoupMessage *msg,
                        const char *path, GHashTable *query,
                        SoupClientContext *client, gpointer user_data)
{
  MeloJSONRPC *parser;
  char *response;

  /* We only support GET and POST methods */
  if (msg->method != SOUP_METHOD_GET && msg->method != SOUP_METHOD_POST) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }

  /* We only accept "/rpc" path */
  if (path[4] != '\0') {
    soup_message_set_status (msg, SOUP_STATUS_BAD_REQUEST);
    return;
  }

  /* Create a new JSON-RPC parser */
  parser = melo_jsonrpc_new ();

  /* Parse request */
  melo_jsonrpc_parse_request (parser,
                              msg->request_body->data,
                              msg->request_body->length,
                              melo_httpd_rpc_parse_handler, NULL);

  /* Build response */
  response = melo_jsonrpc_get_response (parser);

  /* Free parser */
  g_object_unref (parser);

  /* Set response */
  soup_message_set_status (msg, SOUP_STATUS_OK);
  soup_message_set_response (msg, "application/json", SOUP_MEMORY_TAKE,
                             response, strlen (response));
}
