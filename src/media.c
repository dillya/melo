/*
 * Copyright (C) 2020 Alexandre Dilly <dillya@sparod.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#include <melo/melo_browser.h>

#define MELO_LOG_TAG "melo_media"
#include <melo/melo_log.h>

#include "media.h"

static bool
media_async_cb (MeloMessage *msg, void *user_data)
{
  MeloHttpServerConnection *connection = user_data;
  const unsigned char *data;
  size_t size;

  /* Get message data */
  data = melo_message_get_cdata (msg, &size);

  /* Send message or close connection */
  if (data && size) {
    melo_message_ref (msg);
    melo_http_server_connection_send (
        connection, 200, data, size, (GDestroyNotify) melo_message_unref, msg);
  } else
    melo_http_server_connection_close (connection);

  return true;
}

static void
media_chunk_cb (
    MeloHttpServerConnection *connection, GBytes *chunk, void *user_data)
{
  MeloRequest *req = user_data;

  /* Write next chunk of data */
  if (!req || !melo_browser_put_media_chunk (req, chunk))
    melo_http_server_connection_close (connection);

  /* Release chunk */
  if (!req)
    g_bytes_unref (chunk);
}

/**
 * media_header_cb:
 * @server: the #MeloHttpServer
 * @connection: the #MeloHttpServerConnection
 * @path: the path of the request
 * @user_data: data pointer associated to the callback
 *
 * A new HTTP request has been received and headers are ready to be parsed.
 */
void
media_header_cb (MeloHttpServer *server, MeloHttpServerConnection *connection,
    const char *path, void *user_data)
{
  MeloRequest *req;
  const char *p;
  ssize_t len;
  char *id;

  /* Invalid root path */
  if (strncmp (path, "/media/", 7)) {
    melo_http_server_connection_close (connection);
    return;
  }
  path += 7;

  /* Support only POST */
  if (melo_http_server_connection_get_method (connection) !=
      MELO_HTTP_SERVER_METHOD_PUT) {
    melo_http_server_connection_close (connection);
    return;
  }

  /* Get browser ID */
  p = strchr (path, '/');
  if (!p) {
    melo_http_server_connection_close (connection);
    return;
  }
  id = strndup (path, p - path);
  path = p;

  /* Get content length */
  len = melo_http_server_connection_get_content_length (connection);

  /* Create media request */
  req = melo_browser_put_media (id, path, len, media_async_cb, connection);
  if (req) {
    /* Capture request body */
    melo_http_server_connection_capture_body (connection, media_chunk_cb, req);

    /* Save request */
    melo_http_server_connection_set_user_data (connection, req);
  }
  g_free (id);
}

/**
 * media_body_cb:
 * @server: the #MeloHttpServer
 * @connection: the #MeloHttpServerConnection
 * @path: the path of the request
 * @user_data: data pointer associated to the callback
 *
 * A new HTTP request has been received and body is fully received.
 */
void
media_body_cb (MeloHttpServer *server, MeloHttpServerConnection *connection,
    const char *path, void *user_data)
{
  MeloRequest *req = melo_http_server_connection_get_user_data (connection);

  /* Complete body reception */
  if (!req || !melo_browser_put_media_chunk (req, NULL))
    melo_http_server_connection_close (connection);
}

/**
 * media_close_cb:
 * @server: the #MeloHttpServer
 * @connection: the #MeloHttpServerConnection
 * @user_data: data pointer associated to the callback
 *
 * The HTTP request is finished and connection is closing.
 */
void
media_close_cb (MeloHttpServer *server, MeloHttpServerConnection *connection,
    void *user_data)
{
  MeloRequest *req = melo_http_server_connection_get_user_data (connection);

  /* Finish and cancel request */
  melo_browser_put_media_chunk (req, NULL);
  melo_request_cancel (req);
}
