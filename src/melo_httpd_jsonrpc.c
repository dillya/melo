/*
 * melo_httpd_jsonrpc.c: JSON-RPC 2.0 handler for Melo HTTP server
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

#include "melo_httpd_jsonrpc.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

void
melo_httpd_jsonrpc_handler (SoupServer *server, SoupMessage *msg,
                            const char *path, GHashTable *query,
                            SoupClientContext *client, gpointer user_data)
{
  GError *err = NULL;
  char *res;

  /* We only support POST method */
  if (msg->method != SOUP_METHOD_POST) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }

  /* We only accept "/rpc" path */
  if (path[4] != '\0') {
    soup_message_set_status (msg, SOUP_STATUS_BAD_REQUEST);
    return;
  }

  /* Parse request */
  res = melo_jsonrpc_parse_request (msg->request_body->data,
                                    msg->request_body->length,
                                    &err);

  /* Set response status */
  soup_message_set_status (msg, SOUP_STATUS_OK);
  if (!res)
    return;

  /* Set response */
  soup_message_set_response (msg, "application/json", SOUP_MEMORY_TAKE,
                             res, strlen (res));
}
