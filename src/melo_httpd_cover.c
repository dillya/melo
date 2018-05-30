/*
 * melo_httpd_cover.c: MeloTags Cover handler for Melo HTTP server
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

#include "melo_tags.h"

#include "melo_httpd_cover.h"

void
melo_httpd_cover_thread_handler (gpointer data, gpointer user_data)
{
  SoupServer *server = SOUP_SERVER (user_data);
  SoupMessage *msg = SOUP_MESSAGE (data);
  SoupBuffer *buffer;
  SoupURI *uri;
  GBytes *cover = NULL;
  const char *cover_data;
  const gchar *url;
  gsize size;

  /* Get URL from request */
  uri = soup_message_get_uri (msg);
  url = soup_uri_get_path (uri);

  /* Check URL and move to ID */
  if (!url || strncmp (url, "/cover/", 7))
    goto error;
  url += 7;

  /* Get cover data from its URL */
  cover = melo_tags_get_cover_by_id (url);
  if (!cover)
    goto error;

  /* Set response status */
  soup_message_set_status (msg, SOUP_STATUS_OK);

  /* Create a soup buffer */
  cover_data = g_bytes_get_data (cover, &size);
  buffer = soup_buffer_new_with_owner (cover_data, size, cover,
                                       (GDestroyNotify) g_bytes_unref);

  /* Append buffer to message */
  soup_message_body_append_buffer (msg->response_body, buffer);
  soup_buffer_free (buffer);

  /* Set response */
  soup_server_unpause_message (server, msg);
  return;

error:
  soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
  soup_server_unpause_message (server, msg);
}

void
melo_httpd_cover_handler (SoupServer *server, SoupMessage *msg,
                          const char *path, GHashTable *query,
                          SoupClientContext *client, gpointer user_data)
{
  GThreadPool *pool = (GThreadPool *) user_data;

  /* We only support GET method */
  if (msg->method != SOUP_METHOD_GET) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }

  /* Push request to thread pool */
  soup_server_pause_message (server, msg);
  g_thread_pool_push (pool, msg, NULL);
}
